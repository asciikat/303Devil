// "Woodward Nights" - an 80s Detroit style warehouse track rendered offline. No 303.
//
// Built for a crowd: 126 BPM, a shuffled 808/909-style kit with a cowbell, a deep
// hypnotic synth bass that slowly breathes its filter, and the old-school pads that
// carry the emotion: a string machine and a Juno-style PWM pad playing
// Dm9 - Bbmaj7 - Gm9 - A7b9, a syncopated poly stab riff and an FM bell hook.
//
// The bass works two ways on purpose: a pedal on D (hypnotic, tension) and following
// the chord roots (release). Sections switch between them, and the break parks the
// bass on A under the A7b9 for four bars before the drop.
//
// Every pattern, sound and arrangement choice is original.
//
//   g++ -std=c++17 -O2 -I Source tools/track_woodward.cpp -o build/track_woodward
//   ./build/track_woodward renders/woodward.wav

#include "synth_kit.h"

#include <array>
#include <functional>
#include <map>

using namespace kit;

namespace
{
constexpr double kBpm  = 126.0;
constexpr int    kBars = 144;

double mtof (double n) { return 440.0 * std::pow (2.0, (n - 69.0) / 12.0); }

// The progression, two bars per chord
const std::vector<int> kChords[4] = { { 50, 53, 57, 60, 64 },   // Dm9
                                      { 46, 50, 53, 57, 62 },   // Bbmaj7
                                      { 43, 50, 53, 57, 58 },   // Gm9
                                      { 45, 49, 52, 55, 58 } }; // A7b9
const int kBassRoot[4] = { 38, 34, 31, 33 };                    // D2 Bb1 G1 A1

int chordAt (int bar)
{
    if (bar >= 76 && bar < 80) return 3;   // the hang into the drop
    return (bar / 2) % 4;
}

//==============================================================================
// Step patterns: tokens separated by spaces, "-" = rest, number = semitone offset,
// suffix "a" = accent, "s" = slide into the next note, "l" = long gate
struct Note { bool on = false; int n = 0; bool acc = false, slide = false, lng = false; };

std::array<Note, 16> parse (const char* text)
{
    std::array<Note, 16> out {};
    const char* p = text;
    for (auto& s : out)
    {
        while (*p == ' ') ++p;
        if (! *p) break;
        if (*p == '-' && (p[1] == ' ' || p[1] == 0)) { ++p; continue; }
        const bool neg = *p == '-';
        if (neg) ++p;
        int v = 0;
        while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        s.on = true;
        s.n = neg ? -v : v;
        for (; *p == 'a' || *p == 's' || *p == 'l'; ++p)
            (*p == 'a' ? s.acc : *p == 's' ? s.slide : s.lng) = true;
    }
    return out;
}

//==============================================================================
// Deep synth bass: saw + square sub an octave down, two cascaded lowpasses (24 dB),
// a snappy filter envelope, glide on slides. Rendered from an event list.
struct BassEvent { double start, len; int note; bool acc, slide; };

void renderBass (Stereo& out, const std::vector<BassEvent>& ev,
                 const std::function<double (double bar)>& cutoffHz,
                 const std::function<double (double bar)>& envOct)
{
    Svf f1, f2;
    double ph1 = 0, ph2 = 0, freq = 55, target = 55, fenv = 0, amp = 0;
    bool glide = false;
    size_t k = 0;
    const double barSamples = 4.0 * kSamplesPerBeat;
    const double atk = 1.0 - std::exp (-1.0 / (0.002 * kFs)), rel = 1.0 - std::exp (-1.0 / (0.03 * kFs));
    const double gl = 1.0 - std::exp (-1.0 / (0.045 * kFs));
    const double fdec = std::exp (-1.0 / (0.13 * kFs));
    double accAmt = 0;

    for (size_t i = 0; i < out.l.size(); ++i)
    {
        while (k < ev.size() && ev[k].start <= (double) i)
        {
            target = mtof (ev[k].note);
            glide = k > 0 && ev[k - 1].slide && ev[k - 1].start + ev[k - 1].len >= ev[k].start - 1;
            if (! glide) { freq = target; fenv = 1.0; }
            accAmt = ev[k].acc ? 1.0 : 0.0;
            ++k;
        }
        const bool gate = k > 0 && (double) i < ev[k - 1].start + ev[k - 1].len;
        amp += ((gate ? 1.0 : 0.0) - amp) * (gate ? atk : rel);
        freq += (target - freq) * (glide ? gl : 1.0);
        fenv *= fdec;

        ph1 = std::fmod (ph1 + freq / kFs, 1.0);
        ph2 = std::fmod (ph2 + 0.5 * freq / kFs, 1.0);
        const double osc = 0.55 * (2.0 * ph1 - 1.0) + 0.6 * (ph2 < 0.5 ? 1.0 : -1.0);

        if ((i & 15) == 0)
        {
            const double bar = i / barSamples;
            const double fc = cutoffHz (bar) * std::pow (2.0, envOct (bar) * fenv * (1.0 + 0.6 * accAmt));
            f1.set (fc, 0.9);
            f2.set (fc, 0.62 + 0.25 * accAmt);
        }
        double lo, bp, hp, lo2;
        f1.tick (osc, lo, bp, hp);
        f2.tick (lo, lo2, bp, hp);
        const double y = std::tanh (lo2 * amp * (1.6 + 0.6 * accAmt)) * 0.7;
        out.l[i] += (float) y;
        out.r[i] += (float) y;
    }
}

//==============================================================================
// Juno-style pad: pulse waves with slow PWM plus a quiet saw, gentle lowpass, slow attack
void addPwmPad (Stereo& out, const std::vector<int>& notes, double at, double lenSamples, double gain, double cutoffHz)
{
    const double attack = 0.45 * kFs, release = 1.4 * kFs;
    const size_t n = (size_t) (lenSamples + release), start = (size_t) std::lround (at);
    for (size_t vi = 0; vi < notes.size(); ++vi)
    {
        double ph = 0.21 * vi, ph2 = 0.5;
        const double f = mtof (notes[vi] + 0.06 * ((int) vi - 2)), f2 = f * 1.003;
        Svf lp; lp.set (cutoffHz, 0.7);
        const double pan = notes.size() > 1 ? 0.8 * ((double) vi / (notes.size() - 1)) - 0.4 : 0.0;
        const double gL = 1.0 - pan, gR = 1.0 + pan;
        for (size_t i = 0; i < n && start + i < out.l.size(); ++i)
        {
            const double t = (double) (start + i) / kFs;
            double env = i < attack ? i / attack : 1.0;
            if (i > lenSamples) env *= std::exp (-(i - lenSamples) / (0.4 * kFs));
            const double width = 0.5 + 0.38 * std::sin (kTwoPi * (0.35 + 0.04 * vi) * t + vi);
            ph = std::fmod (ph + f / kFs, 1.0);
            ph2 = std::fmod (ph2 + f2 / kFs, 1.0);
            const double x = (ph < width ? 1.0 : -1.0) * 0.6 + (2.0 * ph2 - 1.0) * 0.4;
            double lo, bp, hp;
            lp.tick (x, lo, bp, hp);
            const double y = lo * env * gain;
            out.l[start + i] += (float) (y * gL);
            out.r[start + i] += (float) (y * gR);
        }
    }
}

// Poly stab: chord of detuned saws, fast filter snap, short body
Buf makePolyStab (const std::vector<int>& notes)
{
    Buf b ((size_t) (0.42 * kFs));
    std::vector<double> ph (notes.size() * 2, 0.0), f;
    for (int n : notes) { f.push_back (mtof (n + 12 - 0.07)); f.push_back (mtof (n + 12 + 0.07)); }
    Svf lp;
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        double s = 0;
        for (size_t k = 0; k < f.size(); ++k) { ph[k] = std::fmod (ph[k] + f[k] / kFs, 1.0); s += 2.0 * ph[k] - 1.0; }
        if ((i & 7) == 0) lp.set (450.0 + 6000.0 * std::exp (-t * 22.0), 1.1);
        double lo, bp, hp;
        lp.tick (s / f.size(), lo, bp, hp);
        const double env = (t < 0.002 ? t / 0.002 : 1.0) * std::exp (-t * 9.0);
        b[i] = (float) (lo * env * 2.2);
    }
    return b;
}

