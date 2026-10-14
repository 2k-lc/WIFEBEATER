#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "ParamIds.h"

// All DSP for one channel lives here. Everything is header-only, allocation-free
// on the audio thread, and driven by a per-sub-block parameter Frame.
namespace wb
{
using Frame = std::array<float, p::kNumP>;
inline int I (const Frame& f, int id) { return (int) (f[(size_t) id] + 0.5f); }

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed = 1) : s (seed ? seed : 1) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    inline float bi() { return uni() * 2.f - 1.f; }
};

inline float coefHz (float hz, double sr)
{
    return 1.f - std::exp (-kTwoPi * std::min (hz, (float) (sr * 0.45)) / (float) sr);
}

inline float finiteOr (float x, float fallback = 0.f)
{
    return std::isfinite (x) ? x : fallback;
}

// 0..1 tilt: <0.5 low-pass, >0.5 high-pass, 0.5 = flat
struct Tilt
{
    float z = 0, g = 1; bool hp = false;
    void set (float t, double sr)
    {
        if (t < 0.5f) { hp = false; g = coefHz (100.f * std::pow (200.f, t * 2.f), sr); }
        else          { hp = true;  g = coefHz (20.f * std::pow (250.f, (t - 0.5f) * 2.f), sr); }
    }
    inline float tick (float x) { z += g * (x - z); return hp ? x - z : z; }
};

struct DcBlock
{
    float x1 = 0, y1 = 0;
    inline float tick (float x, float r = 0.9975f) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

//==============================================================================
struct NoiseGen
{
    Rng rng { 12345 };
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, brown = 0, w1 = 0, w2 = 0;
    int impCount = 0;
    Tilt tilt; int type = 0; float color = 0.5f; double sr = 44100;

    void update (int t, float c, double s) { type = t; color = c; sr = s; if (t < 5) tilt.set (c, s); }

    inline float tick()
    {
        const float w = rng.bi();
        float o = 0.f;
        switch (type)
        {
            case 0: o = w; break;
            case 1:
                b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
                b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
                b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
                o = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.15f; b6 = w * 0.115926f; break;
            case 2: brown = (brown + 0.02f * w) / 1.02f; o = brown * 4.f; break;
            case 3: o = (w - w1) * 0.7f; break;
            case 4: o = (w - 2.f * w1 + w2) * 0.45f; break;
            case 5:
            {
                const float dens = 0.0003f + color * color * color * 0.05f;
                const float u = rng.uni();
                o = u < dens ? rng.bi() * (0.3f + 0.7f * rng.uni()) : 0.f; break;
            }
            default:
                if (--impCount <= 0)
                {
                    const float hz = 3.f + color * color * 3000.f;
                    impCount = (int) std::max (1.f, (float) sr / hz * (0.5f + rng.uni()));
                    o = rng.bi();
                }
                break;
        }
        w2 = w1; w1 = w;
        return type < 5 ? tilt.tick (o) : o;
    }
};

//==============================================================================
struct ChaosOsc
{
    Rng rng { 777 };
    float x = 0.3f, y = 0.1f, held = 0.f, fb1 = 0.f, fb2 = 0.f;
    double acc = 0, phase = 0, lx = 1, ly = 1, lz = 1;
    int type = 0; float rate = 800.f, amt = 0.7f; double sr = 44100;

    void update (int t, float r, float a, double s) { type = t; rate = r; amt = a; sr = s; }

