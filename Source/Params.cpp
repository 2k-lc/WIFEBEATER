#include "ParamIds.h"
#include <map>

using namespace juce;

namespace wb
{
AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    std::map<String, String> names;

    auto F = [&] (const char* id, const char* name, float lo, float hi, float def,
                  float centre = 0.f, const char* unit = "", float step = 0.f)
    {
        NormalisableRange<float> r (lo, hi, step);
        if (centre > lo && centre < hi) r.setSkewForCentre (centre);
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                             AudioParameterFloatAttributes().withLabel (unit)
                .withStringFromValueFunction ([] (float v, int) { return String (std::abs (v) >= 100.f ? String (roundToInt (v)) : std::abs (v) >= 10.f ? String (v, 1) : String (v, 2)); })));
        names[id] = name;
    };
    auto C = [&] (const char* id, const char* name, StringArray choices, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    auto B = [&] (const char* id, const char* name, bool def)
    {
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // ---- master
    F ("gain", "Output", -60.f, 12.f, -6.f, 0.f, "dB");
    B ("safe", "Safe Limiter", true);
    B ("drone", "Drone", false);
    F ("root", "Root Note", 12.f, 84.f, 36.f, 0.f, "", 1.f);
    F ("inLevel", "Audio In", 0.f, 2.f, 0.f, 0.5f);
    F ("width", "Width", 0.f, 1.f, 1.f);

    // ---- amp envelope
    F ("eA", "Attack", 0.001f, 5.f, 0.002f, 0.1f, "s");
    F ("eD", "Decay", 0.001f, 5.f, 0.3f, 0.2f, "s");
    F ("eS", "Sustain", 0.f, 1.f, 1.f);
    F ("eR", "Release", 0.001f, 10.f, 0.15f, 0.3f, "s");

    // ---- noise
    F ("nLevel", "Noise", 0.f, 1.f, 0.6f);
    C ("nType", "Noise Type", { "White", "Pink", "Brown", "Blue", "Violet", "Crackle", "Impulse" }, 0);
    F ("nColor", "Color", 0.f, 1.f, 0.5f);

    // ---- chaos
    F ("cLevel", "Chaos", 0.f, 1.f, 0.f);
    C ("cType", "Chaos Type", { "Logistic", "Lorenz", "Henon", "Feedback FM" }, 0);
    F ("cRate", "Chaos Rate", 20.f, 20000.f, 800.f, 1000.f, "Hz");
    F ("cAmt", "Chaos Amt", 0.f, 1.f, 0.7f);

    // ---- oscillator
    F ("oLevel", "Osc", 0.f, 1.f, 0.f);
    C ("oWave", "Osc Wave", { "Saw", "Square", "Sine", "Triangle" }, 0);
    F ("oTune", "Tune", -48.f, 48.f, 0.f, 0.f, "st");
    F ("oDetune", "Detune", -100.f, 100.f, 0.f, 0.f, "ct");
    F ("oRatio", "B Ratio", 0.25f, 16.f, 1.5f, 2.f);
    F ("oFM", "FM", 0.f, 1.f, 0.f);
    B ("oSync", "Hard Sync", false);
    F ("oRing", "Ring", 0.f, 1.f, 0.f);

    // ---- resonator
    F ("rLevel", "Resonator", 0.f, 1.f, 0.f);
    F ("rDecay", "Res Decay", 0.f, 1.f, 0.9f);
    F ("rDamp", "Res Damp", 0.f, 1.f, 0.3f);
    F ("rExcite", "Excite", 0.f, 1.f, 0.5f);

    // ---- sample engine
    F ("sLevel", "Sample", 0.f, 1.f, 0.4f);
    C ("sMode", "Sample Mode", { "Loop", "Granular", "Glitch", "Wavetable" }, 0);
    F ("sStart", "Start", 0.f, 1.f, 0.f);
    F ("sLen", "Length", 0.001f, 1.f, 1.f, 0.2f);
    F ("sPitch", "Pitch", -48.f, 48.f, 0.f, 0.f, "st");
    B ("sKey", "Keytrack", true);
    B ("sRev", "Reverse", false);
    F ("gSize", "Grain Size", 1.f, 500.f, 60.f, 40.f, "ms");
    F ("gDens", "Density", 1.f, 200.f, 25.f, 30.f, "Hz");
    F ("gScatPos", "Pos Scatter", 0.f, 1.f, 0.1f);
    F ("gScatPitch", "Pitch Scatter", 0.f, 12.f, 0.f, 0.f, "st");
    F ("gRev", "Grain Rev", 0.f, 1.f, 0.f);
    F ("xRate", "Clock", 0.1f, 64.f, 6.f, 6.f, "Hz");
    F ("xSlice", "Slice", 1.f, 500.f, 80.f, 60.f, "ms");
    F ("xChance", "Repeat", 0.f, 1.f, 0.5f);
    F ("xRev", "Reverse", 0.f, 1.f, 0.2f);
    F ("xStop", "Tape Stop", 0.f, 1.f, 0.1f);
    F ("wPos", "WT Pos", 0.f, 1.f, 0.f);
    F ("wWarp", "WT Warp", 0.f, 1.f, 0.f);
    F ("wFM", "WT FM", 0.f, 1.f, 0.f);
    F ("wRing", "WT Ring", 0.f, 1.f, 0.f);

    // ---- spectral
    C ("spMode", "Spectral Mode", { "Off", "Freeze", "Shuffle", "Smear", "Scramble", "Gate", "Shift", "Robot", "Comb" }, 0);
    F ("spAmt", "Spectral Mix", 0.f, 1.f, 1.f);
    F ("spX", "Spectral X", 0.f, 1.f, 0.5f);
    F ("spY", "Spectral Y", 0.f, 1.f, 0.5f);
    B ("spHold", "Freeze Hold", false);

    // ---- distortion slots
    const StringArray distTypes { "Tube", "Tape", "Diode", "Hard Clip", "Fuzz", "Bitcrush", "Rate Crush",
                                  "Wavefold", "Overflow", "Ring Mod", "Comb", "FM", "Chaos Map",
                                  "Rectify", "Chebyshev", "Drawn Curve", "Slew" };
    const char* dn[3][6] = {
        { "d1On", "d1Type", "d1Drive", "d1X", "d1Tone", "d1Mix" },
        { "d2On", "d2Type", "d2Drive", "d2X", "d2Tone", "d2Mix" },
        { "d3On", "d3Type", "d3Drive", "d3X", "d3Tone", "d3Mix" } };
    const int defType[3] = { 0, 5, 7 };
    for (int i = 0; i < 3; ++i)
    {
        const String n = "D" + String (i + 1) + " ";
        B (dn[i][0], (n + "On").toRawUTF8(), i == 0);
        C (dn[i][1], (n + "Type").toRawUTF8(), distTypes, defType[i]);
        F (dn[i][2], (n + "Drive").toRawUTF8(), 0.f, 1.f, 0.5f);
        F (dn[i][3], (n + "X").toRawUTF8(), 0.f, 1.f, 0.5f);
        F (dn[i][4], (n + "Tone").toRawUTF8(), 0.f, 1.f, 0.5f);
        F (dn[i][5], (n + "Mix").toRawUTF8(), 0.f, 1.f, 1.f);
    }

    // ---- feedback + filter
    F ("fbAmt", "Feedback", 0.f, 1.f, 0.f);
    F ("fbTime", "FB Time", 0.f, 1.f, 0.3f);
    F ("fbTone", "FB Tone", 0.f, 1.f, 0.5f);
    C ("fType", "Filter", { "Off", "Lowpass", "Highpass", "Bandpass", "Notch" }, 0);
    F ("fCut", "Cutoff", 20.f, 20000.f, 8000.f, 1000.f, "Hz");
    F ("fRes", "Resonance", 0.f, 1.f, 0.2f);

    // ---- modulators
    const StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth Rand" };
    F ("l1Rate", "LFO1 Rate", 0.01f, 500.f, 1.f, 3.f, "Hz");
    C ("l1Shape", "LFO1 Shape", shapes, 0);
    F ("l2Rate", "LFO2 Rate", 0.01f, 500.f, 0.3f, 3.f, "Hz");
    C ("l2Shape", "LFO2 Shape", shapes, 4);
    F ("meA", "ModEnv A", 0.001f, 2.f, 0.01f, 0.05f, "s");
    F ("meD", "ModEnv D", 0.005f, 5.f, 0.3f, 0.3f, "s");
    F ("mcRate", "Chaos Rate", 0.1f, 200.f, 8.f, 10.f, "Hz");
    F ("mrRate", "Random Rate", 0.1f, 100.f, 4.f, 4.f, "Hz");

    StringArray dstNames { "None" };
    for (int d : kModDest) dstNames.add (names[kIdStr[d]]);
    for (int i = 1; i <= kNumModSlots; ++i)
    {
        const String s = "m" + String (i);
        C ((s + "Src").toRawUTF8(), ("Mod " + String (i) + " Src").toRawUTF8(), kModSrcNames, 0);
        C ((s + "Dst").toRawUTF8(), ("Mod " + String (i) + " Dest").toRawUTF8(), dstNames, 0);
        F ((s + "Amt").toRawUTF8(), ("Mod " + String (i) + " Amt").toRawUTF8(), -1.f, 1.f, 0.f);
    }

    jassert ((int) ps.size() == p::kNumP);
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& q : ps) layout.add (std::move (q));
    return layout;
}
}
