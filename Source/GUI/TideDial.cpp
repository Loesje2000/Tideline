#include "TideDial.h"

TideDial::TideDial()
{
    // Platform list — ordered by LUFS from quietest to loudest.
    // radialSlot: 0 = outermost tick, 1 = slightly inward, 2 = most inward.
    // Platforms sharing -14 LUFS get different slots so tick marks are visually distinct.
    platforms = {
        // idx 0 — Spotify Normal (-14, primary, outermost of the -14 cluster)
        { "Spotify", -14.0f, "Normal", true,
          "Spotify normalizes to -14 LUFS (Normal). Masters louder than this will be turned down automatically.",
          0, "Sptfy" },
        // idx 1 — Apple Music (-16, primary)
        { "Apple Music", -16.0f, "", true,
          "Apple Music (Sound Check) targets approximately -16 LUFS. Louder masters are turned down when Sound Check is on.",
          0, "Apple" },
        // idx 2 — YouTube Video (-14, primary, middle slot)
        { "YouTube", -14.0f, "Video", true,
          "YouTube normalizes video content to -14 LUFS. Louder masters will be turned down.",
          1, "YT" },
        // idx 3 — TIDAL (-14, primary, innermost slot)
        { "TIDAL", -14.0f, "", true,
          "TIDAL normalizes per album using the loudest track — measure your loudest song if album normalization matters for your release.",
          2, "TIDAL" },
        // idx 4 — Spotify Quiet (-19, sub-tick)
        { "Spotify", -19.0f, "Quiet", false,
          "Spotify Quiet mode targets -19 LUFS. Applies when the listener has set Spotify to Quiet.",
          0, "Sptfy Q" },
        // idx 5 — Spotify Loud (-11, sub-tick)
        { "Spotify", -11.0f, "Loud", false,
          "Spotify Loud mode targets -11 LUFS. Only applies when the listener has set Spotify to Loud.",
          0, "Sptfy L" },
        // idx 6 — YouTube Music (-7, sub-tick)
        { "YouTube", -7.0f, "Music", false,
          "YouTube Music targets -7 LUFS — notably louder than most streaming platforms.",
          0, "YT M" },
    };

    selectedTick = 0; // Spotify Normal
    startTimerHz(60);
    setRepaintsOnMouseActivity(true);
}

float TideDial::lufsToAngle(float lufs) const
{
    float t = (lufs - LUFS_MIN) / (LUFS_MAX - LUFS_MIN);
    t = juce::jlimit(0.0f, 1.0f, t);
    return DIAL_START_ANGLE + t * (DIAL_END_ANGLE - DIAL_START_ANGLE);
}

juce::Point<float> TideDial::angleToPoint(float angle, float radius) const
{
    return { dialCentre.x + radius * std::sin(angle),
             dialCentre.y - radius * std::cos(angle) };
}

void TideDial::resized()
{
    auto b = getLocalBounds().toFloat();
    dialRadius = juce::jmin(b.getWidth() * 0.42f, b.getHeight() * 0.80f);
    dialCentre = { b.getCentreX(), b.getHeight() * 0.50f };
}

void TideDial::timerCallback()
{
    // Needle: spring-damper with slight underdamping for analog feel
    const float dt    = 1.0f / 60.0f;
    const float omega = 7.0f;
    const float zeta  = 0.78f;
    float acc = -omega * omega * (needlePos - targetNeedle) - 2.0f * zeta * omega * needleVel;
    needleVel += acc * dt;
    needlePos += needleVel * dt;

    // Glow arc: slow lerp to integrated LUFS (looks like water level settling)
    glowPos += (targetGlow - glowPos) * 0.025f;

    // Settle dot: pulse when unsettled, solid when settled
    if (!isSettled)
        settleAlpha = (std::sin(juce::Time::getMillisecondCounterHiRes() * 0.003) + 1.0f) * 0.5f;
    else
        settleAlpha += (1.0f - settleAlpha) * 0.06f;

    repaint();
}

void TideDial::paint(juce::Graphics& g)
{
    paintDialFace(g);
    paintGlowArc(g);
    if (!vinylMode) paintPlatformTicks(g);
    paintNeedle(g);
    paintSettleDot(g);
    if (hoveredTick >= 0) paintTooltip(g);
}