    inline float tick()
    {
        switch (type)
        {
            case 0:
            {
                acc += rate / sr; int guard = 0;
                const float r = 3.5f + 0.4999f * amt;
                while (acc >= 1.0 && guard++ < 16)
                {
                    acc -= 1.0; x = r * x * (1.f - x);
                    if (! (x > 1e-6f && x < 0.999999f)) x = 0.01f + 0.98f * rng.uni();
                    held = 2.f * x - 1.f;
                }
                return held;
            }
            case 1:
            {
                const int sub = 4; const double dt = std::min (0.02, (double) rate / sr * 3.0 / sub);
                const double rho = 24.0 + 24.0 * amt;
                for (int i = 0; i < sub; ++i)
                {
                    const double dx = 10.0 * (ly - lx), dy = lx * (rho - lz) - ly, dz = lx * ly - 2.6667 * lz;
                    lx += dx * dt; ly += dy * dt; lz += dz * dt;
                }
                if (! std::isfinite (lx + ly + lz) || std::abs (lx) > 500.0) { lx = 1; ly = 1; lz = 1; }
                return std::clamp ((float) lx * 0.05f, -1.f, 1.f);
            }
            case 2:
            {
                acc += rate / sr; int guard = 0;
                const float a = 1.0f + 0.4f * amt;
                while (acc >= 1.0 && guard++ < 16)
                {
                    acc -= 1.0; const float nx = 1.f - a * x * x + y; y = 0.3f * x; x = nx;
                    if (! std::isfinite (x) || std::abs (x) > 3.f) { x = 0.1f; y = 0.f; }
                    held = std::clamp (x * 0.8f, -1.f, 1.f);
                }
                return held;
            }
            default:
            {
                phase += rate / sr; phase -= std::floor (phase);
                const float beta = 0.5f + amt * 4.5f;
                const float o = std::sin (kTwoPi * (float) phase + beta * 0.5f * (fb1 + fb2));
                fb2 = fb1; fb1 = o; return o;
            }
        }
    }
};

//==============================================================================
struct ClassicOsc
{
    double pa = 0, pb = 0; int wave = 0; float freq = 110.f, ratio = 1.5f, det = 0.f, fm = 0.f, ring = 0.f;
    bool sync = false; double sr = 44100;

    void update (int w, float f, float cents, float r, float fmAmt, bool s, float rg, double sampleRate)
    { wave = w; freq = f; det = std::exp2 (cents / 1200.f); ratio = r; fm = fmAmt; sync = s; ring = rg; sr = sampleRate; }

    inline float tick()
    {
        pb += (double) (freq * ratio * det) / sr;
        if (pb >= 1.0) { pb -= std::floor (pb); if (sync) pa = 0.0; }
        const float b = std::sin (kTwoPi * (float) pb);
        pa += (double) freq / sr; pa -= std::floor (pa);
        double ph = pa + (double) (fm * b * 1.5f); ph -= std::floor (ph);
        const float pf = (float) ph;
        float a;
        switch (wave)
        {
            case 0: a = 2.f * pf - 1.f; break;
            case 1: a = pf < 0.5f ? 1.f : -1.f; break;
            case 2: a = std::sin (kTwoPi * pf); break;
            default: a = 4.f * std::abs (pf - 0.5f) - 1.f; break;
        }
        return a + ring * (a * b - a);
    }
};

//==============================================================================
struct Resonator
{
    std::vector<float> buf; int mask = 0, w = 0; float lp = 0; Rng rng { 4242 };
    float freq = 110.f, decay = 0.9f, damp = 0.3f, exc = 0.5f; double sr = 44100;

    void prepare (double s)
    {
        sr = s; int n = 1; while ((double) n < s / 10.0) n <<= 1;
        buf.assign ((size_t) n, 0.f); mask = n - 1; w = 0; lp = 0;
    }
    void update (float f, float dec, float dmp, float ex) { freq = f; decay = dec; damp = dmp; exc = ex; }

    inline float tick (float excite)
    {
        const float len = std::clamp ((float) sr / std::max (freq, 10.f), 2.f, (float) mask - 2.f);
        const float rp = (float) w - len;
        const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
        const float d = buf[(size_t) (i0 & mask)] * (1.f - fr) + buf[(size_t) ((i0 + 1) & mask)] * fr;
        lp += (1.f - damp * 0.92f) * (d - lp);
        const float y = std::tanh (lp * (0.9f + 0.115f * decay) + excite * exc + rng.bi() * 1e-5f);
        buf[(size_t) w] = y; w = (w + 1) & mask;
        return y;
    }
};

//==============================================================================
struct SampleData
{
    std::vector<float> data;
    double sr = 44100;
    juce::String name;
};

struct SampleEngine
{
    struct Grain { bool on = false; double pos = 0, inc = 1; int age = 0, len = 1; };
    static constexpr int kGrains = 48;

