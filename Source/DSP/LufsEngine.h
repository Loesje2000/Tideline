#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

// BS.1770-4 K-weighted loudness engine.
// All measurements written to atomics on the audio thread; read from the UI thread.
class LufsEngine
{
public:
    LufsEngine() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxBlock = maxBlockSize;

        // K-weighting Stage 1: high-shelf (+4 dB, fc = 1681.97 Hz, Q = 0.7072)
        // K-weighting Stage 2: high-pass (fc = 38.135 Hz, Q = 0.5003)
        auto coeffs1 = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 1681.97f, 0.7072f, juce::Decibels::decibelsToGain(4.0f));
        auto coeffs2 = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, 38.135f, 0.5003f);

        for (int ch = 0; ch < 2; ++ch)
        {
            kwStage1[ch].coefficients = coeffs1;
            kwStage2[ch].coefficients = coeffs2;
            kwStage1[ch].reset();
            kwStage2[ch].reset();
        }

        // 10ms sub-block accumulator
        subBlockSamples = juce::jmax(1, juce::roundToInt(sampleRate * 0.01));

        // Ring buffer: 300 sub-blocks = 3 seconds
        msL.assign(300, 0.0f);
        msR.assign(300, 0.0f);

        subHead = 0;
        subFilled = 0;
        totalSubBlocks = 0;
        accL = accR = 0.0;
        accN = 0;

        gateBlocks.clear();
        gateBlocks.reserve(3600); // up to 1 hour
        shortTermHist.clear();
        shortTermHist.reserve(3600);

        truePeak.store(0.0f);
        momentaryLUFS.store(-70.0f);
        shortTermLUFS.store(-70.0f);
        integratedLUFS.store(-70.0f);
        lra.store(0.0f);
        settled.store(false);
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) { kwStage1[ch].reset(); kwStage2[ch].reset(); }
        std::fill(msL.begin(), msL.end(), 0.0f);
        std::fill(msR.begin(), msR.end(), 0.0f);
        subHead = 0; subFilled = 0; totalSubBlocks = 0;
        accL = accR = 0.0; accN = 0;
        gateBlocks.clear();
        shortTermHist.clear();
        truePeak.store(0.0f);
        momentaryLUFS.store(-70.0f);
        shortTermLUFS.store(-70.0f);
        integratedLUFS.store(-70.0f);
        lra.store(0.0f);
        settled.store(false);
    }

    void process(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);
        if (numSamples == 0 || numCh == 0) return;

        const float* srcL = buffer.getReadPointer(0);
        const float* srcR = (numCh > 1) ? buffer.getReadPointer(1) : srcL;

        // True peak (4x interpolation)
        {
            float tp = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                tp = juce::jmax(tp, std::abs(srcL[i]), std::abs(srcR[i]));
                if (i < numSamples - 1)
                {
                    // 3 intermediate interpolated samples
                    for (int k = 1; k < 4; ++k)
                    {
                        float t = k * 0.25f;
                        tp = juce::jmax(tp,
                            std::abs(srcL[i] + t * (srcL[i+1] - srcL[i])),
                            std::abs(srcR[i] + t * (srcR[i+1] - srcR[i])));
                    }
                }
            }
            // Slow decay hold
            float prev = truePeak.load(std::memory_order_relaxed);
            truePeak.store(juce::jmax(prev * 0.99999f, tp), std::memory_order_relaxed);
        }

        // K-weight and accumulate into 10ms sub-blocks
        for (int i = 0; i < numSamples; ++i)
        {
            float kL = kwStage2[0].processSample(kwStage1[0].processSample(srcL[i]));
            float kR = kwStage2[1].processSample(kwStage1[1].processSample(srcR[i]));
            accL += (double)(kL * kL);
            accR += (double)(kR * kR);
            accN++;

            if (accN >= subBlockSamples)
                flushSubBlock();
        }

        updateOutputs();
    }

    float getMomentaryLUFS()  const { return momentaryLUFS.load(std::memory_order_relaxed); }
    float getShortTermLUFS()  const { return shortTermLUFS.load(std::memory_order_relaxed); }
    float getIntegratedLUFS() const { return integratedLUFS.load(std::memory_order_relaxed); }
    float getTruePeakDB()     const
    {
        float tp = truePeak.load(std::memory_order_relaxed);
        return (tp > 1e-9f) ? juce::Decibels::gainToDecibels(tp) : -144.0f;
    }
    float getLRA()            const { return lra.load(std::memory_order_relaxed); }
    bool  isSettled()         const { return settled.load(std::memory_order_relaxed); }

