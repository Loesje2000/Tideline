#include "PluginProcessor.h"
#include "PluginEditor.h"

TidelineAudioProcessor::TidelineAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "TidelineState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
TidelineAudioProcessor::createParameterLayout()
{
    using PID = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PID{"mode", 1}, "Mode",
        juce::StringArray{"Streaming", "Vinyl"}, 0));

    // Order must match TideDial platform list:
    // 0=Spotify Normal, 1=Apple Music, 2=YouTube Video, 3=TIDAL,
    // 4=Spotify Quiet, 5=Spotify Loud, 6=YouTube Music
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PID{"active_platform", 1}, "Active Platform",
        juce::StringArray{
            "Spotify Normal", "Apple Music", "YouTube Video", "TIDAL",
            "Spotify Quiet", "Spotify Loud", "YouTube Music"
        }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        PID{"preview", 1}, "Preview", false));

    return { params.begin(), params.end() };
}

void TidelineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lufsEngine.prepare(sampleRate, samplesPerBlock);
    vinylAdvisor.prepare(sampleRate);
    filePlayer.prepare(sampleRate, samplesPerBlock);

    rmsAlpha = 1.0f - std::exp(-1.0f / (float)(sampleRate * 2.0));
    rmsSmooth = 0.0f;
    previewGainSmoothed = 1.0f;
}

void TidelineAudioProcessor::releaseResources()
{
    filePlayer.releaseResources();
}

void TidelineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Handle reset request
    if (resetRequested.load(std::memory_order_relaxed))
    {
        lufsEngine.reset();
        vinylAdvisor.reset();
        rmsSmooth = 0.0f;
        resetRequested.store(false, std::memory_order_relaxed);
    }

    // If a file is loaded, replace input buffer with file audio
    if (filePlayer.hasFile())
        filePlayer.process(buffer);

    // Compute RMS for crest factor (before any gain adjustment)
    {
        float sumSq = 0.0f;
        int count = 0;
        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), 2); ++ch)
        {
            auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                sumSq += data[i] * data[i];
            count += buffer.getNumSamples();
        }
        float rms = (count > 0) ? std::sqrt(sumSq / count) : 0.0f;
        rmsSmooth += rmsAlpha * (rms - rmsSmooth);
    }

    // Run LUFS and vinyl measurements
    lufsEngine.process(buffer);
    vinylAdvisor.process(buffer, rmsSmooth);

    // Preview gain-match output
    applyPreviewGain(buffer);
}

float TidelineAudioProcessor::getCrestFactorDB() const
{
    float peak = lufsEngine.getTruePeakDB();
    float rmsDB = (rmsSmooth > 1e-9f) ? juce::Decibels::gainToDecibels(rmsSmooth) : -144.0f;
    return peak - rmsDB;
}

void TidelineAudioProcessor::applyPreviewGain(juce::AudioBuffer<float>& buffer)
{
    auto* previewParam   = apvts.getRawParameterValue("preview");
    auto* platformParam  = apvts.getRawParameterValue("active_platform");

    if (previewParam == nullptr || platformParam == nullptr) return;

    bool previewOn = (*previewParam > 0.5f);
    if (!previewOn)
    {
        // Ramp gain back to unity
        previewGainSmoothed += (1.0f - previewGainSmoothed) * 0.1f;
        if (std::abs(previewGainSmoothed - 1.0f) < 0.001f) previewGainSmoothed = 1.0f;
        if (previewGainSmoothed != 1.0f)
            buffer.applyGainRamp(0, buffer.getNumSamples(), previewGainSmoothed, previewGainSmoothed);
        return;
    }

    float integrated = lufsEngine.getIntegratedLUFS();
    if (integrated <= -69.0f) return; // no measurement yet

    // Platform target LUFS values — order matches AudioParameterChoice above
    // 0=Spotify Normal, 1=Apple Music, 2=YouTube Video, 3=TIDAL,
    // 4=Spotify Quiet, 5=Spotify Loud, 6=YouTube Music
    const float targets[] = { -14.0f, -16.0f, -14.0f, -14.0f, -19.0f, -11.0f, -7.0f };
    int idx = juce::jlimit(0, 6, juce::roundToInt(platformParam->load()));
    float targetLufs = targets[idx];

    float gainDB = targetLufs - integrated;
    float targetGain = juce::Decibels::decibelsToGain(juce::jlimit(-24.0f, 24.0f, gainDB));

    // Smooth the gain change
    float prevGain = previewGainSmoothed;
    previewGainSmoothed += (targetGain - previewGainSmoothed) * 0.05f;
    buffer.applyGainRamp(0, buffer.getNumSamples(), prevGain, previewGainSmoothed);
}

void TidelineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TidelineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* TidelineAudioProcessor::createEditor()
{
    return new TidelineAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TidelineAudioProcessor();
}
