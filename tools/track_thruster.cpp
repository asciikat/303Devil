// "Thruster" - a full-length mid-90s style acid techno track rendered offline.
//
// v2 "Detroit tension" pass: string-machine pad through an ensemble chorus playing a minor
// progression (Dm9 - Bbmaj7 - Gm9 - A7b9) over the 303's D pedal, so the A7b9 rubs against
// the bass every 8 bars and hangs for 4 bars before the drop. Noise risers and dead stops
// before each drop, straight 16th 909 hats, tribal parts pulled back, 138 BPM.
//
// Inspired by the Peacefrog-era Planetary Assault Systems sound (driving 909-style kick,
// loopy tribal toms and congas, one relentless acid line), but written from scratch:
// own patterns, own arrangement, every sound synthesised here or by the 303 engine.
//
// Old-school moves used on purpose:
//   - two 303s (saw lead + square counter-line), pattern changes on the bar like a real 303
//   - hand-ridden CUTOFF / RESO / ENV MOD / DECAY curves (what you'd do on the knobs live)
//   - dotted-eighth tape-style delay throws at the end of phrases
//   - shuffle (~54%) shared by drums and 303 so everything locks
//   - kick drop the bar before the drop, snare roll build, mute-in / mute-out arranging
//   - a minor-7th chord stab dubbed out through delay + reverb in the break
//   - console-style saturation on the master bus and a kick-keyed duck on the music
//
//   g++ -std=c++17 -O2 -I Source tools/track_thruster.cpp -o build/track_thruster
//   ./build/track_thruster renders/thruster.wav

#include "dsp/AcidEngine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr int    kFs     = 44100;
constexpr double kBpm    = 138.0;
constexpr int    kBars   = 152;
constexpr double kSwing  = 0.54;   // drums and 303 share this
constexpr double kTwoPi  = 6.283185307179586;
constexpr double kSamplesPerBeat = 60.0 / kBpm * kFs;

using Buf = std::vector<float>;

struct Stereo
{
    Buf l, r;
    explicit Stereo (size_t n) : l (n, 0.0f), r (n, 0.0f) {}
};

// Sample position of a 16th step, swing applied to every second 16th
double stepTime (int64_t absStep)
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

