#include "PluginEditor.h"

TidelineAudioProcessorEditor::TidelineAudioProcessorEditor(TidelineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&laf);
    setSize(700, 450);
    setResizable(true, true);
    setResizeLimits(560, 360, 1100, 700);

    // Dial
    addAndMakeVisible(dial);
    dial.onTickSelected = [this](int idx)
    {
        auto* param = processor.apvts.getParameter("active_platform");
        if (param) param->setValueNotifyingHost(param->convertTo0to1((float)idx));
    };

    // Mode buttons
    addAndMakeVisible(streamingBtn);
    addAndMakeVisible(vinylBtn);
    streamingBtn.setClickingTogglesState(false);
    vinylBtn.setClickingTogglesState(false);
    streamingBtn.onClick = [this]
    {
        vinylMode = false;
        dial.setVinylMode(false);
        vinylPanel.setVisible(false);
        updateModeButtons();
        auto* param = processor.apvts.getParameter("mode");
        if (param) param->setValueNotifyingHost(0.0f);
    };
    vinylBtn.onClick = [this]
    {
        vinylMode = true;
        dial.setVinylMode(true);
        vinylPanel.setVisible(true);
        updateModeButtons();
        auto* param = processor.apvts.getParameter("mode");
        if (param) param->setValueNotifyingHost(1.0f);
    };

    // Preview / Reset
    addAndMakeVisible(previewBtn);
    previewBtn.setClickingTogglesState(true);
    previewAttach = std::make_unique<APVTS::ButtonAttachment>(
        processor.apvts, "preview", previewBtn);

    addAndMakeVisible(resetBtn);
    resetBtn.onClick = [this] { processor.requestReset(); };

    // File transport — play/stop
    addAndMakeVisible(playStopBtn);
    playStopBtn.onClick = [this]
    {
        auto& fp = processor.filePlayer;
        if (fp.isPlaying())
            fp.stop();
        else
        {
            if (fp.isAtEnd()) fp.rewind();
            fp.play();
        }
    };

    // Eject
    addAndMakeVisible(ejectBtn);
    ejectBtn.onClick = [this]
    {
        processor.filePlayer.stop();
        processor.requestReset();
        // Reload with no file — just stops playback
        // (FilePlayerEngine holds state; clearing requires new loadFile)
    };

    // File name label
    addAndMakeVisible(fileNameLabel);
    fileNameLabel.setColour(juce::Label::textColourId,
                            juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.65f));
    fileNameLabel.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);

    // Progress slider (scrubber)
    addAndMakeVisible(progressSlider);
    progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    progressSlider.setRange(0.0, 1.0);
    progressSlider.setColour(juce::Slider::trackColourId,  juce::Colour(0xff3e2912));
    progressSlider.setColour(juce::Slider::thumbColourId,  juce::Colour(TidelineLookAndFeel::COL_AMBER));
    progressSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a0e04));
    progressSlider.onValueChange = [this]
    {
        auto& fp = processor.filePlayer;
        if (fp.hasFile())
            fp.stop(); // seek on drag — simple approach: stop and rewind is better than mid-seek
    };

    // Vinyl panel
    addAndMakeVisible(vinylPanel);
    vinylPanel.setVisible(false);

    updateModeButtons();
    startTimerHz(30);
}

TidelineAudioProcessorEditor::~TidelineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void TidelineAudioProcessorEditor::timerCallback()
{
    dial.setMomentaryLUFS(processor.lufsEngine.getMomentaryLUFS());
    dial.setIntegratedLUFS(processor.lufsEngine.getIntegratedLUFS());
    dial.setSettled(processor.lufsEngine.isSettled());

    if (vinylMode)
    {
        vinylPanel.update(
            processor.vinylAdvisor.getMonoScore(),
            processor.getCrestFactorDB(),
            processor.vinylAdvisor.getHFWidthScore());
    }

    // Update file transport UI
    updateFileTransportUI();

    // Preview gain-change cue
    if (previewBtn.getToggleState())
    {
        float integrated = processor.lufsEngine.getIntegratedLUFS();
        float targetLufs = dial.getSelectedTargetLUFS();
        float newGainDB  = (integrated > -69.0f) ? (targetLufs - integrated) : 0.0f;
        if (std::abs(newGainDB - lastPreviewGainDB) > 6.0f) gainCueAlpha = 1.0f;
        lastPreviewGainDB = newGainDB;
        if (gainCueAlpha > 0.005f) { gainCueAlpha *= 0.92f; repaint(); }
    }
}

