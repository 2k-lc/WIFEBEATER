#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { std::printf ("FAIL: %s\n", msg); ++failures; } } while (0)

struct Stats { float peak = 0, rms = 0; bool finite = true; };

static Stats render (WifebeaterProcessor& p, double sr, int blocks, bool noteOn)
{
    Stats st; double sum = 0; long cnt = 0;
    AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer m;
        if (noteOn && b == 0) m.addEvent (MidiMessage::noteOn (1, 48, (uint8) 110), 0);
        buf.clear();
        p.processBlock (buf, m);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) st.finite = false;
                st.peak = std::max (st.peak, std::abs (v)); sum += (double) v * v; ++cnt;
            }
    }
    st.rms = (float) std::sqrt (sum / (double) cnt);
    juce::ignoreUnused (sr);
    return st;
}

static void setP (WifebeaterProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 48000;
    WifebeaterProcessor p;
    p.setPlayConfigDetails (0, 2, sr, 512);
    p.prepareToPlay (sr, 512);

    // 0. spectral engine is unity gain when it does nothing (gate mode, threshold 0)
    {
        auto sp = std::make_unique<wb::Spectral>(); sp->update (5, 1.f, 0.f, 0.f, false);
        double e = 0; int c = 0;
        for (int i = 0; i < 20000; ++i) { const float x = std::sin (0.05f * (float) i); const float y = sp->tick (x);
            if (i > 4000) { e += (double) y * y; ++c; } }
        const double rms = std::sqrt (e / c); std::printf ("spectral identity rms %.4f (expect 0.7071)\n", rms);
        CHECK (std::abs (rms - 0.7071) < 0.02, "spectral unity gain");
    }
    // 0b. cpu cost of a heavy patch
    {
        WifebeaterProcessor h; h.setPlayConfigDetails (0, 2, sr, 512); h.prepareToPlay (sr, 512);
        auto set = [&] (const char* id, float v) { auto* q = h.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set ("drone", 1); set ("cLevel", 0.5f); set ("oLevel", 0.5f); set ("rLevel", 0.5f); set ("sLevel", 0.6f); set ("sMode", 1);
        set ("spMode", 2); set ("d2On", 1); set ("d3On", 1); set ("fbAmt", 0.5f); set ("fType", 1);
        const auto t0 = Time::getMillisecondCounterHiRes();
        AudioBuffer<float> b (2, 512); MidiBuffer m;
        for (int i = 0; i < 400; ++i) { b.clear(); h.processBlock (b, m); }
        const double ms = Time::getMillisecondCounterHiRes() - t0; const double audioMs = 400.0 * 512.0 / sr * 1000.0;
        std::printf ("heavy patch cpu: %.1f%% of realtime\n", 100.0 * ms / audioMs);
    }
    // 1. silence when no note and no drone
    { auto s = render (p, sr, 8, false); CHECK (s.peak < 1e-6f, "silent without note"); }
    // 2. default patch with a note makes sound, bounded by the limiter
    { auto s = render (p, sr, 40, true); std::printf ("default: peak %.3f rms %.3f\n", s.peak, s.rms);
      CHECK (s.finite, "finite"); CHECK (s.peak > 0.01f, "audible"); CHECK (s.peak <= 0.95f, "limited"); }

    // 3. every sample mode, noise type, chaos type and distortion type, with and w/o spectral
    setP (p, "drone", 1);
    setP (p, "nLevel", 0.3f); setP (p, "cLevel", 0.5f); setP (p, "oLevel", 0.5f); setP (p, "rLevel", 0.5f); setP (p, "sLevel", 0.6f);
    setP (p, "d2On", 1); setP (p, "d3On", 1); setP (p, "fbAmt", 0.9f); setP (p, "fType", 1);
    for (int sm = 0; sm < 4; ++sm)
        for (int nt = 0; nt < 7; ++nt)
        {
            setP (p, "sMode", (float) sm); setP (p, "nType", (float) nt); setP (p, "cType", (float) (nt % 4));
            for (int dt = 0; dt < 17; ++dt)
            {
                setP (p, "d1Type", (float) dt); setP (p, "d2Type", (float) ((dt + 5) % 17)); setP (p, "d3Type", (float) ((dt + 11) % 17));
                setP (p, "d1Drive", dt % 2 ? 1.f : 0.3f);
                auto s = render (p, sr, 3, false);
                if (! s.finite || s.peak > 0.95f) { std::printf ("sm %d nt %d dt %d peak %f finite %d\n", sm, nt, dt, s.peak, (int) s.finite); }
                CHECK (s.finite, "finite output"); CHECK (s.peak <= 0.95f, "limited output");
            }
        }
    for (int sp = 1; sp < 9; ++sp)
    {
        setP (p, "spMode", (float) sp); setP (p, "spHold", sp == 1 ? 1.f : 0.f);
        auto s = render (p, sr, 10, false); CHECK (s.finite, "spectral finite"); CHECK (s.peak > 0.f, "spectral audible");
    }
    // 4. mod matrix
    setP (p, "m1Src", 1); setP (p, "m1Dst", 1); setP (p, "m1Amt", 1.f);
    setP (p, "m2Src", 5); setP (p, "m2Dst", 30); setP (p, "m2Amt", 0.7f);
    { auto s = render (p, sr, 20, false); CHECK (s.finite, "mod finite"); }
    // 5. limiter off still finite
    setP (p, "safe", 0); { auto s = render (p, sr, 10, false); CHECK (s.finite, "raw finite"); std::printf ("raw peak %.2f\n", s.peak); }
    // 6. state round trip
    { MemoryBlock mb; p.getStateInformation (mb); WifebeaterProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
      CHECK (std::abs (q.apvts.getRawParameterValue ("sMode")->load() - 3.f) < 0.01f, "state restores"); }
    // 7. editor builds
    { std::unique_ptr<AudioProcessorEditor> ed (p.createEditor()); CHECK (ed != nullptr, "editor");
      auto* we = dynamic_cast<WifebeaterEditor*> (ed.get());
      for (int pg = 0; pg < 5; ++pg)
      {
          we->showPage (pg);
          Image img = ed->createComponentSnapshot (ed->getLocalBounds());
          PNGImageFormat png; File out ("/tmp/wb_page" + String (pg) + ".png"); FileOutputStream fo (out); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
      } }

    std::printf (failures ? "FAILED (%d)\n" : "ALL OK\n", failures);
    return failures ? 1 : 0;
}