void TideDial::paintDialFace(juce::Graphics& g)
{
    const float r = dialRadius;

    // Outer bezel
    g.setColour(juce::Colour(0xff150e04));
    g.fillEllipse(dialCentre.x - r - 7, dialCentre.y - r - 7, (r + 7) * 2, (r + 7) * 2);

    // Dial face gradient
    {
        juce::ColourGradient grad(
            juce::Colour(0xff3a2918), dialCentre.x - r * 0.2f, dialCentre.y - r * 0.4f,
            juce::Colour(0xff1c1006), dialCentre.x + r * 0.2f, dialCentre.y + r * 0.3f,
            true);
        g.setGradientFill(grad);
        g.fillEllipse(dialCentre.x - r, dialCentre.y - r, r * 2.0f, r * 2.0f);
    }

    // Inner dial shadow ring (depth effect)
    {
        juce::ColourGradient rim(
            juce::Colour(0x00000000), dialCentre.x, dialCentre.y,
            juce::Colour(0x50000000), dialCentre.x + r, dialCentre.y, true);
        g.setGradientFill(rim);
        g.fillEllipse(dialCentre.x - r, dialCentre.y - r, r * 2.0f, r * 2.0f);
    }

    // Arc track (background sweep)
    {
        juce::Path track;
        track.addCentredArc(dialCentre.x, dialCentre.y, r * 0.84f, r * 0.84f,
                            0.0f, DIAL_START_ANGLE, DIAL_END_ANGLE, true);
        g.setColour(juce::Colour(0xff3e2912));
        g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Scale labels at key LUFS positions — INSIDE the arc (at 66% radius)
    {
        const float labelR = r * 0.66f;
        g.setFont(juce::FontOptions(r * 0.088f, juce::Font::plain));

        struct ScaleMark { float lufs; bool bold; };
        const ScaleMark marks[] = {
            { -24.0f, false }, { -18.0f, false }, { -14.0f, true },
            { -9.0f,  false }, { -3.0f,  false }, { 0.0f,   false }
        };

        for (auto& m : marks)
        {
            float angle = lufsToAngle(m.lufs);
            auto  pt    = angleToPoint(angle, labelR);

            g.setFont(juce::FontOptions(r * 0.088f, m.bold ? juce::Font::bold : juce::Font::plain));
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM)
                            .withAlpha(m.bold ? 0.55f : 0.40f));

            juce::String txt = juce::String(juce::roundToInt(m.lufs));
            g.drawText(txt,
                       juce::Rectangle<float>(pt.x - 13.0f, pt.y - 7.5f, 26.0f, 15.0f),
                       juce::Justification::centred, false);
        }
    }

    // "LUFS" text near dial centre
    {
        g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.28f));
        g.setFont(juce::FontOptions(r * 0.10f, juce::Font::plain));
        g.drawText("LUFS",
                   juce::Rectangle<float>(dialCentre.x - 22.0f, dialCentre.y + r * 0.20f, 44.0f, 15.0f),
                   juce::Justification::centred, false);
    }
}