std::mt19937 rng (303);
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
double metal (double t)
{
    static const double f[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
    double s = 0;
    for (double fr : f)
        s += std::fmod (t * fr * 1.7, 1.0) < 0.5 ? 1.0 : -1.0;
    return s / 6.0;
}

Buf makeHat (double decay, double len, int seed)
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
Buf makeDrumHead (double baseHz, double bend, double decay, double slap)
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

Buf makeShaker()
{
    rng.seed (11);
    Buf b ((size_t) (0.09 * kFs));
    Svf bp; bp.set (6500, 0.9);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        const double env = (t < 0.012 ? t / 0.012 : std::exp (-(t - 0.012) * 55.0));
        double lo, band, hi;
        bp.tick (noise(), lo, band, hi);
        b[i] = (float) (band * env * 0.9);
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
void addChord (Stereo& out, const std::vector<int>& notes, double at, double lenSamples, double gain, double cutoffHz)
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
void ensemble (Stereo& s)
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
void addRiser (Stereo& out, double from, double to, double gain)
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

void addHit (Stereo& out, const Buf& s, double at, double gain, double pan)
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
double smooth (double x) { x = std::clamp (x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }

// Value of a knob over the song: linear ramps between (bar, value) points, eased
double ride (double bar, std::initializer_list<std::pair<double, double>> pts)
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

bool in (int bar, int a, int b) { return bar >= a && bar < b; }

// x = hit, X = accent, . = rest (16 chars per bar)
bool hit (const char* p, int s)    { return p[s] == 'x' || p[s] == 'X'; }
double vel (const char* p, int s)  { return p[s] == 'X' ? 1.0 : 0.72; }

void writeWav (const std::string& path, const Stereo& s)
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
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    const std::string outPath = argc > 1 ? argv[1] : "thruster.wav";
    const double barSamples = 4.0 * kSamplesPerBeat;
    const size_t total = (size_t) (kBars * barSamples + 4.0 * kFs); // + tail
    std::printf ("Thruster: %d bars at %.0f BPM = %.0f s\n", kBars, kBpm, kBars * barSamples / kFs);

    //--------------------------------------------------------------------------
    // Drums and percussion
    Stereo drums (total), perc (total), stabBus (total), padBus (total), riserBus (total);

    const Buf kick = makeKick(), clap = makeClap(), snare = makeSnare(), ride_ = makeRide();
    const Buf rim = makeRim(), shaker = makeShaker(), stab = makeStab();
    const Buf ch[3] = { makeHat (0.018, 0.08, 1), makeHat (0.022, 0.08, 2), makeHat (0.016, 0.08, 3) };
    const Buf oh = makeHat (0.16, 0.6, 9);
    const Buf tomLo = makeDrumHead (92, 0.55, 0.22, 0.25), tomMid = makeDrumHead (138, 0.5, 0.18, 0.3);
    const Buf congaHi = makeDrumHead (330, 0.25, 0.07, 0.9), congaLo = makeDrumHead (220, 0.3, 0.09, 0.8);

    //                 0123456789abcdef
    const char* kickP  = "X...X...X...X...";
    const char* kickFill = "X...X...X...X.x.";
    const char* chP    = "x.xXx.xXx.xXx.xX";
    const char* chP16  = "xxXxxxXxxxXxxxXx";   // Detroit: straight 16ths, accent on the "and"
    const char* ohP    = "..x...x...x...x.";
    const char* clapP  = "....X.......X...";
    const char* rideP  = "x.x.x.x.x.x.x.x.";
    const char* rimP   = "...x..x....x.x..";
    const char* shkP   = "xxXxxxXxxxXxxxXx";
    // The tribal engine: toms answer each other across two bars
    const char* tomA   = "..x..X....x..x..";
    const char* tomB   = "X.....x.x..X....";
    const char* congaA = ".x.xX..x.xx.X..x";
    const char* congaB = "x..xx.X..x.xX.x.";

    std::vector<double> kickTimes;

    for (int bar = 0; bar < kBars; ++bar)
    {
        const bool phraseEnd = bar % 8 == 7;
        const bool kickOn   = ! in (bar, 72, 88) && bar != 87 && ! in (bar, 150, kBars) && bar != 39 && bar != 119;
        const bool hatOn    = in (bar, 4, 72) || in (bar, 80, 150);
        const bool ohOn     = in (bar, 24, 64) || in (bar, 88, 136);
        const bool clapOn   = in (bar, 16, 72) || in (bar, 88, 136);
        const bool rideOn   = in (bar, 48, 64) || in (bar, 96, 128);
        const bool tomOn    = in (bar, 8, 140);
        const bool congaOn  = in (bar, 32, 64);
        const bool rimOn    = in (bar, 40, 64) || in (bar, 104, 128);
        const bool shkOn    = in (bar, 12, 140);
        const bool rollOn   = in (bar, 84, 88);

        for (int s = 0; s < 16; ++s)
        {
            const int64_t abs = (int64_t) bar * 16 + s;
            const double t = stepTime (abs);

            if (kickOn && hit (phraseEnd ? kickFill : kickP, s))
            {
                addHit (drums, kick, t, 0.72 * vel (phraseEnd ? kickFill : kickP, s), 0.0);
                kickTimes.push_back (t);
            }
            const char* hp = bar >= 40 ? chP16 : chP;
            if (hatOn && hit (hp, s))
                addHit (drums, ch[abs % 3], t, 0.3 * vel (hp, s) * (hp[s] == 'X' ? 1.0 : 0.8), 0.15);
            if (ohOn && hit (ohP, s))
                addHit (drums, oh, t, 0.2, -0.1);
            if (clapOn && hit (clapP, s))
                addHit (drums, clap, t, 0.42, 0.0);
            if (rideOn && hit (rideP, s))
                addHit (drums, ride_, t, 0.13, 0.35);
            if (rimOn && hit (rimP, s))
                addHit (perc, rim, t, 0.25, -0.55);
            if (shkOn && hit (shkP, s))
                addHit (perc, shaker, t, 0.12 * vel (shkP, s) * ride (bar, { { 12, 0.3 }, { 24, 1.0 } }), 0.5);

            if (tomOn)
            {
                const char* p = bar % 2 ? tomB : tomA;
                if (hit (p, s))
                {
                    const double g = 0.5 * vel (p, s) * ride (bar, { { 8, 0.35 }, { 16, 1.0 } });
                    addHit (perc, (s % 3 == 0) ? tomLo : tomMid, t, g, (s % 3 == 0) ? -0.3 : 0.3);
                }
            }
            if (congaOn)
            {
                const char* p = bar % 2 ? congaB : congaA;
                if (hit (p, s))
                    addHit (perc, (s % 2) ? congaHi : congaLo, t, 0.3 * vel (p, s), (s % 2) ? 0.6 : -0.45);
            }

            // Snare roll build into the drop: 8ths -> 16ths -> 32nds, rising
            if (rollOn)
            {
                const int q = bar - 84;
                const double g = 0.12 + 0.1 * (q * 16 + s) / 64.0;
                if (q < 2 ? s % 2 == 0 : true)
                    addHit (drums, snare, t, g, 0.0);
                if (q == 3)
                    addHit (drums, snare, t + 0.25 * kSamplesPerBeat * 0.5, g, 0.0);
            }
        }
    }

    // Detroit strings: Dm9 - Bbmaj7 - Gm9 - A7b9, two bars each, over the D pedal.
    // Bars 84-88 hang on the A7b9 for maximum pull into the drop.
    {
        const std::vector<int> chords[4] = { { 50, 53, 57, 60, 64 }, { 46, 50, 53, 57, 62 },
                                             { 43, 50, 53, 57, 58 }, { 45, 49, 52, 55, 58 } };
        for (int bar = 32; bar < 136; bar += 2)
        {
            const bool hang = bar == 84;
            const int c = hang ? 3 : (bar / 2) % 4;
            const double len = (hang ? 4 : 2) * barSamples;
            const double g = ride (bar, { { 32, 0.05 }, { 62, 0.11 }, { 64, 0.2 }, { 86, 0.24 }, { 88, 0.12 },
                                          { 120, 0.13 }, { 134, 0.04 } });
            const double cut = ride (bar, { { 32, 500 }, { 62, 1300 }, { 64, 900 }, { 84, 2600 }, { 88, 2200 },
                                            { 120, 1900 }, { 134, 600 } });
            addChord (padBus, chords[c], stepTime ((int64_t) bar * 16), len, g, cut);
            if (hang) bar += 2;
        }
        ensemble (padBus);
    }

    // Risers into each section change
    addRiser (riserBus, stepTime (28 * 16), stepTime (32 * 16) - 0.5 * kSamplesPerBeat, 0.35);
    addRiser (riserBus, stepTime (56 * 16), stepTime (64 * 16), 0.4);
    addRiser (riserBus, stepTime (80 * 16), stepTime (88 * 16) - 0.5 * kSamplesPerBeat, 0.6);
    addRiser (riserBus, stepTime (112 * 16), stepTime (120 * 16), 0.4);

    // Chord stab: the dubbed-out break, plus a few calls in the second peak
    for (int bar = 64; bar < 88; ++bar)
        if (bar % 2 == 0)
            addHit (stabBus, stab, stepTime ((int64_t) bar * 16 + 3), in (bar, 72, 88) ? 0.5 : 0.3, 0.0);
    for (int bar = 104; bar < 128; bar += 4)
        addHit (stabBus, stab, stepTime ((int64_t) bar * 16 + 14), 0.32, 0.0);

    //--------------------------------------------------------------------------
    // The two 303s
    acid::AcidEngine lead, counter;

    // Lead: a hammering D line, the hypnotic one
    lead.patterns[0].loadFromString ("0a 0 12s 0 0 3a 0 0s 10 0a 0 7s 0 0 12a 0");
    // Second half: more slides, climbs to the minor third up top
    lead.patterns[1].loadFromString ("0a 0 12s 15 0 3a 0s 5 0 0a 12s 10 0 7a 0s 12");
    // Peak variation: octave jumps on accents
    lead.patterns[2].loadFromString ("0a 12a 0 0s 3 0a 15s 12 0 0a 10s 0 12a 0 7s 0");
    // Counter: low square, sparse, answers on the off-beats
    counter.patterns[0].loadFromString ("- - -12a - - - -5s -2 - - -12a - - 0s -12 -");
    counter.patterns[1].loadFromString ("- -12 - -12a - - -9s -7 - -12 - -12a - -5s -2 -");

    acid::EngineParams lp, cp;
    lp.clock = cp.clock = acid::EngineParams::hostSync;
    lp.root = 38; cp.root = 38;              // D
    lp.shuffle = cp.shuffle = (kSwing - 0.5) / 0.25;

    lp.voice.acidMode = true;
    lp.voice.waveform = 0;
    lp.voice.overdrive = 0.35;
    lp.voice.slideMs = 70.0;
    lp.voice.ampDecayMs = 900.0;
    lp.voice.accentDecayMs = 170.0;
    lp.voice.sub = 0.35;
    lp.voice.volumeDb = -4.0;
    lp.fx.delayBeats = 0.75;
    lp.fx.delayFeedback = 0.5;
    lp.fx.reverbSize = 0.7;

    cp.voice.acidMode = true;
    cp.voice.waveform = 1;
    cp.voice.cutoff = 0.3;
    cp.voice.resonance = 0.55;
    cp.voice.envMod = 0.45;
    cp.voice.decay = 0.3;
    cp.voice.overdrive = 0.2;
    cp.voice.volumeDb = -7.0;
    cp.fx.delayBeats = 0.75;
    cp.fx.delayFeedback = 0.35;
    cp.fx.delayMix = 0.18;

    lead.setParams (lp);   lead.prepare (kFs);
    counter.setParams (cp); counter.prepare (kFs);

    Stereo acidBus (total), counterBus (total);
    constexpr int block = 128;
    std::vector<float> bl (block), br (block);

    for (size_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<size_t> (block, total - pos);
        const double ppq = pos / kSamplesPerBeat;
        const double bar = ppq / 4.0;
        const int ibar = (int) bar;

        // --- Hand-ridden knobs on the lead ---------------------------------
        lp.voice.cutoff    = ride (bar, { { 16, 0.08 }, { 32, 0.22 }, { 48, 0.36 }, { 60, 0.5 }, { 64, 0.3 },
                                          { 76, 0.18 }, { 86, 0.42 }, { 88, 0.32 }, { 104, 0.52 }, { 112, 0.62 },
                                          { 120, 0.45 }, { 136, 0.15 }, { 144, 0.05 } });
        // Breathing on top of the long ride: a 2-bar hand wobble, the way you'd rock the knob
        lp.voice.cutoff   += 0.04 * std::sin (kTwoPi * bar / 2.0 + 0.7) * ride (bar, { { 32, 0 }, { 40, 1 }, { 136, 1 }, { 140, 0 } });
        lp.voice.resonance = ride (bar, { { 16, 0.55 }, { 48, 0.72 }, { 64, 0.8 }, { 80, 0.9 }, { 88, 0.7 },
                                          { 112, 0.85 }, { 136, 0.6 } });
        lp.voice.envMod    = ride (bar, { { 16, 0.3 }, { 48, 0.55 }, { 64, 0.7 }, { 80, 0.45 }, { 88, 0.6 },
                                          { 110, 0.85 }, { 136, 0.4 } });
        lp.voice.decay     = ride (bar, { { 16, 0.25 }, { 56, 0.45 }, { 72, 0.7 }, { 88, 0.35 }, { 112, 0.55 },
                                          { 136, 0.3 } });
        lp.voice.accent    = ride (bar, { { 16, 0.5 }, { 96, 0.85 }, { 136, 0.6 } });
        lp.voice.overdrive = ride (bar, { { 16, 0.25 }, { 88, 0.35 }, { 96, 0.6 }, { 120, 0.6 }, { 136, 0.3 } });
        lp.voice.tuneSemis = 0.0;

        // Pattern program (changes land at the pattern end, like a 303)
        lp.pattern = in (ibar, 48, 64) ? 1 : in (ibar, 96, 104) ? 1 : in (ibar, 104, 120) ? 2 : 0;
        cp.pattern = in (ibar, 104, 128) ? 1 : 0;

        // Delay throws on the last bar of each 8-bar phrase, washed reverb in the break
        const bool throwBar = ibar % 8 == 7 && ibar >= 24;
        lp.fx.delayMix  = (throwBar ? 0.45 : 0.08) + (in (ibar, 64, 88) ? 0.3 : 0.0);
        lp.fx.reverbMix = ride (bar, { { 60, 0.05 }, { 68, 0.35 }, { 86, 0.4 }, { 88, 0.06 }, { 136, 0.08 }, { 148, 0.4 } });
        lp.fx.lfoDepth  = in (ibar, 64, 88) ? 0.12 : 0.0;
        lp.fx.lfoBeats  = 32.0;

        cp.voice.cutoff = ride (bar, { { 40, 0.12 }, { 56, 0.32 }, { 64, 0.25 }, { 96, 0.28 }, { 116, 0.45 }, { 128, 0.2 } });

        lead.setParams (lp);
        counter.setParams (cp);

        acid::Transport tr;
        tr.playing = true; tr.hasPpq = true; tr.ppq = ppq; tr.bpm = kBpm;

        lead.beginBlock (tr);
        lead.render (bl.data(), br.data(), n);
        // Mute-in / mute-out arranging on the mixer, with short fades
        const double leadGain = ride (bar, { { 15.9, 0 }, { 16, 1 }, { 143.9, 1 }, { 144, 0 } });
        for (int i = 0; i < n; ++i)
        {
            acidBus.l[pos + i] = (float) (bl[i] * leadGain);
            acidBus.r[pos + i] = (float) (br[i] * leadGain);
        }

        counter.beginBlock (tr);
        counter.render (bl.data(), br.data(), n);
        const double cGain = (in (ibar, 40, 64) || in (ibar, 96, 128)) ? 1.0 : 0.0;
        for (int i = 0; i < n; ++i)
        {
            counterBus.l[pos + i] = (float) (bl[i] * cGain);
            counterBus.r[pos + i] = (float) (br[i] * cGain);
        }
    }

    //--------------------------------------------------------------------------
    // Stab and percussion sends: tape-ish dotted-eighth delay and a room
    {
        acid::PingPongDelay dly; dly.prepare (kFs);
        acid::Reverb verb; verb.prepare (kFs);
        acid::Reverb room; room.prepare (kFs);
        acid::Reverb padVerb; padVerb.prepare (kFs);
        const double d = 0.75 * kSamplesPerBeat;
        for (size_t i = 0; i < total; ++i)
        {
            const float sIn = 0.5f * (stabBus.l[i] + stabBus.r[i]);
            float dl, dr, rl, rr;
            dly.process (sIn, d, 0.62, dl, dr);
            verb.process (sIn * 0.6f + 0.4f * (dl + dr), 0.85, rl, rr);
            stabBus.l[i] += 0.8f * dl + 0.7f * rl;
            stabBus.r[i] += 0.8f * dr + 0.7f * rr;

            float vl, vr;
            padVerb.process (0.5f * (padBus.l[i] + padBus.r[i]) * 0.5f, 0.9, vl, vr);
            padBus.l[i] += vl;
            padBus.r[i] += vr;

            float pl, pr;
            room.process (0.5f * (perc.l[i] + perc.r[i]) * 0.25f, 0.35, pl, pr);
            perc.l[i] += pl;
            perc.r[i] += pr;
        }
    }

    //--------------------------------------------------------------------------
    // Mix: kick-keyed duck on everything that isn't the kick, then bus saturation
    Buf duck (total, 1.0f);
    for (double t : kickTimes)
    {
        const size_t s0 = (size_t) t;
        const size_t len = (size_t) (0.28 * kSamplesPerBeat * 2.0);
        for (size_t i = 0; i < len && s0 + i < total; ++i)
        {
            const double x = (double) i / len;
            const double g = 1.0 - 0.3 * (x < 0.05 ? x / 0.05 : std::pow (1.0 - (x - 0.05) / 0.95, 2.0));
            duck[s0 + i] = std::min (duck[s0 + i], (float) g);
        }
    }

    {
        const size_t a = (size_t) (96 * barSamples), b = (size_t) (104 * barSamples);
        auto rms = [&] (const Stereo& st) { double q = 0; for (size_t i = a; i < b; ++i) q += st.l[i] * st.l[i]; return 20 * std::log10 (std::sqrt (q / (b - a)) + 1e-9); };
        std::printf ("bus RMS (bars 96-104): drums %.1f  perc %.1f  acid %.1f  counter %.1f  stab %.1f  pad %.1f dB\n",
                     rms (drums), rms (perc), rms (acidBus), rms (counterBus), rms (stabBus), rms (padBus));
    }

    Stereo mix (total);
    OnePole dcL, dcR; dcL.setLP (15.0); dcR.setLP (15.0);
    double peak = 0, sumSq = 0;
    for (size_t i = 0; i < total; ++i)
    {
        const double dk = duck[i];
        const double music_l = 1.1 * acidBus.l[i] + 0.9 * counterBus.l[i] + 0.7 * stabBus.l[i] + 0.9 * perc.l[i] + padBus.l[i] + riserBus.l[i];
        const double music_r = 1.1 * acidBus.r[i] + 0.9 * counterBus.r[i] + 0.7 * stabBus.r[i] + 0.9 * perc.r[i] + padBus.r[i] + riserBus.r[i];
        double l = drums.l[i] * 0.85 + music_l * dk;
        double r = drums.r[i] * 0.85 + music_r * dk;

        // Dead stops: everything cut for the last half beat before bar 32 and the drop
        const double beatPos = i / kSamplesPerBeat;
        if ((beatPos >= 32 * 4 - 0.5 && beatPos < 32 * 4) || (beatPos >= 88 * 4 - 0.5 && beatPos < 88 * 4))
            l = r = 0.0;

        // Console-style saturation, gentle
        l = std::tanh (l * 1.15) / std::tanh (1.15);
        r = std::tanh (r * 1.15) / std::tanh (1.15);
        l = dcL.hp (l); r = dcR.hp (r);

        // Fade the tail
        const double tEnd = (double) (total - i) / kFs;
        const double fade = std::min (1.0, tEnd / 3.0);
        mix.l[i] = (float) (l * fade);
        mix.r[i] = (float) (r * fade);
        peak = std::max ({ peak, std::abs (l), std::abs (r) });
        sumSq += l * l + r * r;
    }

    // Normalise to -0.5 dBFS
    const double gain = std::pow (10.0, -0.5 / 20.0) / std::max (peak, 1e-6);
    for (size_t i = 0; i < total; ++i) { mix.l[i] *= (float) gain; mix.r[i] *= (float) gain; }

    const double rmsDb = 20.0 * std::log10 (std::sqrt (sumSq / (2.0 * total)) * gain);
    std::printf ("peak normalised to -0.5 dBFS, RMS %.1f dBFS\n", rmsDb);
    writeWav (outPath, mix);
    std::printf ("wrote %s\n", outPath.c_str());
    return 0;
}
