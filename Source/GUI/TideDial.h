#pragma once
#include <JuceHeader.h>
#include "TidelineLookAndFeel.h"

struct PlatformTarget
{
    juce::String name;
    float        targetLufs;
    juce::String subLabel;
    bool         isPrimary;
    juce::String tooltip;
    int          radialSlot = 0;  // 0 = outermost; 1, 2 = stepped inward for same-LUFS clusters
    juce::String shortName;       // abbreviated label for always-on display
};

class TideDial : public juce::Component,
                 public juce::Timer
{
public:
    TideDial();
    ~TideDial() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    void setMomentaryLUFS(float v)  { targetNeedle = juce::jlimit(-30.0f, 0.0f, v); }
    void setIntegratedLUFS(float v) { targetGlow   = juce::jlimit(-30.0f, 0.0f, v); }
    void setSettled(bool s)         { isSettled = s; }
    void setVinylMode(bool v)       { vinylMode = v; repaint(); }

    float getSelectedTargetLUFS() const
    {
        if (selectedTick >= 0 && selectedTick < (int)platforms.size())
            return platforms[selectedTick].targetLufs;
        return -14.0f;
    }

    std::function<void(int platformIndex)> onTickSelected;

private:
    static constexpr float DIAL_START_ANGLE = -2.356f;
    static constexpr float DIAL_END_ANGLE   =  2.356f;
    static constexpr float LUFS_MIN = -24.0f;
    static constexpr float LUFS_MAX =   0.0f;

    float needlePos    = -18.0f;
    float needleVel    = 0.0f;
    float targetNeedle = -18.0f;

    float glowPos    = -24.0f;
    float targetGlow = -24.0f;

    bool  isSettled   = false;
    bool  vinylMode   = false;
    float settleAlpha = 0.0f;

    int hoveredTick  = -1;
    int selectedTick = 0;   // default = Spotify Normal (index 0)

    static constexpr float TICK_HIT_RADIUS = 14.0f;

    std::vector<PlatformTarget> platforms;

    juce::Point<float> dialCentre;
    float dialRadius = 0.0f;

    float lufsToAngle(float lufs) const;
    juce::Point<float> angleToPoint(float angle, float radius) const;
    int   findTickAt(juce::Point<float> pos) const;

    void paintDialFace(juce::Graphics& g);
    void paintGlowArc(juce::Graphics& g);
    void paintNeedle(juce::Graphics& g);
    void paintPlatformTicks(juce::Graphics& g);
    void paintSettleDot(juce::Graphics& g);
    void paintTooltip(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TideDial)
};