// FM bell: inharmonic modulator, index decaying faster than the body
Buf makeBell (int note)
{
    Buf b ((size_t) (1.1 * kFs));
    const double fc = mtof (note), fm = fc * 3.5;
    double pc = 0, pm = 0;
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        pm += kTwoPi * fm / kFs;
        const double idx = 2.2 * std::exp (-t * 7.0) + 0.3;
        pc += kTwoPi * fc / kFs;
        const double y = std::sin (pc + idx * std::sin (pm)) * std::exp (-t * 3.2) * (t < 0.001 ? t / 0.001 : 1.0);
        b[i] = (float) (y * 0.6);
    }
    return b;
}

// 808-style cowbell: two squares, bandpassed, fast attack, two-stage decay
Buf makeCowbell()
{
    Buf b ((size_t) (0.35 * kFs));
    Svf bp; bp.set (1100, 1.4);
    double p1 = 0, p2 = 0;
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        p1 = std::fmod (p1 + 540.0 / kFs, 1.0);
        p2 = std::fmod (p2 + 800.0 / kFs, 1.0);
        const double x = (p1 < 0.5 ? 1.0 : -1.0) + (p2 < 0.5 ? 1.0 : -1.0);
        double lo, band, hp;
        bp.tick (x, lo, band, hp);
        const double env = 0.6 * std::exp (-t * 60.0) + 0.4 * std::exp (-t * 11.0);
        b[i] = (float) (band * env * 0.9);
    }
    return b;
}