    Rng rng { 99 }; double sr = 44100;
    // block params
    int mode = 0; double rate = 1, startS = 0, lenS = 1; bool rev = false;
    float gSize = 60, gDens = 25, gScatPos = 0.1f, gScatPitch = 0, gRev = 0, startN = 0;
    float xRate = 6, xSlice = 80, xChance = 0.5f, xRev = 0.2f, xStop = 0.1f;
    float wPos = 0, wWarp = 0, wFM = 0, wRing = 0, wFreq = 110.f, pitchMul = 1.f;
    // state
    double pos = 0; Grain grains[kGrains]; double spawnAcc = 0;
    double clk = 0; bool repeating = false, glitchRev = false; double repStart = 0, repPos = 0; float speed = 1.f, speedTarget = 1.f;
    double wph = 0;

    void prepare (double s) { sr = s; }

    void update (const SampleData& sd, const Frame& f, float noteRatio, float noteFreq, float rootFreq)
    {
        const float n = (float) std::max<size_t> (sd.data.size(), 1);
        mode = I (f, p::sMode); rev = f[p::sRev] > 0.5f;
        const bool key = f[p::sKey] > 0.5f;
        pitchMul = std::exp2 (f[p::sPitch] / 12.f) * (key ? noteRatio : 1.f);
        rate = sd.sr / sr * pitchMul;
        startN = f[p::sStart];
        startS = (double) (f[p::sStart] * (n - 2.f));
        lenS = std::max (4.0, (double) (f[p::sLen] * n));
        if (startS + lenS > n - 1) lenS = std::max (4.0, (double) n - 1.0 - startS);
        gSize = f[p::gSize]; gDens = f[p::gDens]; gScatPos = f[p::gScatPos]; gScatPitch = f[p::gScatPitch]; gRev = f[p::gRev];
        xRate = f[p::xRate]; xSlice = f[p::xSlice]; xChance = f[p::xChance]; xRev = f[p::xRev]; xStop = f[p::xStop];
        wPos = f[p::wPos]; wWarp = f[p::wWarp]; wFM = f[p::wFM]; wRing = f[p::wRing];
        wFreq = (key ? noteFreq : rootFreq) * std::exp2 (f[p::sPitch] / 12.f);
    }

    static inline float readLin (const float* d, int n, double pos)
    {
        double p0 = std::fmod (pos, (double) n); if (p0 < 0) p0 += n;
        const int i0 = (int) p0; const float fr = (float) (p0 - i0);
        const int i1 = i0 + 1 >= n ? 0 : i0 + 1;
        return d[i0] + fr * (d[i1] - d[i0]);
    }