void TidelineAudioProcessorEditor::updateFileTransportUI()
{
    auto& fp = processor.filePlayer;
    bool hasFile = fp.hasFile();

    playStopBtn.setVisible(hasFile);
    ejectBtn.setVisible(hasFile);
    progressSlider.setVisible(hasFile);
    fileNameLabel.setVisible(hasFile);

    if (!hasFile)
    {
        playStopBtn.setButtonText("Play");
        fileNameLabel.setText("", juce::dontSendNotification);
        return;
    }

    playStopBtn.setButtonText(fp.isPlaying() ? "Pause" : "Play");
    fileNameLabel.setText(fp.getFileName(), juce::dontSendNotification);

    double len = fp.getLengthSeconds();
    if (len > 0.0)
    {
        double pos = fp.getPositionSeconds() / len;
        progressSlider.setValue(juce::jlimit(0.0, 1.0, pos), juce::dontSendNotification);
    }

    if (fp.isAtEnd())
        playStopBtn.setButtonText("Replay");
}

void TidelineAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawBackground(g);

    // Rubblesonic wordmark
    TidelineLookAndFeel::drawRubblesonicWordmark(
        g, juce::Rectangle<float>(12.0f, 8.0f, 90.0f, 18.0f), -6.0f);

    g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.42f));
    g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    g.drawText("Tideline", juce::Rectangle<float>(12.0f, 26.0f, 90.0f, 13.0f),
               juce::Justification::centredLeft, false);

    // Numeric readouts
    {
        auto drawReadout = [&](float value, const juce::String& label,
                                float x, float y, float w, float h, bool warn = false)
        {
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_PANEL));
            g.fillRoundedRectangle(x, y, w, h, 3.0f);
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.38f));
            g.setFont(juce::FontOptions(8.5f, juce::Font::plain));
            g.drawText(label, juce::Rectangle<float>(x, y + 1.0f, w, 11.0f),
                       juce::Justification::centred, false);
            juce::Colour valCol = warn ? juce::Colour(0xffdd5050) : juce::Colour(TidelineLookAndFeel::COL_AMBER);
            g.setColour(valCol);
            g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            juce::String vs = (value <= -70.0f) ? "---" : juce::String(value, 1);
            g.drawText(vs, juce::Rectangle<float>(x, y + 12.0f, w, 17.0f),
                       juce::Justification::centred, false);
        };

        float mom  = processor.lufsEngine.getMomentaryLUFS();
        float ig   = processor.lufsEngine.getIntegratedLUFS();
        float tp   = processor.lufsEngine.getTruePeakDB();
        float lra  = processor.lufsEngine.getLRA();

        float bY = (float)getHeight() - (processor.filePlayer.hasFile() ? 70.0f : 42.0f);
        float rW = 70.0f, rH = 32.0f, rGap = 8.0f;
        float totalW = 4.0f * rW + 3.0f * rGap;
        float startX = ((float)getWidth() - totalW) * 0.5f;

        drawReadout(mom, "Momentary",  startX,                   bY, rW, rH);
        drawReadout(ig,  "Integrated", startX + (rW + rGap),    bY, rW, rH);
        drawReadout(tp,  "True Peak",  startX + 2*(rW + rGap),  bY, rW, rH, tp > -1.0f);
        drawReadout(lra, "LRA",        startX + 3*(rW + rGap),  bY, rW, rH);
    }

    // File transport bar background
    if (processor.filePlayer.hasFile())
        drawFileBar(g);

    // Drop overlay
    if (fileDragOver)
        drawDropOverlay(g);

    // "Drop a file" hint (only when no file loaded and not dragging)
    if (!processor.filePlayer.hasFile() && !fileDragOver)
    {
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.20f));
        g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
        g.drawText("Drop an audio file to analyze",
                   juce::Rectangle<float>(0.0f, (float)getHeight() - 18.0f,
                                           (float)getWidth(), 14.0f),
                   juce::Justification::centred, false);
    }

    // Preview cue highlight
    if (gainCueAlpha > 0.01f)
    {
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(gainCueAlpha * 0.14f));
        g.fillRoundedRectangle(previewBtn.getX() - 4.0f, previewBtn.getY() - 4.0f,
                                previewBtn.getWidth() + 8.0f, previewBtn.getHeight() + 8.0f, 5.0f);
    }
}

