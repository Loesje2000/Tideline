#pragma once
#include <JuceHeader.h>

enum class VinylVerdictTier { Cuttable, Workable, TalkToEngineer };

struct VinylVerdictResult
{
    VinylVerdictTier tier       = VinylVerdictTier::Cuttable;
    juce::String     reasoning;
    juce::String     highFreqNote; // empty if not notable
};

// Pure logic: combines mono-bass score + crest factor + HF width into a three-tier verdict.
struct VinylVerdict
{
    // monoScore:    0 = fully wide/out-of-phase below 150 Hz, 1 = fully mono
    // crestFactorDB: peak/RMS ratio in dB
    // hfWidthScore: 0 = mono HF, 1 = very wide HF above 5 kHz
    static VinylVerdictResult evaluate(float monoScore, float crestFactorDB, float hfWidthScore)
    {
        VinylVerdictResult result;

        // Classify mono-bass
        // monoScore > 0.85 → well-centered; 0.6–0.85 → mostly mono; < 0.6 → wide
        const bool bassWellCentered = (monoScore > 0.85f);
        const bool bassMostlyMono   = (monoScore >= 0.60f && monoScore <= 0.85f);
        const bool bassWide         = (monoScore < 0.60f);

        // Classify crest factor
        // > 12 dB → plenty; 6–12 dB → moderate; < 6 dB → heavily limited
        const bool crPlenty   = (crestFactorDB > 12.0f);
        const bool crModerate = (crestFactorDB >= 6.0f && crestFactorDB <= 12.0f);
        const bool crLimited  = (crestFactorDB < 6.0f);

        // Tier logic
        if ((bassWellCentered) && (crPlenty || crModerate))
        {
            result.tier = VinylVerdictTier::Cuttable;
            result.reasoning = "Looks cuttable. Your low end (below 150 Hz) is well-centered, "
                               "and there's good headroom for the cutting engineer to work with.";
        }
        else if (bassWide && crLimited)
        {
            result.tier = VinylVerdictTier::TalkToEngineer;
            result.reasoning = "Worth a conversation with your cutting engineer before sending this off. "
                               "The low end below 150 Hz is quite wide, and combined with how limited this "
                               "master is, there's a higher risk of groove-tracking issues. A pass to mono "
                               "the sub/bass region would likely help.";
        }
        else
        {
            result.tier = VinylVerdictTier::Workable;

            juce::String issue1, advice1;
            if (bassWide)
            {
                issue1  = "your bass below 150 Hz is fairly wide";
                advice1 = "Centering the low end before cutting would reduce risk of tracking issues.";
            }
            else if (bassMostlyMono)
            {
                issue1  = "your bass is reasonably centered";
                advice1 = "";
            }

            juce::String issue2, advice2;
            if (crLimited)
            {
                issue2  = "this master is fairly limited";
                advice2 = "Expect the cutting engineer may reduce the level somewhat — "
                          "that's normal and not necessarily a problem.";
            }

            juce::String combined;
            if (issue1.isNotEmpty()) combined += issue1;
            if (issue1.isNotEmpty() && issue2.isNotEmpty()) combined += ", and ";
            if (issue2.isNotEmpty()) combined += issue2;

            result.reasoning = "Workable, but worth flagging: " + combined + ". ";
            if (advice1.isNotEmpty()) result.reasoning += advice1 + " ";
            if (advice2.isNotEmpty()) result.reasoning += advice2;
            result.reasoning = result.reasoning.trimEnd();
        }

        // HF width note appended for any tier when hfWidthScore > 0.5 (notably wide)
        if (hfWidthScore > 0.5f)
        {
            result.highFreqNote = "Also worth knowing: your high end is fairly wide. That's fine for most "
                                  "of the record — just be aware mixes with a lot of wide top end can sound "
                                  "harsher near the end of a side.";
        }

        return result;
    }

    static juce::String tierLabel(VinylVerdictTier tier)
    {
        switch (tier)
        {
            case VinylVerdictTier::Cuttable:        return "Cuttable";
            case VinylVerdictTier::Workable:        return "Workable";
            case VinylVerdictTier::TalkToEngineer:  return "Talk to your cutting engineer";
        }
        return {};
    }

    static juce::Colour tierColour(VinylVerdictTier tier)
    {
        switch (tier)
        {
            case VinylVerdictTier::Cuttable:        return juce::Colour(0xff7abf82); // soft green
            case VinylVerdictTier::Workable:        return juce::Colour(0xffC8A840); // amber
            case VinylVerdictTier::TalkToEngineer:  return juce::Colour(0xffbf6060); // soft red
        }
        return juce::Colours::white;
    }
};