    inline float tick (const SampleData& sd, float fmIn, float ringIn)
    {
        const int N = (int) sd.data.size(); if (N < 8) return 0.f;
        const float* d = sd.data.data();
        switch (mode)
        {
            case 0:
            {
                const double a = startS, b = startS + lenS;
                pos += rev ? -rate : rate;
                if (pos >= b) pos = a + std::fmod (pos - a, b - a);
                if (pos < a) pos = b - std::fmod (a - pos, b - a);
                return readLin (d, N, pos);
            }
            case 1:
            {
                spawnAcc += (double) gDens / sr;
                int guard = 0;
                while (spawnAcc >= 1.0 && guard++ < 8)
                {
                    spawnAcc -= 1.0 - 0.0 * rng.uni();
                    for (auto& g : grains)
                        if (! g.on)
                        {
                            g.on = true; g.age = 0; g.len = std::max (8, (int) (gSize * 0.001f * sr));
                            const float p01 = std::clamp (startN + (rng.uni() - 0.5f) * gScatPos * 2.f, 0.f, 1.f);
                            g.pos = (double) p01 * (N - 1);
                            g.inc = rate * std::exp2 (rng.bi() * gScatPitch / 12.f) * (rng.uni() < gRev ? -1.0 : 1.0) * (rev ? -1.0 : 1.0);
                            break;
                        }
                }
                float sum = 0.f;
                for (auto& g : grains)
                    if (g.on)
                    {
                        const float ph = (float) g.age / (float) g.len;
                        const float wv = std::sin (kPi * ph);
                        sum += readLin (d, N, g.pos) * wv * wv;
                        g.pos += g.inc; if (++g.age >= g.len) g.on = false;
                    }
                return sum / std::sqrt (std::max (1.f, gDens * gSize * 0.001f));
            }
            case 2:
            {
                clk += (double) xRate / sr;
                if (clk >= 1.0)
                {
                    clk -= 1.0;
                    if (rng.uni() < xChance) { repeating = true; repStart = pos; repPos = 0; glitchRev = rng.uni() < xRev; }
                    else
                    {
                        repeating = false; glitchRev = rng.uni() < xRev * 0.5f;
                        if (rng.uni() < 0.35f) pos = (double) rng.uni() * (N - 1);
                    }
                    speedTarget = rng.uni() < xStop ? 0.f : 1.f;
                }
                speed += (speedTarget - speed) * (speedTarget < 0.5f ? 0.0004f : 0.004f);
                const double adv = rate * (double) speed;
                const double sliceS = std::max (4.0, (double) xSlice * 0.001 * sd.sr);
                float o;
                if (repeating)
                {
                    repPos += adv; if (repPos >= sliceS) repPos = std::fmod (repPos, sliceS);
                    o = readLin (d, N, repStart + (glitchRev ? sliceS - repPos : repPos));
                    pos += rev ? -adv : adv;
                }
                else { pos += (rev != glitchRev) ? -adv : adv; o = readLin (d, N, pos); }
                if (pos >= N) pos -= N;
                if (pos < 0) pos += N;
                return o;
            }
            default:
            {
                const int F = std::min (2048, N); const int nF = std::max (1, N / F);
                wph += (double) wFreq / sr + (double) (fmIn * wFM) * 0.5; wph -= std::floor (wph);
                double q = wph * (1.0 + (double) wWarp * 7.0); q -= std::floor (q);
                const float fp = wPos * (float) (nF - 1); const int f0 = (int) fp; const float ff = fp - (float) f0;
                const int f1 = std::min (nF - 1, f0 + 1);
                const double ix = q * (F - 1); const int i0 = (int) ix; const float fr = (float) (ix - i0);
                const int i1 = std::min (F - 1, i0 + 1);
                const float a = d[f0 * F + i0] + fr * (d[f0 * F + i1] - d[f0 * F + i0]);
                const float b = d[f1 * F + i0] + fr * (d[f1 * F + i1] - d[f1 * F + i0]);
                float o = a + ff * (b - a);
                return o * (1.f - wRing + wRing * ringIn);
            }
        }
    }
};

//==============================================================================
struct Spectral
{
    static constexpr int N = 1024, HOP = 256, NB = N / 2 + 1;
    juce::dsp::FFT fft { 10 };
    std::array<float, N> win {}, inRing {}, outRing {};
    std::array<juce::dsp::Complex<float>, N> a {}, b {};
    std::array<float, NB> frozen {}, smear {}, mag {}, ph {}, nm {}, np {}, fph {};
    std::array<int, NB> perm {};
    int wp = 0, hop = 0, frames = 0; bool haveFrozen = false;
    Rng rng { 31337 };
    int mode = 0; float amt = 1, X = 0.5f, Y = 0.5f; bool hold = false;
    float norm = 1.f / 1.5f;

    Spectral() { for (int i = 0; i < N; ++i) { win[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) N); } for (int k = 0; k < NB; ++k) perm[(size_t) k] = k; }

    void update (int m, float am, float x, float y, bool h) { mode = m; amt = am; X = x; Y = y; hold = h; }

