// Shared synth kit for the offline demo tracks in tools/: drum one-shots, filters,
// string-machine pad, ensemble chorus, riser, arrangement helpers and a WAV writer.
// Header-only, no JUCE. Call kit::setTempo() before using anything time-based.
#pragma once

#include "dsp/Effects.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace kit
{
constexpr int    kFs    = 44100;
constexpr double kTwoPi = 6.283185307179586;

// Tempo state shared by the helpers (set once at the start of main)
inline double kSwing = 0.5;            // 0.5 = straight, every second 16th delayed
inline double kSamplesPerBeat = 0.0;
inline void setTempo (double bpm, double swing) { kSamplesPerBeat = 60.0 / bpm * kFs; kSwing = swing; }

using Buf = std::vector<float>;

struct Stereo
{
    Buf l, r;
    explicit Stereo (size_t n) : l (n, 0.0f), r (n, 0.0f) {}
};

// Sample position of a 16th step, swing applied to every second 16th
inline double stepTime (int64_t absStep)
{
    const int64_t eighth = absStep / 2;
    const double inEighth = (absStep % 2) ? kSwing : 0.0;
    return (eighth + inEighth) * 0.5 * kSamplesPerBeat;
}

//==============================================================================
// Simple filters
struct OnePole
{
    double a = 0, z = 0;
    void setLP (double hz) { a = 1.0 - std::exp (-kTwoPi * hz / kFs); }
    double lp (double x) { z += a * (x - z); return z; }
    double hp (double x) { return x - lp (x); }
};

struct Svf  // Chamberlin-style state variable (TPT form), bandpass/lowpass/highpass
{
    double g = 0, k = 1, ic1 = 0, ic2 = 0;
    void set (double hz, double q) { g = std::tan (M_PI * std::min (hz, kFs * 0.45) / kFs); k = 1.0 / q; }
    void tick (double x, double& lp, double& bp, double& hp)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + g * a1 * v3;
        const double v2 = ic2 + g * v1;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

inline std::mt19937 rng (303);
double noise() { return std::uniform_real_distribution<double> (-1.0, 1.0) (rng); }

//==============================================================================
// Drum one-shots

Buf makeKick()
{
    // 909-ish: sine with fast pitch drop, a click, soft clipped
    Buf b ((size_t) (0.55 * kFs));
    double ph = 0;
    OnePole clickLP; clickLP.setLP (6000);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        const double f = 48.0 + 190.0 * std::exp (-t * 38.0) + 40.0 * std::exp (-t * 9.0);
        ph += kTwoPi * f / kFs;
        const double amp = std::exp (-t * 5.2) * (t < 0.002 ? t / 0.002 : 1.0);
        const double click = clickLP.lp (noise()) * std::exp (-t * 400.0) * 0.6;
        const double y = std::sin (ph) * amp * 1.6 + click;
        b[i] = (float) std::tanh (y * 1.4);
    }
    return b;
}

// 808/909 style metallic source: six detuned squares
inline double metal (double t)
{
    static const double f[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
    double s = 0;
    for (double fr : f)
        s += std::fmod (t * fr * 1.7, 1.0) < 0.5 ? 1.0 : -1.0;
    return s / 6.0;
}

inline Buf makeHat (double decay, double len, int seed)
{
    rng.seed ((unsigned) seed);
    Buf b ((size_t) (len * kFs));
    Svf bp; bp.set (9000, 1.2);
    OnePole hp; hp.setLP (7000);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        double lo, band, hi;
        bp.tick (0.55 * metal (t) + 0.45 * noise(), lo, band, hi);
        const double y = hp.hp (band + 0.4 * hi);
        b[i] = (float) (y * std::exp (-t / decay) * 1.6);
    }
    return b;
}

Buf makeRide()
{
    rng.seed (77);
    Buf b ((size_t) (1.2 * kFs));
    OnePole hp; hp.setLP (5000);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        const double y = hp.hp (0.7 * metal (t * 1.31) + 0.3 * noise());
        const double env = 0.35 * std::exp (-t * 30.0) + 0.65 * std::exp (-t * 2.6);
        b[i] = (float) (y * env * 0.8);
    }
    return b;
}

Buf makeClap()
{
    rng.seed (909);
    Buf b ((size_t) (0.45 * kFs));
    Svf bp; bp.set (1150, 1.6);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        // Three fast bursts then the tail, like the 909 clap circuit
        double env = 0;
        for (int k = 0; k < 3; ++k)
        {
            const double tk = t - k * 0.0105;
            if (tk >= 0) env = std::max (env, std::exp (-tk * 180.0));
        }
        const double tt = t - 0.021;
        if (tt >= 0) env = std::max (env, 0.8 * std::exp (-tt * 16.0));
        double lo, band, hi;
        bp.tick (noise(), lo, band, hi);
        b[i] = (float) (band * env * 2.6);
    }
    return b;
}

