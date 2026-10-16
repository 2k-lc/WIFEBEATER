// Offline "jam session": drives the real plugin with scripted MIDI and knob automation
// and renders the result to a WAV file.   usage: wb_jam <out.wav> [workdir]
#include "PluginProcessor.h"
#include <cstdio>
#include <functional>

using namespace juce;
using namespace wb;

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;
constexpr double kBpm = 140.0;
constexpr double kBeat = 60.0 / kBpm;
constexpr double kEnd = 106.0;

WifebeaterProcessor* P = nullptr;
Random rnd (666);

void setPlain (const char* id, float v)
{
    if (getenv ("JAM_SKIP") && String (id).startsWith (getenv ("JAM_SKIP"))) return;
    auto* prm = P->apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
void setNorm (const char* id, float n) { P->apvts.getParameter (id)->setValueNotifyingHost (jlimit (0.f, 1.f, n)); }
float getNorm (const char* id) { return P->apvts.getParameter (id)->getValue(); }

// mod matrix: slot 1..8, source, destination param, amount
void slot (int i, int src, int dstParam, float amt)
{
    int d = 0;
    for (int k = 0; k < kNumModDest; ++k) if (kModDest[k] == dstParam) d = k + 1;
    const String s = "m" + String (i);
    setPlain ((s + "Src").toRawUTF8(), (float) src);
    setPlain ((s + "Dst").toRawUTF8(), (float) d);
    setPlain ((s + "Amt").toRawUTF8(), amt);
}
enum Src { None, LFO1, LFO2, AmpEnv, ModEnv, Chaos, Random_, Follower, Velocity, ModWheel, Key };

struct Note { double t, dur; int note; float vel; };
struct Ev { double t; std::function<void()> fn; };
struct Ramp { const char* id; double t0, t1; float a, b; };

std::vector<Note> notes;
std::vector<Ev> events;
std::vector<Ramp> ramps;

void note (double t, double dur, int n, float v = 0.9f) { notes.push_back ({ t, dur, n, v }); }
void at (double t, std::function<void()> f) { events.push_back ({ t, std::move (f) }); }
void ramp (const char* id, double t0, double t1, float a, float b) { ramps.push_back ({ id, t0, t1, a, b }); }

// randomise a few parameters (normalised space) - the MANGLE button in miniature
void mangle (std::initializer_list<const char*> ids, float amount)
{
    for (auto* id : ids) setNorm (id, getNorm (id) + (rnd.nextFloat() - getNorm (id)) * amount);
}

// a two-bar found-sound loop: kick, noisy snare, hats, one metal ping
void makeSample (const File& f)
{
    const double sr = 44100.0; const int n = (int) (sr * kBeat * 8.0);
    AudioBuffer<float> b (1, n); b.clear();
    float* d = b.getWritePointer (0);
    Random r (4);
    auto add = [&] (double t, std::function<float (double)> fn, double len)
    {
        const int s0 = (int) (t * sr), L = (int) (len * sr);
        for (int i = 0; i < L && s0 + i < n; ++i) d[s0 + i] += fn ((double) i / sr);
    };
    const double twoPi = MathConstants<double>::pi * 2.0;
    for (int beat = 0; beat < 8; ++beat)
    {
        const double t = beat * kBeat;
        if (beat % 2 == 0)
            add (t, [=] (double x) { return (float) (std::sin (twoPi * (40.0 * x + 90.0 * (1.0 - std::exp (-x * 28.0)) / 28.0)) * std::exp (-x * 9.0) * 1.2); }, 0.4);
        else
            add (t, [&] (double x) { return (r.nextFloat() * 2.f - 1.f) * (float) std::exp (-x * 18.0) * 0.8f; }, 0.25);
        for (int h = 0; h < 2; ++h)
            add (t + h * kBeat * 0.5, [&] (double x) { return (r.nextFloat() * 2.f - 1.f) * (float) std::exp (-x * 70.0) * 0.25f; }, 0.06);
    }
    add (kBeat * 3.0, [=] (double x) { return (float) (std::sin (twoPi * 1480.0 * x) * std::exp (-x * 4.0) * 0.3); }, 1.0);
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 16, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, n);
}