    inline float tick (float x)
    {
        inRing[(size_t) wp] = x;
        if (++hop >= HOP) { hop = 0; frame(); }
        const int rp = (wp + 1) % N;
        const float wet = outRing[(size_t) rp], dry = inRing[(size_t) rp];
        outRing[(size_t) rp] = 0.f;
        wp = (wp + 1) % N;
        return dry + amt * (wet - dry);
    }

    void frame()
    {
        for (int i = 0; i < N; ++i) a[(size_t) i] = { inRing[(size_t) ((wp + 1 + i) % N)] * win[(size_t) i], 0.f };
        fft.perform (a.data(), b.data(), false);
        for (int k = 0; k < NB; ++k) { mag[(size_t) k] = std::abs (b[(size_t) k]); ph[(size_t) k] = std::arg (b[(size_t) k]); }
        ++frames;
        switch (mode)
        {
            case 1: // freeze
                if (! hold || ! haveFrozen) { frozen = mag; fph = ph; haveFrozen = true; }
                else for (int k = 0; k < NB; ++k) fph[(size_t) k] = std::remainder (fph[(size_t) k] + kTwoPi * (float) k * (float) HOP / (float) N, kTwoPi);
                mag = frozen; ph = fph; break;
            case 2: // shuffle
            {
                const int B = 1 + (int) (X * X * 48.f), period = 1 + (int) (Y * Y * 40.f);
                if (frames % period == 0)
                {
                    const int nb = NB / B + 1, reach = 1 + (int) (X * 8.f);
                    for (int k = 0; k < NB; ++k)
                    {
                        int blk = k / B + (int) (rng.bi() * (float) reach);
                        blk = std::clamp (blk, 0, nb - 1);
                        perm[(size_t) k] = std::min (NB - 1, blk * B + k % B);
                    }
                }
                for (int k = 0; k < NB; ++k) { nm[(size_t) k] = mag[(size_t) perm[(size_t) k]]; np[(size_t) k] = ph[(size_t) perm[(size_t) k]]; }
                mag = nm; ph = np; break;
            }
            case 3: // smear
            {
                const float dec = 0.8f + 0.1995f * X;
                for (int k = 0; k < NB; ++k) { smear[(size_t) k] = std::max (mag[(size_t) k], smear[(size_t) k] * dec); mag[(size_t) k] = smear[(size_t) k]; }
                break;
            }
            case 4: // scramble
                for (int k = 0; k < NB; ++k) ph[(size_t) k] += rng.bi() * kPi * X * 2.f;
                break;
            case 5: // gate
            {
                float mx = 1e-9f; for (int k = 0; k < NB; ++k) mx = std::max (mx, mag[(size_t) k]);
                const float th = mx * X * X * 0.5f; const bool invert = Y > 0.5f;
                for (int k = 0; k < NB; ++k) if ((mag[(size_t) k] < th) != invert) mag[(size_t) k] = 0.f;
                break;
            }
            case 6: // shift / stretch
            {
                const float shift = (X - 0.5f) * 96.f, stretch = 0.5f + Y;
                nm.fill (0.f); np.fill (0.f);
                for (int k = 0; k < NB; ++k)
                {
                    const int t = (int) std::lround ((float) k * stretch + shift);
                    if (t >= 0 && t < NB) { nm[(size_t) t] += mag[(size_t) k]; np[(size_t) t] = ph[(size_t) k]; }
                }
                mag = nm; ph = np; break;
            }
            case 7: // robot
                for (int k = 0; k < NB; ++k) ph[(size_t) k] = 0.f;
                break;
            case 8: // comb
            {
                const float period = 2.f + X * 62.f;
                for (int k = 0; k < NB; ++k) mag[(size_t) k] *= 1.f - Y * (0.5f - 0.5f * std::cos (kTwoPi * (float) k / period));
                break;
            }
            default: break;
        }
        for (int k = 0; k < NB; ++k) b[(size_t) k] = std::polar (mag[(size_t) k], ph[(size_t) k]);
        b[0] = { b[0].real(), 0.f }; b[N / 2] = { b[N / 2].real(), 0.f };
        for (int k = 1; k < N / 2; ++k) b[(size_t) (N - k)] = std::conj (b[(size_t) k]);
        fft.perform (b.data(), a.data(), true);
        for (int i = 0; i < N; ++i)
            outRing[(size_t) ((wp + 1 + i) % N)] += a[(size_t) i].real() * win[(size_t) i] * norm;
    }
};

