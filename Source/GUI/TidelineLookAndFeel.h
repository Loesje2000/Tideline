#pragma once
#include <JuceHeader.h>

// Rubblesonic visual identity for Tideline.
// Palette: tobacco brown bg, amber accent, cream text.
class TidelineLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Palette
    static constexpr uint32_t COL_BG        = 0xff2C1F0E; // tobacco brown
    static constexpr uint32_t COL_AMBER     = 0xffC8A840; // amber — needle, glow, accent
    static constexpr uint32_t COL_CREAM     = 0xffE8D5A3; // cream — text, tick marks
    static constexpr uint32_t COL_SAGE      = 0xff7a9e7e; // sage green — Rubblesonic logo
    static constexpr uint32_t COL_PANEL     = 0xff3a2a14; // slightly lighter brown for panels
    static constexpr uint32_t COL_TICK_DIM  = 0xffb8a070; // dimmer cream for secondary ticks
    static constexpr uint32_t COL_BTN_OFF   = 0xff2a1e0a;
    static constexpr uint32_t COL_BTN_RING  = 0xff6b5030;

    TidelineLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(COL_BG));
        setColour(juce::Label::textColourId,                 juce::Colour(COL_CREAM));
        setColour(juce::ToggleButton::textColourId,          juce::Colour(COL_CREAM));
        setColour(juce::TextButton::textColourOffId,         juce::Colour(COL_CREAM));
        setColour(juce::TextButton::textColourOnId,          juce::Colour(COL_AMBER));
        setColour(juce::TextButton::buttonColourId,          juce::Colour(COL_BTN_OFF));
        setColour(juce::TextButton::buttonOnColourId,        juce::Colour(COL_PANEL));
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool highlighted, bool /*down*/) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
        const bool on = button.getToggleState();

        g.setColour(on ? juce::Colour(COL_PANEL) : juce::Colour(COL_BTN_OFF));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(on ? juce::Colour(COL_AMBER) : juce::Colour(COL_BTN_RING));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, on ? 1.5f : 1.0f);

        g.setColour(on ? juce::Colour(COL_AMBER) : juce::Colour(COL_CREAM).withAlpha(0.7f));
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.drawText(button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, false);

        if (highlighted && !on)
        {
            g.setColour(juce::Colour(COL_CREAM).withAlpha(0.06f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& /*bgColour*/,
                              bool /*highlighted*/, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
        const bool on = button.getToggleState();

        g.setColour(on || down ? juce::Colour(COL_PANEL) : juce::Colour(COL_BTN_OFF));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(on ? juce::Colour(COL_AMBER) : juce::Colour(COL_BTN_RING));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, on ? 1.5f : 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*highlighted*/, bool /*down*/) override
    {
        const bool on = button.getToggleState();
        g.setColour(on ? juce::Colour(COL_AMBER) : juce::Colour(COL_CREAM).withAlpha(0.75f));
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, false);
    }

    static void drawRubblesonicWordmark(juce::Graphics& g,
                                         juce::Rectangle<float> bounds,
                                         float rotationDeg = -6.0f)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(
            juce::degreesToRadians(rotationDeg),
            bounds.getCentreX(), bounds.getCentreY()));
        g.setColour(juce::Colour(COL_SAGE));
        g.setFont(juce::FontOptions(bounds.getHeight() * 0.75f, juce::Font::bold));
        g.drawText("Rubblesonic", bounds.toNearestInt(), juce::Justification::centred, false);
        g.restoreState();
    }
};