void TidelineAudioProcessorEditor::drawBackground(juce::Graphics& g)
{
    g.fillAll(juce::Colour(TidelineLookAndFeel::COL_BG));
    juce::ColourGradient vig(
        juce::Colour(0x00000000), (float)getWidth() * 0.5f, (float)getHeight() * 0.5f,
        juce::Colour(0x38000000), 0.0f, 0.0f, true);
    g.setGradientFill(vig);
    g.fillRect(getLocalBounds());
}

void TidelineAudioProcessorEditor::drawDropOverlay(juce::Graphics& g)
{
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(0.12f));
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(0.60f));
    g.drawRect(getLocalBounds().reduced(4), 2);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("Drop to analyze", getLocalBounds(), juce::Justification::centred, false);
}

void TidelineAudioProcessorEditor::drawFileBar(juce::Graphics& g)
{
    float barY = (float)getHeight() - 28.0f;
    g.setColour(juce::Colour(0xff1a0e04));
    g.fillRect(juce::Rectangle<float>(0.0f, barY, (float)getWidth(), 28.0f));
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_BTN_RING).withAlpha(0.5f));
    g.drawHorizontalLine(juce::roundToInt(barY), 0.0f, (float)getWidth());
}

void TidelineAudioProcessorEditor::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    bool hasFile = processor.filePlayer.hasFile();
    int  fileBarH = hasFile ? 28 : 0;

    // Main dial area
    int dialW = juce::roundToInt(W * 0.62f);
    int dialH = juce::roundToInt((H - fileBarH) * 0.82f);
    int dialX = (W - dialW) / 2;
    int dialY = juce::roundToInt((H - fileBarH) * 0.04f);
    dial.setBounds(dialX, dialY, dialW, dialH);

    // Mode toggle — top right
    int btnH = 22, btnW = 76;
    streamingBtn.setBounds(W - btnW * 2 - 14, 10, btnW, btnH);
    vinylBtn.setBounds(W - btnW - 10, 10, btnW, btnH);

    // Controls bottom right (above file bar)
    int ctrlY = H - fileBarH - 36;
    previewBtn.setBounds(W - 66, ctrlY, 58, 22);
    resetBtn.setBounds(W - 132, ctrlY, 58, 22);

    // Vinyl panel — right side
    {
        int vpX = dialX + dialW + 6;
        int vpW = W - vpX - 8;
        int vpY = dialY + 8;
        int vpH = dialH - 8;
        vinylPanel.setBounds(vpX, vpY, vpW, vpH);
    }

    // File transport bar — bottom strip
    if (hasFile)
    {
        int barY = H - fileBarH;
        playStopBtn.setBounds(6, barY + 3, 44, 22);
        ejectBtn.setBounds(54, barY + 3, 36, 22);
        fileNameLabel.setBounds(96, barY + 5, 200, 18);
        progressSlider.setBounds(300, barY + 4, W - 310, 20);
    }
}

bool TidelineAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::String ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".mp3"
            || ext == ".flac" || ext == ".ogg" || ext == ".m4a" || ext == ".aac")
            return true;
    }
    return false;
}

void TidelineAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    fileDragOver = true;
    repaint();
}

void TidelineAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    fileDragOver = false;
    repaint();
}

void TidelineAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    fileDragOver = false;

    for (const auto& f : files)
    {
        juce::File file(f);
        if (processor.filePlayer.loadFile(file))
        {
            processor.requestReset();
            processor.filePlayer.play();
            resized(); // re-layout to show the file transport bar
            repaint();
            break;
        }
    }
}

void TidelineAudioProcessorEditor::updateModeButtons()
{
    streamingBtn.setToggleState(!vinylMode, juce::dontSendNotification);
    vinylBtn.setToggleState(vinylMode, juce::dontSendNotification);
    resized();
    repaint();
}