//==============================================================================
struct DistSlot
{
    int type = 0; float g = 1, drive = 0, X = 0.5f, mix = 1; double sr = 44100;
    Tilt tone; DcBlock dc; Rng rng { 2468 };
    // state
    float held = 0, hAcc = 0, lp = 0, slew = 0, carPh = 0; std::vector<float> comb; int cw = 0, cmask = 0; float cfb = 0;
    float carrierInc = 0; float period = 1.f; int chaosIter = 1; float bits = 16.f;

    void prepare (double s)
    {
        sr = s; int n = 1; while ((double) n < s * 0.05) n <<= 1; comb.assign ((size_t) n, 0.f); cmask = n - 1; cw = 0;
    }

    void update (int t, float d, float x, float tn, float m, double s)
    {
        type = t; drive = d; X = x; mix = m; sr = s; tone.set (tn, s);
        g = 1.f + 249.f * d * d;
        switch (type)
        {
            case 5: bits = 16.f - 14.5f * d; break;
            case 6: period = 1.f + d * d * 220.f; break;
            case 9: carrierInc = 20.f * std::pow (400.f, x) / (float) s; break;
            case 10: period = std::clamp (0.05e-3f * std::pow (400.f, x) * (float) s, 2.f, (float) cmask - 2.f); cfb = 0.2f + 0.8f * d; break;
            case 11: carrierInc = 20.f * std::pow (400.f, x) / (float) s; break;
            case 12: chaosIter = 1 + (int) (x * 7.f); break;
            default: break;
        }
    }

    static inline float fold (float v) { return 4.f * std::abs (0.25f * v + 0.25f - std::floor (0.25f * v + 0.75f)) - 1.f; }