Buf makeSnare()
{
    rng.seed (4242);
    Buf b ((size_t) (0.25 * kFs));
    OnePole hp; hp.setLP (1800);
    double ph = 0;
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        ph += kTwoPi * (190.0 + 60.0 * std::exp (-t * 60)) / kFs;
        const double tone = std::sin (ph) * std::exp (-t * 30.0) * 0.6;
        const double nz = hp.hp (noise()) * std::exp (-t * 22.0);
        b[i] = (float) std::tanh ((tone + nz) * 1.3);
    }
    return b;
}

// Tuned membrane: toms / congas. Pitch bend down, a touch of noise for the slap.
inline Buf makeDrumHead (double baseHz, double bend, double decay, double slap)
{
    Buf b ((size_t) (decay * 6.0 * kFs));
    double ph = 0;
    Svf bp; bp.set (baseHz * 4.0, 2.0);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        const double f = baseHz * (1.0 + bend * std::exp (-t * 45.0));
        ph += kTwoPi * f / kFs;
        double lo, band, hi;
        bp.tick (noise(), lo, band, hi);
        const double y = std::sin (ph) * std::exp (-t / decay)
                       + 0.35 * std::sin (2.0 * ph + 0.4) * std::exp (-t / (decay * 0.5))
                       + slap * band * std::exp (-t * 90.0);
        b[i] = (float) std::tanh (y * 1.2);
    }
    return b;
}

Buf makeRim()
{
    Buf b ((size_t) (0.08 * kFs));
    Svf bp; bp.set (1700, 6.0);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        double lo, band, hi;
        const double x = (i < 20 ? 1.0 : 0.0) + 0.3 * std::sin (kTwoPi * 820 * t) * std::exp (-t * 200);
        bp.tick (x, lo, band, hi);
        b[i] = (float) (band * 3.0 * std::exp (-t * 60.0));
    }
    return b;
}

// Minor 7th stab (D F A C), detuned saws through an enveloped lowpass
Buf makeStab()
{
    Buf b ((size_t) (0.6 * kFs));
    const double notes[] = { 62, 65, 69, 72, 50 };
    double ph[10] = {};
    Svf lp; double lo, band, hi;
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        double s = 0;
        int k = 0;
        for (double n : notes)
            for (double det : { -0.08, 0.08 })
            {
                const double f = 440.0 * std::pow (2.0, (n + det - 69.0) / 12.0);
                ph[k] = std::fmod (ph[k] + f / kFs, 1.0);
                s += 2.0 * ph[k] - 1.0;
                ++k;
            }
        lp.set (300.0 + 5200.0 * std::exp (-t * 14.0), 1.4);
        lp.tick (s / 10.0, lo, band, hi);
        b[i] = (float) (lo * std::exp (-t * 6.0) * 1.8);
    }
    return b;
}

// Detroit string machine: three detuned saws per note, slow attack, lowpassed.
// The ensemble chorus is added later on the whole pad bus.
inline void addChord (Stereo& out, const std::vector<int>& notes, double at, double lenSamples, double gain, double cutoffHz)
{
    const double attack = 0.3 * kFs, release = 1.0 * kFs;
    const size_t n = (size_t) (lenSamples + release);
    const size_t start = (size_t) std::lround (at);
    for (size_t vi = 0; vi < notes.size(); ++vi)
    {
        double ph[3] = { 0.13 * vi, 0.37 + 0.11 * vi, 0.71 + 0.05 * vi };
        const double det[3] = { -0.11, 0.0, 0.12 };
        Svf lp; lp.set (cutoffHz, 0.75);
        const double pan = notes.size() > 1 ? (double) vi / (notes.size() - 1) * 1.2 - 0.6 : 0.0;
        const double gl = std::cos ((pan + 1.0) * M_PI * 0.25) * 1.414, gr = std::sin ((pan + 1.0) * M_PI * 0.25) * 1.414;
        double f[3];
        for (int k = 0; k < 3; ++k) f[k] = 440.0 * std::pow (2.0, (notes[vi] + det[k] - 69.0) / 12.0);
        for (size_t i = 0; i < n && start + i < out.l.size(); ++i)
        {
            double env = i < attack ? i / attack : 1.0;
            if (i > lenSamples) env *= std::exp (-(i - lenSamples) / (0.3 * kFs));
            double sum = 0;
            for (int k = 0; k < 3; ++k) { ph[k] = std::fmod (ph[k] + f[k] / kFs, 1.0); sum += 2.0 * ph[k] - 1.0; }
            double lo, band, hi;
            lp.tick (sum / 3.0, lo, band, hi);
            const double y = lo * env * gain;
            out.l[start + i] += (float) (y * gl);
            out.r[start + i] += (float) (y * gr);
        }
    }
}

