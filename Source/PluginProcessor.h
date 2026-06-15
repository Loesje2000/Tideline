#pragma once
#include <JuceHeader.h>
#include "DSP/LufsEngine.h"
#include "DSP/VinylAdvisor.h"
#include "DSP/FilePlayerEngine.h"

class TidelineAudioProcessor : public juce::AudioProcessor
{
public:
    TidelineAudioProcessor();
    ~TidelineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // DSP modules — read from UI thread (all output values are atomic)
    LufsEngine       lufsEngine;
    VinylAdvisor     vinylAdvisor;
    FilePlayerEngine filePlayer;

    // Crest factor (derived from peak + smoothed RMS)
    float getCrestFactorDB() const;

    void requestReset() { resetRequested.store(true, std::memory_order_relaxed); }

private:
    std::atomic<bool> resetRequested { false };

    // RMS smoothing for crest factor
    float rmsSmooth = 0.0f;
    float rmsAlpha  = 0.0f;

    // Preview gain
    float previewGainSmoothed = 1.0f;

    void applyPreviewGain(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TidelineAudioProcessor)
};