    inline float tick (float in, const std::atomic<float>* curve)
    {
        float y = in;
        switch (type)
        {
            case 0: { const float bias = X * 0.8f; y = std::tanh (g * in + bias) - std::tanh (bias); break; }
            case 1:
            {
                const float v = std::tanh (g * in * 0.7f);
                lp += (0.15f + 0.85f * (1.f - X)) * (v - lp); y = lp * 1.2f; break;
            }
            case 2:
            {
                const float v = g * in;
                y = v >= 0.f ? 1.f - std::exp (-v) : -(1.f - std::exp (v * (0.2f + 0.8f * (1.f - X)))); break;
            }
            case 3: { const float t = 1.f - X * 0.9f; y = std::clamp (g * in, -t, t) / t; break; }
            case 4:
            {
                const float v = g * in; const float th = X * 0.1f;
                y = std::abs (v) < th ? 0.f : std::tanh (8.f * v); break;
            }
            case 5:
            {
                const float lv = std::exp2 (bits);
                y = std::round ((in + rng.bi() * X * 2.f / lv) * lv * 0.5f) / (lv * 0.5f); break;
            }
            case 6:
            {
                hAcc += 1.f / (period * (1.f + X * rng.uni()));
                if (hAcc >= 1.f) { hAcc -= std::floor (hAcc); held = in; }
                y = held; break;
            }
            case 7: { const float v = in * (1.f + g * 0.3f); y = X * fold (v) + (1.f - X) * std::sin (v * kPi * 0.5f); break; }
            case 8: { const float v = g * in + X; y = v - 2.f * std::floor ((v + 1.f) * 0.5f); break; }
            case 9:
            {
                carPh += carrierInc; carPh -= std::floor (carPh);
                const float c = std::sin (kTwoPi * carPh); y = in * (1.f - drive + drive * c); break;
            }
            case 10:
            {
                const float rp = (float) cw - period; const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
                const float d = comb[(size_t) (i0 & cmask)] * (1.f - fr) + comb[(size_t) ((i0 + 1) & cmask)] * fr;
                y = std::tanh (in + d * cfb * 1.05f); comb[(size_t) cw] = y; cw = (cw + 1) & cmask; break;
            }
            case 11:
            {
                carPh += carrierInc; carPh -= std::floor (carPh);
                y = std::sin (kTwoPi * carPh + drive * 10.f * in); break;
            }
            case 12:
            {
                float u = (std::tanh (in * (1.f + 3.f * drive)) + 1.f) * 0.5f;
                for (int i = 0; i < chaosIter; ++i) u = 3.99f * u * (1.f - u);
                y = 2.f * u - 1.f; break;
            }
            case 13: { const float v = std::tanh (g * in * 0.5f); y = v + X * (std::abs (v) - v); break; }
            case 14:
            {
                const float v = std::clamp (std::tanh (in * (1.f + 4.f * drive)), -0.9999f, 0.9999f);
                const float n = 2.f + X * 12.f; const int n0 = (int) n; const float fr = n - (float) n0;
                const float th = std::acos (v);
                y = (1.f - fr) * std::cos ((float) n0 * th) + fr * std::cos ((float) (n0 + 1) * th); break;
            }
            case 15:
            {
                const float v = std::clamp (std::tanh (in * (0.5f + 5.f * drive)) + (X - 0.5f), -1.f, 1.f); // drive pushes the signal across the curve, X offsets it
                const float u = (v * 0.5f + 0.5f) * (float) (kCurvePoints - 1);
                const int i0 = std::min (kCurvePoints - 2, (int) u); const float fr = u - (float) i0;
                const float c0 = curve[i0].load (std::memory_order_relaxed), c1 = curve[i0 + 1].load (std::memory_order_relaxed);
                y = c0 + fr * (c1 - c0); break;
            }
            default:
            {
                const float s = 0.0005f + (1.f - drive) * (1.f - drive) * 0.5f;
                slew += std::clamp (std::clamp (in * (1.f + X * 8.f), -2.f, 2.f) - slew, -s, s); y = slew; break;
            }
        }
        y = tone.tick (finiteOr (y));
        y = dc.tick (y);
        return in + mix * (y - in);
    }
};

//==============================================================================
// One complete signal chain (sources -> spectral -> distortion/feedback -> filter)
struct Voice
{
    NoiseGen noise; ChaosOsc chaos; ClassicOsc osc; Resonator res; SampleEngine samp; Spectral spec;
    DistSlot slots[3]; Tilt fbTone; juce::dsp::StateVariableTPTFilter<float> filt; Rng rng { 5 };
    std::vector<float> fb; int fbMask = 0, fbW = 0; double sr = 44100;
    float fbAmt = 0, fbDelay = 10.f; int ftype = 0; float resonBurst = 0.f; int sBlock = 0;
    bool slotOn[3] {}; int specMode = 0; float inLevel = 0;
    float lvlN = 0, lvlC = 0, lvlO = 0, lvlR = 0, lvlS = 0;
    int smode = 0; float fmDepthWt = 0;

    void prepare (double s, int seed, int maxBlock)
    {
        sr = s;
        noise.rng = Rng ((uint32_t) (12345 + seed * 7919)); chaos.rng = Rng ((uint32_t) (777 + seed * 104729));
        chaos.x = 0.3f + 0.01f * (float) seed; res.rng = Rng ((uint32_t) (4242 + seed * 31)); samp.rng = Rng ((uint32_t) (99 + seed * 17));
        spec.rng = Rng ((uint32_t) (31337 + seed * 3)); rng = Rng ((uint32_t) (5 + seed));
        res.prepare (s); samp.prepare (s);
        for (auto& sl : slots) sl.prepare (s);
        int n = 1; while ((double) n < s * 0.1) n <<= 1; fb.assign ((size_t) n, 0.f); fbMask = n - 1; fbW = 0;
        filt.prepare ({ s, (juce::uint32) maxBlock, 1 });
    }

    void trigger() { }