// String-ensemble chorus: three taps on slow + fast modulated delays
inline void ensemble (Stereo& s)
{
    const size_t n = s.l.size();
    Buf mono (n);
    for (size_t i = 0; i < n; ++i) mono[i] = 0.5f * (s.l[i] + s.r[i]);
    auto read = [&] (double idx) {
        if (idx < 0) return 0.0;
        const size_t a = (size_t) idx; const double fr = idx - a;
        return a + 1 < n ? mono[a] * (1 - fr) + mono[a + 1] * fr : 0.0;
    };
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kFs;
        double tap[3];
        for (int k = 0; k < 3; ++k)
        {
            const double d = 0.012 + 0.004 * std::sin (kTwoPi * 0.63 * t + k * 2.094) + 0.0006 * std::sin (kTwoPi * 6.1 * t + k * 2.094);
            tap[k] = read (i - d * kFs);
        }
        s.l[i] = (float) (0.4 * s.l[i] + 0.6 * (0.8 * tap[0] + 0.4 * tap[1]));
        s.r[i] = (float) (0.4 * s.r[i] + 0.6 * (0.8 * tap[2] + 0.4 * tap[1]));
    }
}

// Noise riser: resonant band sweeping up, growing, chopped into 16ths at the very end
inline void addRiser (Stereo& out, double from, double to, double gain)
{
    rng.seed (55);
    Svf bp;
    double ph = 0;
    const size_t a = (size_t) from, b = std::min ((size_t) to, out.l.size());
    for (size_t i = a; i < b; ++i)
    {
        const double x = (double) (i - a) / (b - a);
        bp.set (250.0 * std::pow (36.0, x), 2.5);
        double lo, band, hi;
        bp.tick (noise(), lo, band, hi);
        ph += kTwoPi * (90.0 + 900.0 * x * x) / kFs;
        double y = (band + 0.25 * std::sin (ph)) * std::pow (x, 2.2) * gain;
        if (x > 0.75)
        {
            const double sixteenth = std::fmod ((double) i / (kSamplesPerBeat * 0.25), 1.0);
            y *= sixteenth < 0.55 ? 1.0 : 0.0;
        }
        const double pan = 0.6 * std::sin (kTwoPi * x * 6.0);
        out.l[i] += (float) (y * (1.0 - pan));
        out.r[i] += (float) (y * (1.0 + pan));
    }
}

inline void addHit (Stereo& out, const Buf& s, double at, double gain, double pan)
{
    const size_t start = (size_t) std::lround (at);
    const double gl = gain * std::cos ((pan + 1.0) * M_PI * 0.25) * 1.414;
    const double gr = gain * std::sin ((pan + 1.0) * M_PI * 0.25) * 1.414;
    for (size_t i = 0; i < s.size() && start + i < out.l.size(); ++i)
    {
        out.l[start + i] += (float) (s[i] * gl);
        out.r[start + i] += (float) (s[i] * gr);
    }
}

//==============================================================================
// Arrangement helpers
inline double smooth (double x) { x = std::clamp (x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }

// Value of a knob over the song: linear ramps between (bar, value) points, eased
inline double ride (double bar, std::initializer_list<std::pair<double, double>> pts)
{
    auto it = pts.begin();
    if (bar <= it->first) return it->second;
    auto prev = *it;
    for (++it; it != pts.end(); ++it)
    {
        if (bar <= it->first)
            return prev.second + (it->second - prev.second) * smooth ((bar - prev.first) / (it->first - prev.first));
        prev = *it;
    }
    return prev.second;
}

inline bool in (int bar, int a, int b) { return bar >= a && bar < b; }

// x = hit, X = accent, . = rest (16 chars per bar)
inline bool hit (const char* p, int s)    { return p[s] == 'x' || p[s] == 'X'; }
inline double vel (const char* p, int s)  { return p[s] == 'X' ? 1.0 : 0.72; }

inline void writeWav (const std::string& path, const Stereo& s)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("could not write %s\n", path.c_str()); return; }
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t bytes = (uint32_t) s.l.size() * 4;
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + bytes);
    std::fwrite ("WAVEfmt ", 1, 8, f); u32 (16); u16 (1); u16 (2);
    u32 (kFs); u32 (kFs * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (bytes);
    std::minstd_rand d (1);
    for (size_t i = 0; i < s.l.size(); ++i)
        for (float v : { s.l[i], s.r[i] })
        {
            const double dither = ((double) d() / d.max() - (double) d() / d.max()) / 32768.0;
            u16 ((uint16_t) (int16_t) std::lround (std::clamp (v + dither, -1.0, 1.0) * 32767.0));
        }
    std::fclose (f);
}
} // namespace kit