// ------------------------------------------------------------------ the score
void compose()
{
    const double T1 = 0, T2 = 12, T3 = 26, T4 = 42, T5 = 58, T6 = 74, T7 = 92, END = kEnd;
    const int root = 36; const int scale[] = { 0, 3, 5, 7, 10, 12, 15 };
    auto n = [=] (int degree, int oct = 0) { return root + scale[degree % 7] + 12 * (degree / 7) + 12 * oct; };

    at (0, [] {
        setPlain ("gain", -9.f); setPlain ("safe", 1); setPlain ("drone", 0); setPlain ("root", 36);
        setPlain ("nLevel", 0); setPlain ("cLevel", 0); setPlain ("oLevel", 0); setPlain ("rLevel", 0); setPlain ("sLevel", 0);
        setPlain ("eA", 0.003f); setPlain ("eD", 0.3f); setPlain ("eS", 0.7f); setPlain ("eR", 0.4f);
        setPlain ("d1On", 1); setPlain ("d2On", 0); setPlain ("d3On", 0); setPlain ("fType", 1); setPlain ("fCut", 400.f); setPlain ("fRes", 0.25f);
        setPlain ("spMode", 0); setPlain ("fbAmt", 0.f);
    });

    // ===== 1. INTRO: resonator plucks over the looped found-sound beat, tube saturation, filter opening
    at (T1, [] {
        setPlain ("sLevel", 0.85f); setPlain ("sMode", 0); setPlain ("sKey", 0); setPlain ("sPitch", 0); setPlain ("sLen", 1.f);
        setPlain ("rLevel", 0.55f); setPlain ("rDecay", 0.95f); setPlain ("rDamp", 0.35f); setPlain ("rExcite", 0.7f);
        setPlain ("d1Type", 0); setPlain ("d1Drive", 0.35f); setPlain ("d1X", 0.4f);
        setPlain ("nLevel", 0.05f); setPlain ("nType", 2);
        slot (1, LFO1, p::d1Drive, 0.2f); setPlain ("l1Rate", 0.4f);
    });
    ramp ("fCut", T1, T2, 300.f, 9000.f);
    ramp ("d1Drive", T1, T2, 0.3f, 0.75f);
    for (int bar = 0; bar < 6; ++bar)
        for (int s = 0; s < 4; ++s)
            if (s != 2) note (T1 + (bar * 4 + s) * kBeat, kBeat * 0.4, n ((bar * 3 + s * 2) % 7, 1), 0.7f);

    // ===== 2. GRANULAR CLOUD: smear + feedback rise
    at (T2, [] {
        setPlain ("sMode", 1); setPlain ("sKey", 1); setPlain ("sLevel", 0.95f); setPlain ("sStart", 0.1f);
        setPlain ("rLevel", 0.15f); setPlain ("nLevel", 0.12f); setPlain ("nType", 1);
        setPlain ("spMode", 3); setPlain ("spAmt", 0.65f); setPlain ("spX", 0.85f);
        setPlain ("d1Type", 1); setPlain ("d1Drive", 0.55f); setPlain ("d1X", 0.3f);
        setPlain ("d2On", 1); setPlain ("d2Type", 14); setPlain ("d2Drive", 0.35f); setPlain ("d2X", 0.3f); setPlain ("d2Mix", 0.5f);
        setPlain ("fbTime", 0.42f); setPlain ("fbTone", 0.35f);
        setPlain ("eA", 0.8f); setPlain ("eS", 1.f); setPlain ("eR", 1.5f);
        slot (1, LFO1, p::sStart, 0.35f); setPlain ("l1Rate", 0.25f); setPlain ("l1Shape", 5);
        slot (2, LFO2, p::gScatPitch, 0.5f); setPlain ("l2Rate", 0.6f); setPlain ("l2Shape", 0);
    });
    ramp ("gDens", T2, T3, 6.f, 130.f);
    ramp ("gSize", T2, T3, 240.f, 22.f);
    ramp ("gScatPos", T2, T3, 0.03f, 0.6f);
    ramp ("gScatPitch", T2, T3, 0.f, 7.f);
    ramp ("fbAmt", T2, T3, 0.f, 0.6f);
    ramp ("fCut", T2, T3, 9000.f, 4000.f);
    note (T2, 6.0, n (0), 0.9f); note (T2 + 6.2, 5.0, n (4), 0.9f); note (T2 + 11.5, 2.5, n (2), 0.9f);

    // ===== 3. GLITCH + DIGITAL DESTRUCTION: rate crush, wavefold, bitcrush, tape-stop
    at (T3, [] {
        setPlain ("fbAmt", 0.f); setPlain ("spMode", 0); setPlain ("sMode", 2); setPlain ("sKey", 0);
        setPlain ("sLevel", 0.9f); setPlain ("xChance", 0.7f); setPlain ("xRev", 0.4f); setPlain ("xStop", 0.25f); setPlain ("xSlice", 90.f);
        setPlain ("nLevel", 0.45f); setPlain ("nType", 0); setPlain ("rLevel", 0.f);
        setPlain ("eA", 0.001f); setPlain ("eD", 0.07f); setPlain ("eS", 1.f); setPlain ("eR", 0.03f); setPlain ("drone", 1); setPlain ("gain", -3.f);
        setPlain ("d1Type", 6); setPlain ("d1Drive", 0.4f); setPlain ("d1X", 0.5f);
        setPlain ("d2On", 1); setPlain ("d2Type", 7); setPlain ("d2Drive", 0.5f); setPlain ("d2X", 0.6f); setPlain ("d2Mix", 0.8f);
        setPlain ("d3On", 1); setPlain ("d3Type", 5); setPlain ("d3Drive", 0.55f); setPlain ("d3X", 0.3f); setPlain ("d3Mix", 0.7f);
        setPlain ("fType", 2); setPlain ("fCut", 90.f); setPlain ("fRes", 0.3f);
        slot (1, Random_, p::d2X, 0.5f); setPlain ("mrRate", 6.f);
        slot (2, LFO2, p::xSlice, 0.4f); setPlain ("l2Shape", 4); setPlain ("l2Rate", 3.f);
        slot (3, ModEnv, p::d1Drive, 0.4f); setPlain ("meA", 0.001f); setPlain ("meD", 0.1f);
    });
    ramp ("xRate", T3, T4, 5.f, 26.f);
    ramp ("d1Drive", T3, T4, 0.25f, 0.9f);
    ramp ("d3Drive", T3, T4, 0.2f, 0.85f);
    ramp ("fCut", T3, T4, 90.f, 2500.f);
    for (double t = T3; t < T4 - 0.01; t += kBeat / 4.0)
    {
        const int step = (int) std::round ((t - T3) / (kBeat / 4.0));
        if (step % 4 == 0 || (step % 8 == 3) || (step % 16 == 14)) note (t, kBeat * 0.15, n (step % 5 == 0 ? 0 : 2, 0), 1.f);
        else if (step % 2 == 1) note (t, kBeat * 0.05, n (5, 2), 0.5f);
    }

    // ===== 4. CHAOS: Lorenz -> feedback FM, comb + chaos-map distortion, wobbling band-pass
    at (T4, [] {
        setPlain ("drone", 0); setPlain ("gain", -9.f);
        setPlain ("sLevel", 0.2f); setPlain ("sMode", 1); setPlain ("sKey", 1); setPlain ("nLevel", 0.f);
        setPlain ("cLevel", 0.85f); setPlain ("cType", 1); setPlain ("cRate", 100.f); setPlain ("cAmt", 0.8f);
        setPlain ("eA", 0.3f); setPlain ("eD", 0.5f); setPlain ("eS", 1.f); setPlain ("eR", 1.f);
        setPlain ("d1Type", 10); setPlain ("d1Drive", 0.85f); setPlain ("d1X", 0.3f); setPlain ("d1Mix", 0.9f);
        setPlain ("d2On", 1); setPlain ("d2Type", 12); setPlain ("d2Drive", 0.5f); setPlain ("d2X", 0.4f); setPlain ("d2Mix", 0.6f);
        setPlain ("d3On", 1); setPlain ("d3Type", 3); setPlain ("d3Drive", 0.5f); setPlain ("d3X", 0.5f); setPlain ("d3Mix", 0.5f);
        setPlain ("fType", 3); setPlain ("fRes", 0.65f); setPlain ("fbAmt", 0.75f); setPlain ("fbTime", 0.3f); setPlain ("fbTone", 0.6f);
        slot (1, LFO1, p::fCut, 0.45f); setPlain ("l1Rate", 0.3f); setPlain ("l1Shape", 0);
        slot (2, Chaos, p::cRate, 0.2f); setPlain ("mcRate", 5.f);
        slot (3, LFO2, p::d1X, 0.5f); setPlain ("l2Shape", 1); setPlain ("l2Rate", 0.7f);
        slot (4, Follower, p::fbTime, 0.15f);
    });
    ramp ("cRate", T4, T4 + 8, 100.f, 1800.f);
    at (T4 + 8, [] { setPlain ("cType", 3); setPlain ("cRate", 300.f); });
    ramp ("cRate", T4 + 8, T5, 300.f, 3000.f);
    ramp ("fCut", T4, T5, 250.f, 6000.f);
    ramp ("d1X", T4, T5, 0.15f, 0.7f);
    note (T4, 4.0, n (0), 1.f); note (T4 + 4, 4.0, n (3), 1.f); note (T4 + 8, 4.0, n (2), 1.f); note (T4 + 12, 4.0, n (1, 1), 1.f);

    // ===== 5. WAVETABLE + OSC: sample as oscillator, FM/sync/ring, wraparound, ring mod, spectral shift
    at (T5, [] {
        setPlain ("gain", -4.f);
        setPlain ("sMode", 3); setPlain ("sKey", 1); setPlain ("sLevel", 0.8f); setPlain ("wFM", 0.35f); setPlain ("wRing", 0.2f);
        setPlain ("cLevel", 0.f); setPlain ("oLevel", 0.55f); setPlain ("oWave", 0); setPlain ("oFM", 0.5f); setPlain ("oSync", 1);
        setPlain ("oRatio", 1.5f); setPlain ("oRing", 0.3f); setPlain ("oDetune", 9.f);
        setPlain ("eA", 0.002f); setPlain ("eD", 0.2f); setPlain ("eS", 0.6f); setPlain ("eR", 0.2f);
        setPlain ("d1Type", 8); setPlain ("d1Drive", 0.6f); setPlain ("d1X", 0.5f); setPlain ("d1Mix", 1.f);
        setPlain ("d2Type", 9); setPlain ("d2Drive", 0.8f); setPlain ("d2X", 0.3f); setPlain ("d2Mix", 0.7f);
        setPlain ("d3Type", 11); setPlain ("d3Drive", 0.3f); setPlain ("d3X", 0.4f); setPlain ("d3Mix", 0.4f);
        setPlain ("fType", 1); setPlain ("fCut", 7000.f); setPlain ("fRes", 0.3f); setPlain ("fbAmt", 0.3f);
        setPlain ("spMode", 6); setPlain ("spAmt", 0.5f);
        slot (1, LFO1, p::wPos, 0.5f); setPlain ("l1Rate", 0.5f); setPlain ("l1Shape", 1);
        slot (2, ModEnv, p::oFM, 0.4f); setPlain ("meD", 0.25f);
        slot (3, Key, p::oRatio, 0.2f);
        slot (4, LFO2, p::d2X, 0.6f); setPlain ("l2Rate", 2.f); setPlain ("l2Shape", 5);
    });
    ramp ("wPos", T5, T6, 0.f, 0.9f);
    ramp ("wWarp", T5, T6, 0.f, 0.8f);
    ramp ("oRatio", T5, T6, 1.5f, 3.2f);
    ramp ("spX", T5, T6, 0.5f, 0.95f);
    ramp ("d2X", T5, T6, 0.15f, 0.7f);
    {
        const int arp[] = { 0, 0, 2, 0, 3, 0, 2, 4 };
        int i = 0;
        for (double t = T5; t < T6 - 0.01; t += kBeat / 2.0, ++i)
            note (t, kBeat * 0.4, n (arp[i % 8], (i / 16) % 2), 0.85f + 0.15f * (float) ((i % 4) == 0));
    }

    // ===== 6. CLIMAX: everything on, noise types swapping, shuffle/freeze/comb, drawn curve, random patch jumps
    at (T6, [] {
        setPlain ("gain", 1.f);
        setPlain ("sMode", 2); setPlain ("sLevel", 0.6f); setPlain ("sKey", 0);
        setPlain ("nLevel", 0.5f); setPlain ("nType", 0); setPlain ("cLevel", 0.5f); setPlain ("cType", 0); setPlain ("cRate", 900.f);
        setPlain ("oLevel", 0.4f); setPlain ("rLevel", 0.45f); setPlain ("rDecay", 0.99f);
        setPlain ("eA", 0.001f); setPlain ("eD", 0.5f); setPlain ("eS", 0.9f); setPlain ("eR", 0.3f);
        setPlain ("d1Type", 4); setPlain ("d1Drive", 0.9f); setPlain ("d1X", 0.5f);
        setPlain ("d2Type", 2); setPlain ("d2Drive", 0.8f); setPlain ("d2X", 0.4f);
        setPlain ("d3On", 1); setPlain ("d3Type", 15); setPlain ("d3Drive", 0.6f); setPlain ("d3X", 0.5f); setPlain ("d3Mix", 0.8f);
        setPlain ("fType", 0); setPlain ("fbAmt", 0.9f); setPlain ("fbTime", 0.25f);
        setPlain ("spMode", 2); setPlain ("spAmt", 0.8f); setPlain ("spX", 0.55f); setPlain ("spY", 0.4f);
        setPlain ("xRate", 18.f); setPlain ("xChance", 0.8f); setPlain ("xStop", 0.35f);
        slot (1, LFO1, p::spX, 0.4f); setPlain ("l1Rate", 4.f); setPlain ("l1Shape", 4);
        slot (2, Chaos, p::d1Drive, 0.3f); setPlain ("mcRate", 14.f);
        slot (3, Random_, p::cRate, 0.5f); setPlain ("mrRate", 9.f);
        slot (4, LFO2, p::fbAmt, 0.15f); setPlain ("l2Rate", 0.5f); setPlain ("l2Shape", 0);
        slot (5, ModEnv, p::d2Drive, 0.2f);
        if (! getenv ("JAM_NOFOLLOW")) slot (6, Follower, p::nLevel, -0.3f);
        for (int i = 0; i < kCurvePoints; ++i) P->curve[(size_t) i].store (rnd.nextFloat() * 2.f - 1.f);
    });
    {
        static const int nts[] = { 0, 3, 4, 5, 6, 2 };
        for (int k = 0; T6 + k * 2 * kBeat < T7; ++k)
            at (T6 + k * 2 * kBeat, [k] { setPlain ("nType", (float) nts[k % 6]); setPlain ("cType", (float) (k % 4)); });
    }
    if (! getenv ("JAM_NOMANGLE")) for (double t = T6; t < T7; t += kBeat)
        at (t, [] { mangle ({ "spX", "spY", "gScatPos", "xSlice", "d1X", "d2X", "d3X", "cAmt", "fbTime", "oRatio" }, 0.5f); });
    at (T6 + 6, [] { setPlain ("spMode", 1); setPlain ("spHold", 1); });
    at (T6 + 9, [] { setPlain ("spHold", 0); setPlain ("spMode", 4); setPlain ("spX", 0.6f); });
    at (T6 + 12, [] { setPlain ("spMode", 8); setPlain ("sMode", 1); setPlain ("gDens", 90.f); setPlain ("gSize", 30.f); });
    at (T6 + 15, [] { setPlain ("spMode", 5); setPlain ("spX", 0.5f); setPlain ("d3Type", 14); setPlain ("sMode", 3); });
    {
        const int rhy[] = { 0, 0, 3, 0, 2, 0, 5, 3, 0, 0, 4, 0, 2, 6, 3, 0 };
        int i = 0;
        for (double t = T6; t < T7 - 0.01; t += kBeat / 4.0, ++i)
            note (t, kBeat * 0.2, n (rhy[i % 16], ((i / 4) % 5 == 4) ? 1 : 0), 0.8f + 0.2f * (float) (i % 4 == 0));
    }

    // ===== 7. OUTRO: freeze, drift into feedback, filter closes
    at (T7, [] {
        setPlain ("gain", -14.f);
        setPlain ("spMode", 1); setPlain ("spHold", 1); setPlain ("spAmt", 0.8f);
        setPlain ("nLevel", 0.15f); setPlain ("cLevel", 0.f); setPlain ("oLevel", 0.f);
        setPlain ("sMode", 1); setPlain ("sLevel", 0.5f); setPlain ("rLevel", 0.5f);
        setPlain ("fType", 1); setPlain ("fCut", 9000.f); setPlain ("fRes", 0.4f);
        setPlain ("eR", 3.f); setPlain ("eA", 0.5f);
        setPlain ("d1Type", 1); setPlain ("d1Drive", 0.5f); setPlain ("d2Type", 0); setPlain ("d2Drive", 0.4f); setPlain ("d3On", 0);
    });
    ramp ("fCut", T7, END, 9000.f, 120.f);
    ramp ("fbAmt", T7, END, 0.9f, 0.55f);
    ramp ("sLevel", T7 + 4, END, 0.5f, 0.f);
    ramp ("nLevel", T7 + 6, END, 0.15f, 0.f);
    note (T7, 4.0, n (0), 0.9f); note (T7 + 5, 6.0, n (4), 0.8f);
    at (END - 3, [] { setPlain ("spHold", 0); });
}
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: wb_jam out.wav [workdir]\n"); return 1; }
    const File outFile (File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const File work = argc > 2 ? File (argv[2]) : outFile.getParentDirectory();
    WifebeaterProcessor proc; P = &proc;
    proc.setPlayConfigDetails (0, 2, kSr, kBlock);
    proc.prepareToPlay (kSr, kBlock);

    const File smp = work.getChildFile ("jam_sample.wav");
    makeSample (smp);
    if (! proc.loadSampleFile (smp)) { std::printf ("could not load sample\n"); return 2; }
    compose();

    const int total = (int) (kEnd * kSr);
    std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });

    struct Msg { int sample; MidiMessage m; };
    std::vector<Msg> msgs;
    for (auto& nn : notes)
    {
        msgs.push_back ({ (int) (nn.t * kSr), MidiMessage::noteOn (1, nn.note, (uint8) jlimit (1, 127, (int) (nn.vel * 127))) });
        msgs.push_back ({ (int) ((nn.t + nn.dur) * kSr), MidiMessage::noteOff (1, nn.note) });
    }
    std::stable_sort (msgs.begin(), msgs.end(), [] (const Msg& a, const Msg& b) { return a.sample < b.sample; });

    AudioBuffer<float> out (2, total);
    AudioBuffer<float> blk (2, kBlock);
    size_t ei = 0, mi = 0;
    double secPeak = 0, secSum = 0; long secCnt = 0; int lastSeg = 0;
    float peakAll = 0;
    for (int pos = 0; pos < total; pos += kBlock)
    {
        const double t = pos / kSr;
        while (ei < events.size() && events[ei].t <= t) events[ei++].fn();
        for (auto& r : ramps)
            if (t >= r.t0 && t <= r.t1) setPlain (r.id, r.a + (r.b - r.a) * (float) ((t - r.t0) / (r.t1 - r.t0)));
        const int n = jmin (kBlock, total - pos);
        MidiBuffer mb;
        while (mi < msgs.size() && msgs[mi].sample < pos + n) { mb.addEvent (msgs[mi].m, jmax (0, msgs[mi].sample - pos)); ++mi; }
        blk.clear();
        proc.processBlock (blk, mb);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, blk, c, 0, n);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float v = blk.getSample (c, i);
                secPeak = jmax (secPeak, (double) std::abs (v)); secSum += (double) v * v; ++secCnt; peakAll = jmax (peakAll, std::abs (v));
            }
        const int seg = (int) (t / 6.0);
        if (seg != lastSeg || pos + n >= total)
        {
            std::printf ("t=%3ds  peak %.2f  rms %.3f\n", lastSeg * 6, secPeak, std::sqrt (secSum / (double) std::max<long> (1, secCnt)));
            secPeak = 0; secSum = 0; secCnt = 0; lastSeg = seg;
        }
    }
    const int fade = (int) (1.5 * kSr);
    for (int c = 0; c < 2; ++c) out.applyGainRamp (c, total - fade, fade, 1.f, 0.f);

    outFile.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<FileOutputStream> os (outFile.createOutputStream());
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), kSr, 2, 16, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (out, 0, total);
    w.reset();
    std::printf ("wrote %s  (%.0f s, overall peak %.2f)\n", outFile.getFullPathName().toRawUTF8(), kEnd, peakAll);
    return 0;
}