void TideDial::paintGlowArc(juce::Graphics& g)
{
    if (glowPos <= LUFS_MIN + 0.3f) return;

    float endAngle = lufsToAngle(glowPos);
    const float arcR = dialRadius * 0.84f;

    juce::Path arc;
    arc.addCentredArc(dialCentre.x, dialCentre.y, arcR, arcR,
                      0.0f, DIAL_START_ANGLE, endAngle, true);

    g.setColour(juce::Colour(0x35C8A840));
    g.strokePath(arc, juce::PathStrokeType(9.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0x15C8A840));
    g.strokePath(arc, juce::PathStrokeType(16.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void TideDial::paintPlatformTicks(juce::Graphics& g)
{
    // Each radialSlot gives a different outer radius for the tick line:
    //   slot 0 = 0.98 (outermost, longest tick)
    //   slot 1 = 0.93 (middle length)
    //   slot 2 = 0.88 (innermost of the cluster, shortest)
    // All ticks share the same inner endpoint at 0.84 (the arc track).
    //
    // Labels are drawn INSIDE the arc at ~0.73 radius for slot 0, stepping inward
    // for slot 1 and 2, so they don't overlap.
    // Only slot-0 primary ticks show a label by default; selected tick always shows its label.

    static const float outerSlot[] = { 0.98f, 0.93f, 0.88f };
    static const float labelSlot[] = { 0.75f, 0.68f, 0.61f }; // label radii per slot

    const float arcR = dialRadius * 0.84f; // tick inner endpoint

    for (int i = 0; i < (int)platforms.size(); ++i)
    {
        const auto& p = platforms[i];
        const bool isSelected = (i == selectedTick);
        const bool isHovered  = (i == hoveredTick);

        float angle    = lufsToAngle(p.targetLufs);
        float outerR   = dialRadius * outerSlot[p.radialSlot];

        auto ptIn  = angleToPoint(angle, arcR);
        auto ptOut = angleToPoint(angle, outerR);

        // Tick colour: amber for selected/hovered, cream otherwise (dimmer for sub-ticks)
        juce::Colour tickCol;
        if (isSelected)
            tickCol = juce::Colour(TidelineLookAndFeel::COL_AMBER);
        else if (isHovered)
            tickCol = juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(0.70f);
        else if (p.isPrimary)
            tickCol = juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.50f);
        else
            tickCol = juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.25f);

        float lineW = p.isPrimary ? (isSelected ? 2.5f : 1.6f) : (isSelected ? 2.0f : 1.0f);

        g.setColour(tickCol);
        g.drawLine(ptIn.x, ptIn.y, ptOut.x, ptOut.y, lineW);

        // Selection dot just beyond the tick end
        if (isSelected)
        {
            auto ptDot = angleToPoint(angle, outerR + dialRadius * 0.025f);
            g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER));
            g.fillEllipse(ptDot.x - 3.0f, ptDot.y - 3.0f, 6.0f, 6.0f);
        }

        // Label — only for:
        //   • Primary slot-0 ticks that are NOT in a shared-LUFS cluster (show shortName always)
        //   • The currently selected tick (always show full name at its slot radius)
        //   • Hovered ticks (tooltip handles the detailed text; skip inline label when hovering)
        bool isClustered = (p.radialSlot > 0); // slot 1 or 2 = part of -14 cluster

        bool showLabel = isSelected || (p.isPrimary && p.radialSlot == 0 && !isClustered);
        // Also show shortName for slot-0 primary ticks that ARE clustered — just the first one
        // (Spotify Normal is slot 0 in the cluster, so it always shows)
        if (p.isPrimary && p.radialSlot == 0)
            showLabel = true;

        if (showLabel && !isHovered) // tooltip replaces inline label when hovered
        {
            float labelR = dialRadius * labelSlot[p.radialSlot];
            auto  ptLbl  = angleToPoint(angle, labelR);

            juce::String labelTxt = isSelected
                ? (p.name + (p.subLabel.isEmpty() ? "" : "\n" + p.subLabel))
                : p.shortName;

            float fontSize = p.isPrimary ? (isSelected ? 9.5f : 8.5f) : 8.0f;
            g.setFont(juce::FontOptions(fontSize, isSelected ? juce::Font::bold : juce::Font::plain));
            g.setColour(tickCol.withMultipliedAlpha(0.90f));

            // Text bounding box centred on the label point
            g.drawText(labelTxt,
                       juce::Rectangle<float>(ptLbl.x - 26.0f, ptLbl.y - 13.0f, 52.0f, 26.0f),
                       juce::Justification::centred, true);
        }
    }
}

