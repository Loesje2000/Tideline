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

    addAndMakeVisible(previousBtn);
    previousBtn.onClick = [this]
    {
        if (processor.filePlayer.previousFile()) { processor.requestReset(); processor.filePlayer.play(); }
    };
    addAndMakeVisible(nextBtn);
    nextBtn.onClick = [this]
    {
        if (processor.filePlayer.nextFile()) { processor.requestReset(); processor.filePlayer.play(); }
    };

    addAndMakeVisible(analyseAlbumBtn);
    analyseAlbumBtn.onClick = [this] { processor.filePlayer.analyseAlbumAsync(); };

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
    if (processor.filePlayer.isAnalysingAlbum() || !processor.filePlayer.getAlbumResults().empty()) repaint();

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
    previousBtn.setVisible(hasFile && fp.getPlaylistSize() > 1);
    nextBtn.setVisible(hasFile && fp.getPlaylistSize() > 1);
    analyseAlbumBtn.setVisible(hasFile && fp.getPlaylistSize() > 1);
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
    juce::String name = fp.getFileName();
    if (fp.getPlaylistSize() > 1)
        name = juce::String(fp.getPlaylistIndex() + 1) + "/" + juce::String(fp.getPlaylistSize()) + "  " + name;
    fileNameLabel.setText(name, juce::dontSendNotification);
    previousBtn.setEnabled(fp.hasPreviousFile());
    nextBtn.setEnabled(fp.hasNextFile());
    analyseAlbumBtn.setEnabled(!fp.isAnalysingAlbum());
    analyseAlbumBtn.setButtonText(fp.isAnalysingAlbum() ? "Analysing..." : "Analyse Album");

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

    if (!vinylMode)
    {
        auto overview = juce::Rectangle<float>(104.0f, 45.0f, (float)getWidth() - 208.0f, 92.0f);
        auto block = juce::Rectangle<float>((float)getWidth() * 0.22f, 148.0f,
                                             (float)getWidth() * 0.56f, 170.0f);
        drawStreamingOverview(g, overview);
        drawLoudnessBlock(g, block);
        drawAlbumSummary(g);
    }

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

void TidelineAudioProcessorEditor::drawStreamingOverview(juce::Graphics& g,
                                                           juce::Rectangle<float> bounds)
{
    static const juce::StringArray labels {
        "Spotify\nNormal", "Apple\nMusic", "YouTube\nVideo", "TIDAL", "Spotify\nQuiet",
        "Spotify\nLoud", "YouTube\nMusic", "Amazon\nMusic", "Pandora", "Deezer"
    };

    const auto* platform = processor.apvts.getRawParameterValue("active_platform");
    const int active = platform != nullptr ? juce::roundToInt(platform->load()) : 0;
    const bool waiting = processor.lufsEngine.getIntegratedLUFS() <= -69.0f;
    const float gap = 5.0f;
    const float cellW = (bounds.getWidth() - 4.0f * gap) / 5.0f;
    const float cellH = (bounds.getHeight() - gap) * 0.5f;

    for (int i = 0; i < labels.size(); ++i)
    {
        const int row = i / 5, col = i % 5;
        auto cell = juce::Rectangle<float>(bounds.getX() + col * (cellW + gap),
                                           bounds.getY() + row * (cellH + gap), cellW, cellH);
        const bool selected = i == active;
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_PANEL).withAlpha(selected ? 1.0f : 0.78f));
        g.fillRoundedRectangle(cell, 4.0f);
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(selected ? 0.90f : 0.16f));
        g.drawRoundedRectangle(cell.reduced(0.5f), 4.0f, selected ? 1.25f : 0.6f);

        const float score = processor.getPlatformGainDB(i);
        g.setColour(score < -0.3f ? juce::Colour(0xffe78354) : juce::Colour(TidelineLookAndFeel::COL_AMBER));
        g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        const auto scoreText = waiting ? "---" : juce::String(score >= 0.0f ? "+" : "") + juce::String(score, 1);
        g.drawText(scoreText, cell.withTrimmedTop(3.0f).withHeight(17.0f), juce::Justification::centred, false);
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.62f));
        g.setFont(juce::FontOptions(7.7f, juce::Font::plain));
        g.drawText(labels[i], cell.withTrimmedTop(21.0f).reduced(2.0f, 0.0f), juce::Justification::centred, true);
    }
}

