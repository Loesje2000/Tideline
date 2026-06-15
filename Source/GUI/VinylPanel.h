#pragma once
#include <JuceHeader.h>
#include "TidelineLookAndFeel.h"
#include "../DSP/VinylVerdict.h"

// Vinyl mode panel: mono-bass gauge, crest factor readout, verdict text.
class VinylPanel : public juce::Component
{
public:
    VinylPanel() = default;
    ~VinylPanel() override = default;

    void update(float monoScore, float crestFactorDB, float hfWidthScore)
    {
        currentMono   = monoScore;
        currentCrest  = crestFactorDB;
        currentResult = VinylVerdict::evaluate(monoScore, crestFactorDB, hfWidthScore);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(4.0f);

        // Background
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_PANEL));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_BTN_RING));
        g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);

        float y = b.getY() + 8.0f;
        float x = b.getX() + 10.0f;
        float w = b.getWidth() - 20.0f;

        // Mono-bass gauge
        {
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.6f));
            g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
            g.drawText("Bass mono (below 150 Hz)", juce::Rectangle<float>(x, y, w, 13),
                       juce::Justification::centredLeft, false);
            y += 15.0f;

            float barW = w;
            float barH = 6.0f;
            g.setColour(juce::Colour(0xff3a2810));
            g.fillRoundedRectangle(x, y, barW, barH, 3.0f);

            // Fill: green = mono (right), red = wide (left)
            juce::ColourGradient grad(
                juce::Colour(0xff7abf82), x + barW, y,
                juce::Colour(0xffbf6060), x, y, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(x, y, barW, barH, 3.0f);

            // Marker at current monoScore position (0=wide, 1=mono — maps to left/right)
            float markerX = x + currentMono * barW;
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(markerX - 1.5f, y - 1.5f, 3.0f, barH + 3.0f, 1.5f);

            y += barH + 4.0f;

            // Labels
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.4f));
            g.setFont(juce::FontOptions(9.0f, juce::Font::plain));
            g.drawText("wide", juce::Rectangle<float>(x, y, 30, 11),
                       juce::Justification::centredLeft, false);
            g.drawText("mono", juce::Rectangle<float>(x + barW - 30, y, 30, 11),
                       juce::Justification::centredRight, false);
            y += 14.0f;
        }

        // Divider
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_BTN_RING).withAlpha(0.5f));
        g.drawHorizontalLine(juce::roundToInt(y), x, x + w);
        y += 6.0f;

        // Crest factor
        {
            juce::String crLabel;
            if      (currentCrest > 12.0f) crLabel = "plenty of headroom";
            else if (currentCrest >  6.0f) crLabel = "moderate headroom";
            else                            crLabel = "heavily limited";

            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.6f));
            g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
            g.drawText("Crest factor:", juce::Rectangle<float>(x, y, w * 0.45f, 13),
                       juce::Justification::centredLeft, false);

            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.85f));
            juce::String crestTxt = juce::String(currentCrest, 1) + " dB — " + crLabel;
            g.drawText(crestTxt, juce::Rectangle<float>(x + w * 0.45f, y, w * 0.55f, 13),
                       juce::Justification::centredLeft, false);
            y += 18.0f;
        }

        // Divider
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_BTN_RING).withAlpha(0.5f));
        g.drawHorizontalLine(juce::roundToInt(y), x, x + w);
        y += 6.0f;

        // Verdict tier label
        {
            juce::Colour tierCol = VinylVerdict::tierColour(currentResult.tier);
            g.setColour(tierCol);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(VinylVerdict::tierLabel(currentResult.tier),
                       juce::Rectangle<float>(x, y, w, 16),
                       juce::Justification::centredLeft, false);
            y += 20.0f;
        }

        // Verdict reasoning
        {
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.78f));
            g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
            juce::AttributedString reasonText;
            reasonText.append(currentResult.reasoning,
                              juce::FontOptions(9.5f, juce::Font::plain),
                              juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.78f));
            juce::TextLayout layout;
            layout.createLayout(reasonText, w);
            layout.draw(g, juce::Rectangle<float>(x, y, w, b.getBottom() - y - 6.0f));
            y += juce::jmin(layout.getHeight(), b.getBottom() - y - 6.0f) + 4.0f;
        }

        // HF note (if any)
        if (currentResult.highFreqNote.isNotEmpty() && y < b.getBottom() - 10.0f)
        {
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.5f));
            g.setFont(juce::FontOptions(9.0f, juce::Font::italic));
            juce::AttributedString hfText;
            hfText.append(currentResult.highFreqNote,
                          juce::FontOptions(9.0f, juce::Font::italic),
                          juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.5f));
            juce::TextLayout layout;
            layout.createLayout(hfText, w);
            layout.draw(g, juce::Rectangle<float>(x, y, w, b.getBottom() - y - 4.0f));
        }
    }

    void resized() override {}

private:
    float            currentMono  = 1.0f;
    float            currentCrest = 14.0f;
    VinylVerdictResult currentResult;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VinylPanel)
};