void TideDial::paintNeedle(juce::Graphics& g)
{
    float angle    = lufsToAngle(needlePos);
    float needleL  = dialRadius * 0.80f;
    float pivotR   = dialRadius * 0.055f;
    auto  tipPt    = angleToPoint(angle, needleL);

    // Drop shadow
    g.setColour(juce::Colour(0x35000000));
    g.drawLine(dialCentre.x + 1.5f, dialCentre.y + 1.5f,
               tipPt.x + 1.5f,       tipPt.y + 1.5f, 2.5f);

    // Needle body
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER));
    g.drawLine(dialCentre.x, dialCentre.y, tipPt.x, tipPt.y, 2.0f);

    // Tip highlight
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(0.35f));
    g.fillEllipse(tipPt.x - 3.5f, tipPt.y - 3.5f, 7.0f, 7.0f);

    // Pivot cap
    g.setColour(juce::Colour(0xff2e1e08));
    g.fillEllipse(dialCentre.x - pivotR, dialCentre.y - pivotR, pivotR * 2.0f, pivotR * 2.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(0.55f));
    g.drawEllipse(dialCentre.x - pivotR, dialCentre.y - pivotR, pivotR * 2.0f, pivotR * 2.0f, 1.0f);
}

void TideDial::paintSettleDot(juce::Graphics& g)
{
    // Settle indicator: bottom-right of dial face
    const float dotR = 4.0f;
    const float dotX = dialCentre.x + dialRadius * 0.66f;
    const float dotY = dialCentre.y + dialRadius * 0.62f;

    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(settleAlpha * 0.80f));
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.18f));
    g.drawEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f, 0.8f);
}

void TideDial::paintTooltip(juce::Graphics& g)
{
    if (hoveredTick < 0 || hoveredTick >= (int)platforms.size()) return;

    const auto& p = platforms[hoveredTick];

    juce::String header = p.name;
    if (p.subLabel.isNotEmpty()) header += " (" + p.subLabel + ")";
    header += "  —  " + juce::String(p.targetLufs, 0) + " LUFS";

    float tipW = juce::jmin((float)getWidth() - 16.0f, 360.0f);
    float tipH = 48.0f;
    float tipX = juce::jlimit(8.0f, (float)getWidth() - tipW - 8.0f,
                               dialCentre.x - tipW * 0.5f);
    float tipY = (float)getHeight() - tipH - 6.0f;

    // Background
    g.setColour(juce::Colour(0xee0f0802));
    g.fillRoundedRectangle(tipX, tipY, tipW, tipH, 5.0f);
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER).withAlpha(0.30f));
    g.drawRoundedRectangle(tipX + 0.5f, tipY + 0.5f, tipW - 1.0f, tipH - 1.0f, 5.0f, 1.0f);

    // Header
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_AMBER));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(header,
               juce::Rectangle<float>(tipX + 9.0f, tipY + 5.0f, tipW - 18.0f, 14.0f),
               juce::Justification::centredLeft, false);

    // Body
    g.setColour(juce::Colour(TidelineLookAndFeel::COL_CREAM).withAlpha(0.72f));
    g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
    g.drawText(p.tooltip,
               juce::Rectangle<float>(tipX + 9.0f, tipY + 21.0f, tipW - 18.0f, 22.0f),
               juce::Justification::centredLeft, true);
}

int TideDial::findTickAt(juce::Point<float> pos) const
{
    // Test outer-slot ticks first (they're easier to click)
    static const float outerSlot[] = { 0.98f, 0.93f, 0.88f };
    int best = -1;
    float bestDist = TICK_HIT_RADIUS;

    for (int i = 0; i < (int)platforms.size(); ++i)
    {
        float angle  = lufsToAngle(platforms[i].targetLufs);
        float midR   = dialRadius * (0.84f + outerSlot[platforms[i].radialSlot]) * 0.5f;
        auto  ptMid  = angleToPoint(angle, midR);
        float dist   = pos.getDistanceFrom(ptMid);
        if (dist < bestDist) { bestDist = dist; best = i; }
    }
    return best;
}

void TideDial::mouseMove(const juce::MouseEvent& e)
{
    int prev = hoveredTick;
    hoveredTick = findTickAt(e.position);
    if (hoveredTick != prev) repaint();
}

void TideDial::mouseDown(const juce::MouseEvent& e)
{
    int tick = findTickAt(e.position);
    if (tick >= 0 && tick != selectedTick)
    {
        selectedTick = tick;
        if (onTickSelected) onTickSelected(tick);
        repaint();
    }
}

void TideDial::mouseExit(const juce::MouseEvent&)
{
    if (hoveredTick >= 0) { hoveredTick = -1; repaint(); }
}