    void update (const Frame& f, float noteFreq, float rootFreq, const SampleData& sd)
    {
        const float ratio = noteFreq / rootFreq;
        noise.update (I (f, p::nType), f[p::nColor], sr);
        chaos.update (I (f, p::cType), f[p::cRate] * ratio, f[p::cAmt], sr);
        osc.update (I (f, p::oWave), noteFreq * std::exp2 (f[p::oTune] / 12.f), f[p::oDetune], f[p::oRatio], f[p::oFM], f[p::oSync] > 0.5f, f[p::oRing], sr);
        res.update (noteFreq, f[p::rDecay], f[p::rDamp], f[p::rExcite]);
        samp.update (sd, f, ratio, noteFreq, rootFreq);
        lvlN = f[p::nLevel]; lvlC = f[p::cLevel]; lvlO = f[p::oLevel]; lvlR = f[p::rLevel]; lvlS = f[p::sLevel];
        smode = I (f, p::sMode); inLevel = f[p::inLevel];
        specMode = I (f, p::spMode);
        spec.update (specMode, f[p::spAmt], f[p::spX], f[p::spY], f[p::spHold] > 0.5f);
        const int on[3] = { p::d1On, p::d2On, p::d3On };
        for (int i = 0; i < 3; ++i)
        {
            const int b = p::d1On + i * 6;
            slotOn[i] = f[(size_t) on[i]] > 0.5f;
            slots[i].update (I (f, b + 1), f[(size_t) b + 2], f[(size_t) b + 3], f[(size_t) b + 4], f[(size_t) b + 5], sr);
        }
        fbAmt = f[p::fbAmt] * 1.2f;
        fbDelay = std::max (1.f, 0.02e-3f * std::pow (2500.f, f[p::fbTime]) * (float) sr);
        fbTone.set (f[p::fbTone], sr);
        ftype = I (f, p::fType);
        if (ftype > 0)
        {
            using T = juce::dsp::StateVariableTPTFilterType;
            filt.setType (ftype == 1 ? T::lowpass : ftype == 2 ? T::highpass : T::bandpass);
            filt.setCutoffFrequency (std::min (f[p::fCut], (float) sr * 0.45f));
            filt.setResonance (0.5f + f[p::fRes] * f[p::fRes] * 25.f);
        }
    }

    // in: audio input (may be null). env: per-sample amp envelope. out: mono result
    void process (const float* in, const float* env, float* out, int n, const SampleData& sd,
                  const std::atomic<float>* curve)
    {
        for (int i = 0; i < n; ++i)
        {
            const float nz = lvlN > 0.f ? noise.tick() : 0.f;
            const float ch = lvlC > 0.f ? chaos.tick() : 0.f;
            const float os = (lvlO > 0.f || smode == 3) ? osc.tick() : 0.f;
            float rs = 0.f;
            if (lvlR > 0.f) rs = res.tick ((lvlN > 0.f ? nz : rng.bi()) * env[i]);
            float sm = 0.f;
            if (lvlS > 0.f) sm = samp.tick (sd, std::clamp (nz * 0.5f + os + ch * 0.5f, -1.5f, 1.5f), os);

            float x = (nz * lvlN + ch * lvlC + os * lvlO + rs * lvlR + sm * lvlS) * env[i];
            if (in != nullptr) x += in[i] * inLevel;
            if (specMode > 0) x = spec.tick (x);

            if (fbAmt > 0.f)
            {
                const float rp = (float) fbW - fbDelay; const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
                const float d = fb[(size_t) (i0 & fbMask)] * (1.f - fr) + fb[(size_t) ((i0 + 1) & fbMask)] * fr;
                x += fbTone.tick (std::tanh (d)) * fbAmt + rng.bi() * 1e-4f;
            }
            for (int s = 0; s < 3; ++s) if (slotOn[s]) x = slots[s].tick (x, curve);
            x = finiteOr (x);
            if (std::abs (x) > 64.f) x = std::copysign (64.f, x);
            fb[(size_t) fbW] = x; fbW = (fbW + 1) & fbMask;
            if (ftype > 0) x = finiteOr (filt.processSample (0, x));
            out[i] = x;
        }
    }
};
}