private:
    double sr = 48000.0;
    int    maxBlock = 512;
    int    subBlockSamples = 480;

    juce::dsp::IIR::Filter<float> kwStage1[2];
    juce::dsp::IIR::Filter<float> kwStage2[2];

    // Sub-block ring buffer (10ms each, 300 = 3 seconds)
    std::vector<float> msL, msR;
    int    subHead = 0;
    int    subFilled = 0;
    int    totalSubBlocks = 0;
    double accL = 0.0, accR = 0.0;
    int    accN = 0;

    // Gate unit (100ms) mean-square history for integrated LUFS
    std::vector<float> gateBlocks;

    // Short-term LUFS history for LRA
    std::vector<float> shortTermHist;

    std::atomic<float> truePeak       { 0.0f };
    std::atomic<float> momentaryLUFS  { -70.0f };
    std::atomic<float> shortTermLUFS  { -70.0f };
    std::atomic<float> integratedLUFS { -70.0f };
    std::atomic<float> lra            { 0.0f };
    std::atomic<bool>  settled        { false };

    static constexpr float K_OFFSET    = -0.691f;
    static constexpr float ABS_GATE    = -70.0f;
    static constexpr float REL_GATE_LU = -10.0f;

    void flushSubBlock()
    {
        if (msL.empty()) { accL = accR = 0.0; accN = 0; return; }

        float meanL = (accN > 0) ? (float)(accL / accN) : 0.0f;
        float meanR = (accN > 0) ? (float)(accR / accN) : 0.0f;

        msL[subHead] = meanL;
        msR[subHead] = meanR;
        subHead = (subHead + 1) % 300;
        if (subFilled < 300) subFilled++;
        totalSubBlocks++;

        accL = accR = 0.0;
        accN = 0;

        // Every 10 sub-blocks (100ms): record a gate unit
        if (totalSubBlocks % 10 == 0)
        {
            // Mean of the last 10 sub-blocks across L+R
            double sum = 0.0;
            const int N = (int)msL.size();
            int count = juce::jmin(subFilled, 10);
            for (int i = 0; i < count; ++i)
            {
                int idx = ((subHead - 1 - i) + N) % N;
                sum += msL[idx] + msR[idx];
            }
            float ms100ms = (count > 0) ? (float)(sum * 0.5 / count) : 0.0f;
            gateBlocks.push_back(ms100ms);

            // Cap history at ~1 hour
            if ((int)gateBlocks.size() > 36000)
                gateBlocks.erase(gateBlocks.begin(), gateBlocks.begin() + 1000);
        }
    }

    void updateOutputs()
    {
        if (subFilled == 0) return;
        const int N = 300;

        // Momentary (400ms = 40 sub-blocks)
        {
            int count = juce::jmin(subFilled, 40);
            double sumL = 0.0, sumR = 0.0;
            for (int i = 0; i < count; ++i)
            {
                int idx = ((subHead - 1 - i) + N) % N;
                sumL += msL[idx];
                sumR += msR[idx];
            }
            float ms = (float)((sumL + sumR) * 0.5 / count);
            float lufs = (ms > 1e-10f) ? K_OFFSET + 10.0f * std::log10(ms) : -70.0f;
            momentaryLUFS.store(juce::jmax(lufs, -70.0f), std::memory_order_relaxed);
        }

        // Short-term (3s = 300 sub-blocks)
        {
            int count = juce::jmin(subFilled, 300);
            double sumL = 0.0, sumR = 0.0;
            for (int i = 0; i < count; ++i)
            {
                int idx = ((subHead - 1 - i) + N) % N;
                sumL += msL[idx];
                sumR += msR[idx];
            }
            float ms = (float)((sumL + sumR) * 0.5 / count);
            float stLufs = (ms > 1e-10f) ? K_OFFSET + 10.0f * std::log10(ms) : -70.0f;
            stLufs = juce::jmax(stLufs, -70.0f);
            shortTermLUFS.store(stLufs, std::memory_order_relaxed);

            // Record for LRA (only once per 10 sub-blocks)
            if (totalSubBlocks % 10 == 0)
            {
                shortTermHist.push_back(stLufs);
                if ((int)shortTermHist.size() > 3600)
                    shortTermHist.erase(shortTermHist.begin(), shortTermHist.begin() + 100);
                updateLRA();
            }
        }

        // Integrated LUFS
        if (!gateBlocks.empty() && totalSubBlocks % 10 == 0)
            updateIntegrated();

        // Settle: integrated is within 0.5 LU of short-term and we have > 3s of data
        if (totalSubBlocks > 300)
        {
            float ig = integratedLUFS.load(std::memory_order_relaxed);
            float st = shortTermLUFS.load(std::memory_order_relaxed);
            settled.store(ig > -69.0f && std::abs(ig - st) < 0.5f, std::memory_order_relaxed);
        }
    }

    void updateIntegrated()
    {
        // Absolute gate: -70 LUFS
        float absGateMS = std::pow(10.0f, (ABS_GATE - K_OFFSET) / 10.0f);

        std::vector<float> pass;
        pass.reserve(gateBlocks.size());
        for (float ms : gateBlocks)
            if (ms > absGateMS) pass.push_back(ms);

        if (pass.empty()) { integratedLUFS.store(-70.0f, std::memory_order_relaxed); return; }

        double sum = 0.0;
        for (float ms : pass) sum += ms;
        float ungatedMS = (float)(sum / pass.size());
        float ungatedLUFS = K_OFFSET + 10.0f * std::log10(ungatedMS);

        float relGateMS = std::pow(10.0f, (ungatedLUFS + REL_GATE_LU - K_OFFSET) / 10.0f);

        double gatedSum = 0.0;
        int gatedCount = 0;
        for (float ms : pass)
        {
            if (ms > relGateMS) { gatedSum += ms; gatedCount++; }
        }

        if (gatedCount == 0) { integratedLUFS.store(-70.0f, std::memory_order_relaxed); return; }

        float ig = K_OFFSET + 10.0f * std::log10((float)(gatedSum / gatedCount));
        integratedLUFS.store(juce::jmax(ig, -70.0f), std::memory_order_relaxed);
    }

    void updateLRA()
    {
        if ((int)shortTermHist.size() < 10) return;

        std::vector<float> vals = shortTermHist;
        std::sort(vals.begin(), vals.end());

        // Filter out below absolute gate
        auto it = std::lower_bound(vals.begin(), vals.end(), -70.0f);
        vals.erase(vals.begin(), it);
        if ((int)vals.size() < 5) return;

        int lo = juce::jmax(0, (int)(vals.size() * 0.10f));
        int hi = juce::jmin((int)vals.size() - 1, (int)(vals.size() * 0.95f));
        lra.store(juce::jmax(0.0f, vals[hi] - vals[lo]), std::memory_order_relaxed);
    }
};