void TidelineAudioProcessorEditor::drawLoudnessBlock(juce::Graphics& g,
                                                      juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_PANEL));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.38f));
    g.setFont(juce::FontOptions(8.5f, juce::Font::plain));
    const bool preview = previewBtn.getToggleState();
    g.drawText(preview ? "NORMALIZED BLOCK" : "SOURCE BLOCK", bounds.reduced(9.0f).removeFromTop(14.0f),
               juce::Justification::centredLeft, false);

    const auto* platform = processor.apvts.getRawParameterValue("active_platform");
    const float gain = (preview && platform != nullptr)
        ? processor.getPlatformGainDB(juce::roundToInt(platform->load())) : 0.0f;
    const float integrated = processor.lufsEngine.getIntegratedLUFS();
    if (integrated <= -69.0f) return;

    const float peak = processor.lufsEngine.getTruePeakDB() + gain;
    const float lra = processor.lufsEngine.getLRA();
    const float loudness = integrated + gain;
    const auto yFor = [&] (float db) { return juce::jmap(juce::jlimit(-36.0f, 0.0f, db), -36.0f, 0.0f,
                                                           bounds.getBottom() - 12.0f, bounds.getY() + 23.0f); };
    const float peakY = yFor(peak), loudY = yFor(loudness), rangeY = yFor(loudness + lra * 0.5f);
    auto column = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.25f, bounds.getHeight() - 34.0f);
    column.setY(bounds.getY() + 24.0f);
    g.setColour(juce::Colour(0xffeee8d5).withAlpha(0.32f));
    g.fillRoundedRectangle(column.getX(), rangeY, column.getWidth(), juce::jmax(4.0f, loudY - rangeY), 3.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER));
    g.drawHorizontalLine(juce::roundToInt(peakY), column.getX(), column.getRight());
    g.setColour(juce::Colour(0xffd85a80));
    g.drawHorizontalLine(juce::roundToInt(loudY), column.getX(), column.getRight());

    g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.72f));
    g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
    g.drawText("TP  " + juce::String(peak, 1), bounds.reduced(10.0f).withY(peakY - 8.0f).withHeight(16.0f), juce::Justification::centredLeft, false);
    g.drawText("PLR " + juce::String(peak - loudness, 1) + "   LRA " + juce::String(lra, 1),
               bounds.reduced(10.0f).withY(loudY - 8.0f).withHeight(16.0f), juce::Justification::centredLeft, false);
    if (preview && std::abs(gain) > 0.05f)
    {
        g.setColour(juce::Colour(0xffd85a80));
        g.drawText(juce::String(gain >= 0.0f ? "+" : "") + juce::String(gain, 1) + " dB",
                   bounds.reduced(10.0f).withY(bounds.getBottom() - 22.0f).withHeight(14.0f), juce::Justification::centredRight, false);
    }
}

void TidelineAudioProcessorEditor::drawAlbumSummary(juce::Graphics& g)
{
    const auto results = processor.filePlayer.getAlbumResults();
    const bool analysing = processor.filePlayer.isAnalysingAlbum();
    if (results.empty() && !analysing) return;

    auto box = juce::Rectangle<float>(104.0f, 323.0f, (float)getWidth() - 208.0f, 34.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_PANEL).withAlpha(0.85f));
    g.fillRoundedRectangle(box, 4.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.68f));
    g.setFont(juce::FontOptions(8.8f, juce::Font::plain));
    if (analysing)
    {
        g.drawText("Analysing album  " + juce::String((int)results.size()) + "/" + juce::String(processor.filePlayer.getPlaylistSize()),
                   box.reduced(8.0f), juce::Justification::centredLeft, false);
        return;
    }

    g.drawText("Album complete: " + juce::String((int)results.size()) + " tracks", box.reduced(8.0f),
               juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER));
    const float tidalGain = processor.filePlayer.getTidalAlbumGainDB();
    g.drawText("TIDAL album gain " + juce::String(tidalGain, 1) + " dB", box.reduced(8.0f),
               juce::Justification::centredRight, false);
}

void TidelineAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (vinylMode) return;

    const auto area = juce::Rectangle<float>(104.0f, 45.0f, (float)getWidth() - 208.0f, 92.0f);
    if (!area.contains(e.position)) return;

    const float gap = 5.0f;
    const float cellW = (area.getWidth() - 4.0f * gap) / 5.0f;
    const float cellH = (area.getHeight() - gap) / 2.0f;
    const int col = juce::jlimit(0, 4, (int)((e.position.x - area.getX()) / (cellW + gap)));
    const int row = juce::jlimit(0, 1, (int)((e.position.y - area.getY()) / (cellH + gap)));
    const int platform = row * 5 + col;
    if (auto* param = processor.apvts.getParameter("active_platform"))
        param->setValueNotifyingHost(param->convertTo0to1((float)platform));
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
        previousBtn.setBounds(54, barY + 3, 22, 22);
        nextBtn.setBounds(78, barY + 3, 22, 22);
        ejectBtn.setBounds(104, barY + 3, 36, 22);
        analyseAlbumBtn.setBounds(146, barY + 3, 88, 22);
        fileNameLabel.setBounds(240, barY + 5, 160, 18);
        progressSlider.setBounds(404, barY + 4, W - 414, 20);
    }
}

bool TidelineAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.isDirectory()) return true;
        juce::String ext = file.getFileExtension().toLowerCase();
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

    juce::Array<juce::File> dropped;
    for (const auto& f : files) dropped.add(juce::File(f));
    if (processor.filePlayer.loadFiles(dropped))
    {
        processor.requestReset();
        processor.filePlayer.play();
        resized();
        repaint();
    }
}

void TidelineAudioProcessorEditor::updateModeButtons()
{
    streamingBtn.setToggleState(!vinylMode, juce::dontSendNotification);
    vinylBtn.setToggleState(vinylMode, juce::dontSendNotification);
    dial.setVisible(vinylMode);
    resized();
    repaint();
}