Buf makeCrash()
{
    rng.seed (808);
    Buf b ((size_t) (2.4 * kFs));
    OnePole hp; hp.setLP (4500);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / kFs;
        const double y = hp.hp (0.5 * metal (t * 1.13) + 0.5 * noise());
        b[i] = (float) (y * (0.3 * std::exp (-t * 25.0) + 0.7 * std::exp (-t * 1.5)) * 0.9);
    }
    return b;
}
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    setTempo (kBpm, 0.56);
    const std::string outPath = argc > 1 ? argv[1] : "woodward.wav";
    const double barSamples = 4.0 * kSamplesPerBeat;
    const size_t total = (size_t) (kBars * barSamples + 4.0 * kFs);
    std::printf ("Woodward Nights: %d bars at %.0f BPM = %.0f s\n", kBars, kBpm, kBars * barSamples / kFs);

    Stereo drums (total), perc (total), bass (total), strings (total), juno (total), stabs (total),
           bells (total), riserBus (total);

    //--------------------------------------------------------------------------
    // Drums: 909-ish kick and hats, 808-ish clap, cowbell, toms and crash
    const Buf kick = makeKick(), clap = makeClap(), snare = makeSnare(), rim = makeRim(), cow = makeCowbell(), crash = makeCrash();
    const Buf ch[3] = { makeHat (0.02, 0.08, 21), makeHat (0.024, 0.08, 22), makeHat (0.018, 0.08, 23) };
    const Buf oh = makeHat (0.2, 0.7, 29);
    const Buf tom[3] = { makeDrumHead (98, 0.35, 0.28, 0.1), makeDrumHead (131, 0.35, 0.24, 0.1), makeDrumHead (175, 0.35, 0.2, 0.1) };

    //                     0123456789abcdef
    const char* kickP    = "X...X...X...X...";
    const char* ohP      = "..x...x...x...x.";
    const char* clapP    = "....X.......X...";
    const char* clapWild = "....X.......X.xx";   // pickups on phrase ends at the peak
    const char* cowA     = "..x..x....x..x..";
    const char* cowB     = "..x..x...x..x...";
    const char* rimP     = ".x.....x..x.....";
    const double hatVel[4] = { 0.5, 0.35, 1.0, 0.45 };

    std::mt19937 human (1987);
    auto jitter = [&] (double ms) { return std::normal_distribution<double> (0.0, ms * 0.001 * kFs) (human); };
    auto vary   = [&] (double amt) { return 1.0 + std::uniform_real_distribution<double> (-amt, amt) (human); };

    std::vector<double> kickTimes;

    for (int bar = 0; bar < kBars; ++bar)
    {
        const bool peak   = in (bar, 80, 112);
        const bool kickOn = ! in (bar, 56, 58) && ! in (bar, 64, 80) && bar != 31 && bar != 111 && bar < 140;
        const bool hatOn  = bar < 64 || in (bar, 72, 142);
        const bool ohOn   = in (bar, 32, 56) || in (bar, 80, 128);
        const bool clapOn = in (bar, 8, 64) || in (bar, 80, 136);
        const bool cowOn  = in (bar, 16, 56) || in (bar, 80, 128);
        const bool rimOn  = in (bar, 24, 64) || in (bar, 80, 136);

        if (bar % 8 == 0 && (in (bar, 32, 56) || in (bar, 80, 112)))
            addHit (drums, crash, stepTime ((int64_t) bar * 16), 0.32, 0.2);

        for (int s = 0; s < 16; ++s)
        {
            const int64_t abs = (int64_t) bar * 16 + s;
            const double t = stepTime (abs);

            if (kickOn && hit (kickP, s))
            {
                addHit (drums, kick, t, 0.74, 0.0);
                kickTimes.push_back (t);
            }
            if (hatOn && (bar >= 4 || s % 4 == 2))
                addHit (drums, ch[abs % 3], t + jitter (1.5), 0.3 * hatVel[s % 4] * vary (0.12), 0.15);
            if (ohOn && hit (ohP, s))
                addHit (drums, oh, t + jitter (1.0), 0.15 * vary (0.1), -0.1);
            const char* cp = peak && bar % 4 == 3 ? clapWild : clapP;
            if (clapOn && hit (cp, s))
                addHit (drums, clap, t + 0.006 * kFs, (s > 12 ? 0.2 : 0.4) * vary (0.05), 0.0);
            const char* cw = bar % 2 ? cowB : cowA;
            if (cowOn && hit (cw, s))
                addHit (perc, cow, t + jitter (1.5), 0.2 * vary (0.15) * ride (bar, { { 16, 0.4 }, { 24, 1.0 } }), 0.45);
            if (rimOn && hit (rimP, s))
                addHit (perc, rim, t + jitter (2.0), 0.2 * vary (0.15), -0.45);

            // Tom rolls down the kit into each big section
            const bool rollBar = bar == 31 || bar == 63 || bar == 111;
            if (rollBar && s >= 8)
                addHit (perc, tom[2 - (s - 8) / 3 % 3], t, 0.32 + 0.03 * (s - 8), (s % 2) ? 0.35 : -0.35);
            // Snare roll in the second half of the break
            if (in (bar, 76, 80))
            {
                const int q = bar - 76;
                const double g = 0.08 + 0.12 * (q * 16 + s) / 64.0;
                if (q < 2 ? s % 2 == 0 : true)
                    addHit (drums, snare, t, g, 0.0);
                if (q == 3)
                    addHit (drums, snare, t + 0.125 * kSamplesPerBeat, g, 0.0);
            }
        }
    }

    //--------------------------------------------------------------------------
    // Bass: hypnotic pedal or chord roots, depending on the section
    {
        const auto rollA = parse ("0a - - 0 - - 12 0 - 0 - - 0 - 12s 0");
        const auto rollB = parse ("0a - - 0 - - 12 0 - 0 - 3 - 0 7s 12");
        const auto peakP = parse ("0a - 0 12 - 0 - 0a - 0 12 - 0 10s 12 0");
        const auto breakP = parse ("0l - - - - - - - 0l - - - - - - -");

        std::vector<BassEvent> ev;
        for (int bar = 8; bar < 136; ++bar)
        {
            const bool pedal   = bar < 32 || in (bar, 112, 136);
            const bool inBreak = in (bar, 64, 80);
            int root = pedal ? 38 : kBassRoot[chordAt (bar)];
            if (in (bar, 76, 80)) root = 33;             // park on A under the A7b9
            const auto& pat = inBreak ? breakP : in (bar, 80, 112) ? peakP : (bar % 2 ? rollB : rollA);
            for (int s = 0; s < 16; ++s)
            {
                const Note& n = pat[(size_t) s];
                if (! n.on) continue;
                const double t = stepTime ((int64_t) bar * 16 + s);
                const double stepLen = 0.25 * kSamplesPerBeat;
                const double len = n.lng ? 7.5 * stepLen : n.slide ? stepLen * 1.05 : stepLen * 0.55;
                ev.push_back ({ t, len, root + n.n, n.acc, n.slide });
            }
        }
        // The hypnosis: base cutoff rides the arrangement, plus a slow 8-bar breath
        renderBass (bass, ev,
                    [] (double bar) {
                        const double base = ride (bar, { { 8, 90 }, { 16, 140 }, { 32, 260 }, { 56, 420 }, { 64, 160 },
                                                         { 80, 320 }, { 104, 600 }, { 112, 380 }, { 124, 180 }, { 136, 90 } });
                        return base * std::pow (2.0, 0.6 * std::sin (kTwoPi * bar / 8.0));
                    },
                    [] (double bar) { return ride (bar, { { 8, 1.2 }, { 32, 2.2 }, { 64, 1.0 }, { 80, 2.4 }, { 112, 2.8 }, { 136, 1.2 } }); });
    }

    //--------------------------------------------------------------------------
    // Pads: string machine high, Juno PWM in the middle
    for (int bar = 16; bar < 132; bar += 2)
    {
        const bool hang = bar == 76;
        const int c = chordAt (bar);
        const double len = (hang ? 4 : 2) * barSamples;
        const double at = stepTime ((int64_t) bar * 16);

        const double gS = ride (bar, { { 16, 0.03 }, { 30, 0.07 }, { 32, 0.1 }, { 56, 0.11 }, { 64, 0.18 }, { 78, 0.24 },
                                       { 80, 0.13 }, { 110, 0.14 }, { 112, 0.0 }, { 120, 0.0 }, { 122, 0.08 }, { 130, 0.02 } });
        const double cS = ride (bar, { { 16, 500 }, { 32, 1500 }, { 64, 1000 }, { 78, 3200 }, { 80, 2600 }, { 130, 700 } });
        if (gS > 0.001)
        {
            std::vector<int> up;
            for (int n : kChords[c]) up.push_back (n + 12);
            addChord (strings, up, at, len, gS, cS);
        }

        const double gJ = ride (bar, { { 32, 0.0 }, { 34, 0.08 }, { 56, 0.1 }, { 64, 0.16 }, { 80, 0.11 },
                                       { 110, 0.12 }, { 112, 0.0 } });
        if (gJ > 0.001)
            addPwmPad (juno, kChords[c], at, len, gJ, ride (bar, { { 32, 900 }, { 64, 1600 }, { 80, 2400 }, { 112, 1800 } }));
        if (hang) bar += 2;
    }
    ensemble (strings);
    ensemble (juno);

    // Poly stab riff: 3-3-2 pushes over the bar, the dancefloor hook
    {
        const Buf st[4] = { makePolyStab (kChords[0]), makePolyStab (kChords[1]), makePolyStab (kChords[2]), makePolyStab (kChords[3]) };
        const char* stabP  = "x..x..x...x..x..";
        const char* stabP2 = "x..x..x.x..x..x.";
        for (int bar = 32; bar < 112; ++bar)
        {
            if (in (bar, 56, 80)) continue;
            const char* p = (bar % 4 == 3) ? stabP2 : stabP;
            for (int s = 0; s < 16; ++s)
                if (hit (p, s))
                    addHit (stabs, st[chordAt (bar)], stepTime ((int64_t) bar * 16 + s), (s == 0 ? 0.32 : 0.24) * vary (0.08), (s % 3 ? 0.25 : -0.25));
        }
    }

    // FM bell hook: arpeggiates whatever chord is playing, so it always sits right.
    // An octave higher (and busier) at the peak - that's the wild bit.
    {
        std::map<int, Buf> cache;
        auto bell = [&] (int n) -> const Buf& { auto it = cache.find (n); if (it == cache.end()) it = cache.emplace (n, makeBell (n)).first; return it->second; };
        const char* rhythm[2] = { "x..x..x.x..x.x..", "..x..x..x.x..x.x" };
        const int order[16] = { 4, 2, 3, 1, 2, 4, 3, 0, 4, 3, 1, 2, 3, 4, 2, 1 };
        for (int bar = 48; bar < 112; ++bar)
        {
            const bool on = in (bar, 48, 56) || in (bar, 60, 64) || in (bar, 72, 76) || in (bar, 80, 112);
            if (! on) continue;
            const int oct = in (bar, 96, 112) ? 24 : 12;
            const auto& chord = kChords[chordAt (bar)];
            int k = 0;
            for (int s = 0; s < 16; ++s)
                if (hit (rhythm[bar % 2], s))
                {
                    const int note = chord[(size_t) order[(bar * 7 + k++) % 16]] + oct;
                    addHit (bells, bell (note), stepTime ((int64_t) bar * 16 + s), 0.16 * vary (0.1), ((k % 3) - 1) * 0.5);
                }
        }
    }

    addRiser (riserBus, stepTime (28 * 16), stepTime (32 * 16) - 0.5 * kSamplesPerBeat, 0.3);
    addRiser (riserBus, stepTime (72 * 16), stepTime (80 * 16) - 0.5 * kSamplesPerBeat, 0.55);
    addRiser (riserBus, stepTime (108 * 16), stepTime (112 * 16), 0.3);

    //--------------------------------------------------------------------------
    // Sends: dotted-eighth delay on stabs and bells, big hall on the pads, room on perc
    {
        acid::PingPongDelay dStab, dBell; dStab.prepare (kFs); dBell.prepare (kFs);
        acid::Reverb hall, room, bellVerb; hall.prepare (kFs); room.prepare (kFs); bellVerb.prepare (kFs);
        const double d = 0.75 * kSamplesPerBeat;
        for (size_t i = 0; i < total; ++i)
        {
            float l, r, vl, vr;
            dStab.process (0.5f * (stabs.l[i] + stabs.r[i]) * 0.5f, d, 0.4, l, r);
            stabs.l[i] += l; stabs.r[i] += r;

            dBell.process (0.5f * (bells.l[i] + bells.r[i]) * 0.6f, d, 0.55, l, r);
            bellVerb.process (0.5f * (bells.l[i] + bells.r[i]) * 0.3f + 0.3f * (l + r), 0.8, vl, vr);
            bells.l[i] += l + vl; bells.r[i] += r + vr;

            hall.process (0.25f * (strings.l[i] + strings.r[i] + juno.l[i] + juno.r[i]) * 0.5f, 0.9, vl, vr);
            strings.l[i] += vl; strings.r[i] += vr;

            room.process (0.5f * (perc.l[i] + perc.r[i]) * 0.2f, 0.4, vl, vr);
            perc.l[i] += vl; perc.r[i] += vr;
        }
    }

    //--------------------------------------------------------------------------
    // Mix: kick-keyed duck (harder on the bass), console saturation, dead stops
    Buf duck (total, 1.0f), duckBass (total, 1.0f);
    for (double t : kickTimes)
    {
        const size_t s0 = (size_t) t, len = (size_t) (0.5 * kSamplesPerBeat);
        for (size_t i = 0; i < len && s0 + i < total; ++i)
        {
            const double x = (double) i / len;
            const double shape = x < 0.04 ? x / 0.04 : std::pow (1.0 - (x - 0.04) / 0.96, 2.0);
            duck[s0 + i]     = std::min (duck[s0 + i], (float) (1.0 - 0.3 * shape));
            duckBass[s0 + i] = std::min (duckBass[s0 + i], (float) (1.0 - 0.75 * shape));
        }
    }

    {
        const size_t a = (size_t) (88 * barSamples), b = (size_t) (96 * barSamples);
        auto rms = [&] (const Stereo& st) { double q = 0; for (size_t i = a; i < b; ++i) q += st.l[i] * st.l[i]; return 20 * std::log10 (std::sqrt (q / (b - a)) + 1e-9); };
        std::printf ("bus RMS (bars 88-96): drums %.1f perc %.1f bass %.1f strings %.1f juno %.1f stabs %.1f bells %.1f dB\n",
                     rms (drums), rms (perc), rms (bass), rms (strings), rms (juno), rms (stabs), rms (bells));
    }

    Stereo mix (total);
    OnePole dcL, dcR; dcL.setLP (15.0); dcR.setLP (15.0);
    double peak = 0;
    for (size_t i = 0; i < total; ++i)
    {
        const double dk = duck[i], db = duckBass[i];
        // Sub sits back in the break so the drop hits
        const double bassG = ride (i / barSamples, { { 63.5, 1.0 }, { 64.5, 0.5 }, { 79.5, 0.6 }, { 80, 1.0 } });
        auto sum = [&] (const Buf Stereo::* ch) {
            return 0.85 * (drums.*ch)[i]
                 + dk * (1.7 * (perc.*ch)[i] + 1.0 * (strings.*ch)[i] + 1.0 * (juno.*ch)[i] + 1.9 * (stabs.*ch)[i]
                         + 1.5 * (bells.*ch)[i] + (riserBus.*ch)[i])
                 + db * 0.85 * bassG * (bass.*ch)[i];
        };
        double l = sum (&Stereo::l), r = sum (&Stereo::r);

        const double beatPos = i / kSamplesPerBeat;
        if ((beatPos >= 32 * 4 - 0.5 && beatPos < 32 * 4) || (beatPos >= 80 * 4 - 0.5 && beatPos < 80 * 4))
            l = r = 0.0;

        l = std::tanh (l * 1.15) / std::tanh (1.15);
        r = std::tanh (r * 1.15) / std::tanh (1.15);
        l = dcL.hp (l); r = dcR.hp (r);
        const double fade = std::min (1.0, (double) (total - i) / kFs / 3.0);
        mix.l[i] = (float) (l * fade);
        mix.r[i] = (float) (r * fade);
        peak = std::max ({ peak, std::abs (l), std::abs (r) });
    }

    const double gain = std::pow (10.0, -0.5 / 20.0) / std::max (peak, 1e-6);
    double sq = 0;
    for (size_t i = 0; i < total; ++i)
    {
        mix.l[i] *= (float) gain; mix.r[i] *= (float) gain;
        sq += mix.l[i] * mix.l[i] + mix.r[i] * mix.r[i];
    }
    std::printf ("peak normalised to -0.5 dBFS, RMS %.1f dBFS\n", 20.0 * std::log10 (std::sqrt (sq / (2.0 * total))));
    writeWav (outPath, mix);
    std::printf ("wrote %s\n", outPath.c_str());
    return 0;
}
