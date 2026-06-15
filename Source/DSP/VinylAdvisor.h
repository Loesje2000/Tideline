#pragma once
#include <JuceHeader.h>

// Measures bass mono-correlation and HF stereo width for vinyl cut advisory.
class VinylAdvisor
{
public:
    VinylAdvisor() = default;

    void prepare(double sampleRate)
    {
        sr = sampleRate;

        // Low-pass ~150 Hz for mono-bass correlation
        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 150.0f, 0.707f);
        for (int ch = 0; ch < 2; ++ch)
        {
            bassLP[ch].coefficients = lpCoeffs;
            bassLP[ch].reset();
        }

        // High-pass ~5 kHz for HF width
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 5000.0f, 0.707f);
        for (int ch = 0; ch < 2; ++ch)
        {
            hfHP[ch].coefficients = hpCoeffs;
            hfHP[ch].reset();
        }

        const double tSmooth = 3.0; // 3s running window via one-pole IIR
        alpha = 1.0f - std::exp(-1.0 / (sampleRate * tSmooth));

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) { bassLP[ch].reset(); hfHP[ch].reset(); }
        bassEnergyL = bassEnergyR = 0.0f;
        bassCorr = 0.0f;
        hfCorrSmooth = 0.5f;
        monoScore.store(1.0f);
        hfWidthScore.store(0.5f);
        rmsSmooth = 0.0f;
    }

    void process(const juce::AudioBuffer<float>& buffer, float rmsIn)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);
        if (numSamples == 0 || numCh == 0) return;

        const float* srcL = buffer.getReadPointer(0);
        const float* srcR = (numCh > 1) ? buffer.getReadPointer(1) : srcL;

        for (int i = 0; i < numSamples; ++i)
        {
            float bL = bassLP[0].processSample(srcL[i]);
            float bR = bassLP[1].processSample(srcR[i]);

            // Running mean-square and cross-product for bass band
            bassEnergyL += alpha * (bL * bL - bassEnergyL);
            bassEnergyR += alpha * (bR * bR - bassEnergyR);
            bassCorr    += alpha * (bL * bR - bassCorr);

            float hL = hfHP[0].processSample(srcL[i]);
            float hR = hfHP[1].processSample(srcR[i]);

            // HF correlation
            float hfEL = hL * hL, hfER = hR * hR, hfCross = hL * hR;
            float hfDenom = (hfEL + hfER) * 0.5f;
            float hfC = (hfDenom > 1e-10f) ? (hfCross / hfDenom) : 1.0f;
            hfCorrSmooth += alpha * (hfC - hfCorrSmooth);
        }

        // Mono-bass correlation coefficient: bassCorr / sqrt(bassEnergyL * bassEnergyR)
        float denom = std::sqrt(bassEnergyL * bassEnergyR);
        float corr = (denom > 1e-10f) ? (bassCorr / denom) : 1.0f;
        corr = juce::jlimit(-1.0f, 1.0f, corr);
        monoScore.store((corr + 1.0f) * 0.5f); // map [-1,1] to [0,1]

        // HF width: 0 = mono, 1 = fully out of phase (wide)
        float hfCorr = juce::jlimit(-1.0f, 1.0f, hfCorrSmooth);
        hfWidthScore.store(1.0f - (hfCorr + 1.0f) * 0.5f);

        // Smooth RMS for crest factor derivation
        rmsSmooth += alpha * (rmsIn - rmsSmooth);
    }

    // 0 = fully out of phase/wide, 1 = perfectly mono below 150 Hz
    float getMonoScore()    const { return monoScore.load(std::memory_order_relaxed); }
    // 0 = mono HF, 1 = very wide HF
    float getHFWidthScore() const { return hfWidthScore.load(std::memory_order_relaxed); }
    float getRMSSmooth()    const { return rmsSmooth; }

private:
    double sr = 48000.0;
    float  alpha = 0.0003f;

    juce::dsp::IIR::Filter<float> bassLP[2];
    juce::dsp::IIR::Filter<float> hfHP[2];

    float bassEnergyL = 0.0f, bassEnergyR = 0.0f;
    float bassCorr = 0.0f;
    float hfCorrSmooth = 0.5f;
    float rmsSmooth = 0.0f;

    std::atomic<float> monoScore   { 1.0f };
    std::atomic<float> hfWidthScore{ 0.5f };
};
