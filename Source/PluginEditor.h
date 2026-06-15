#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/TidelineLookAndFeel.h"
#include "GUI/TideDial.h"
#include "GUI/VinylPanel.h"

class TidelineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer,
                                      public juce::FileDragAndDropTarget
{
public:
    explicit TidelineAudioProcessorEditor(TidelineAudioProcessor&);
    ~TidelineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    TidelineAudioProcessor& processor;
    TidelineLookAndFeel     laf;

    using APVTS = juce::AudioProcessorValueTreeState;

    TideDial          dial;
    VinylPanel        vinylPanel;

    juce::TextButton  streamingBtn { "Streaming" };
    juce::TextButton  vinylBtn     { "Vinyl" };
    juce::TextButton  previewBtn   { "Preview" };
    juce::TextButton  resetBtn     { "Reset" };

    // File player transport bar
    juce::TextButton  playStopBtn  { "Play" };
    juce::TextButton  ejectBtn     { "Eject" };
    juce::Label       fileNameLabel;
    juce::Slider      progressSlider;

    std::unique_ptr<APVTS::ButtonAttachment> previewAttach;

    bool vinylMode    = false;
    bool fileDragOver = false;

    float lastPreviewGainDB = 0.0f;
    float gainCueAlpha = 0.0f;

    void drawBackground(juce::Graphics& g);
    void drawDropOverlay(juce::Graphics& g);
    void drawFileBar(juce::Graphics& g);
    void updateModeButtons();
    void updateFileTransportUI();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TidelineAudioProcessorEditor)
};
