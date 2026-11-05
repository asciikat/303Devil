// "Thruster" - a full-length acid techno track rendered offline.
//
// v3 "minimal groove": raw Detroit-leaning acid at 134 BPM with a deep 57% shuffle.
//   - groove lead 303 that plays off the kick, syncopated, with lots of rests
//   - a second 303, the climber, takes over the break: six stages, each sitting higher than
//     the last, every knob rising with it, ending on an arpeggio of the A7b9 up to D5.
//     It comes back quieter in the build at bars 112-120.
//   - string-machine pad (Dm9 - Bbmaj7 - Gm9 - A7b9) over the 303's D pedal, hanging
//     on the A7b9 for 4 bars into the drop; noise risers and dead stops before the drops
//   - stripped kit: 909-style kick, velocity-shaped 16th hats with a little human timing,
//     laid-back clap with a ghost pickup, sparse rim and toms, a ride only at the peak
//   - hand-ridden CUTOFF / RESO / ENV MOD / DECAY, dotted-eighth delay throws, pattern
//     changes on the bar, kick-keyed duck, console-style saturation
//
// Every pattern and sound is original: synthesised here or by the 303 engine.
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

#include "synth_kit.h"

using namespace kit;

namespace
{
constexpr double kBpm  = 134.0;
constexpr int    kBars = 152;
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    setTempo (kBpm, 0.57); // deep shuffle, shared by drums and 303
    const std::string outPath = argc > 1 ? argv[1] : "thruster.wav";
    const double barSamples = 4.0 * kSamplesPerBeat;
    const size_t total = (size_t) (kBars * barSamples + 4.0 * kFs); // + tail
    std::printf ("Thruster: %d bars at %.0f BPM = %.0f s\n", kBars, kBpm, kBars * barSamples / kFs);

    //--------------------------------------------------------------------------
    // Drums and percussion
    Stereo drums (total), perc (total), stabBus (total), padBus (total), riserBus (total);

    const Buf kick = makeKick(), clap = makeClap(), snare = makeSnare(), ride_ = makeRide();
    const Buf rim = makeRim(), stab = makeStab();
    const Buf ch[3] = { makeHat (0.018, 0.08, 1), makeHat (0.022, 0.08, 2), makeHat (0.016, 0.08, 3) };
    const Buf oh = makeHat (0.16, 0.6, 9);
    const Buf tomLo = makeDrumHead (92, 0.55, 0.22, 0.25), tomMid = makeDrumHead (138, 0.5, 0.18, 0.3);

    // Minimal groove kit. Velocity maps per 16th do the swinging, plus a little human
    // timing: claps sit back, hats breathe, nothing is quantised dead.
    //                     0123456789abcdef
    const char* kickP    = "X...X...X...X...";
    const char* kickFill = "X...X...X...X.x.";
    const char* ohP      = "..x...x...x...x.";
    const char* clapP    = "....X.......X...";
    const char* clapGh   = "....X.......X..x";   // ghost clap pickup every 4th bar
    const char* rideP    = "x.x.x.x.x.x.x.x.";
    const char* rimP     = "...x.....x....x.";
    const char* tomA     = "......x.........";
    const char* tomB     = "..........x..x..";
    // Hat velocity per 16th: soft "1", ghost "e", hard "and", ghost "a"
    const double hatVel[4] = { 0.55, 0.32, 1.0, 0.4 };

    std::mt19937 human (138);
    auto jitter = [&] (double ms) { return std::normal_distribution<double> (0.0, ms * 0.001 * kFs) (human); };
    auto vary   = [&] (double amt) { return 1.0 + std::uniform_real_distribution<double> (-amt, amt) (human); };

    std::vector<double> kickTimes;

    for (int bar = 0; bar < kBars; ++bar)
    {
        const bool kickOn   = ! in (bar, 72, 88) && ! in (bar, 150, kBars) && bar != 47 && bar != 119;
        const bool hatOn    = in (bar, 8, 72) || in (bar, 80, 150);
        const bool ohOn     = in (bar, 24, 64) || in (bar, 88, 136);
        const bool clapOn   = in (bar, 16, 72) || in (bar, 88, 140);
        const bool rideOn   = in (bar, 104, 128);
        const bool tomOn    = in (bar, 48, 64) || in (bar, 96, 128);
        const bool rimOn    = in (bar, 32, 72) || in (bar, 88, 136);
        const bool rollOn   = in (bar, 84, 88);

        for (int s = 0; s < 16; ++s)
        {
            const int64_t abs = (int64_t) bar * 16 + s;
            const double t = stepTime (abs);
            const char* kp = bar % 16 == 15 ? kickFill : kickP;

            if (kickOn && hit (kp, s))
            {
                addHit (drums, kick, t, 0.72 * vel (kp, s), 0.0);
                kickTimes.push_back (t);
            }
            if (hatOn)
            {
                // the hats open up over the intro: only the "and" first, then all 16ths
                const bool play = bar >= 16 || s % 4 == 2;
                if (play)
                    addHit (drums, ch[abs % 3], t + jitter (1.5), 0.32 * hatVel[s % 4] * vary (0.12), 0.15);
            }
            if (ohOn && hit (ohP, s))
                addHit (drums, oh, t + jitter (1.0), 0.17 * vary (0.1), -0.1);
            const char* cp_ = bar % 4 == 3 ? clapGh : clapP;
            if (clapOn && hit (cp_, s))
                addHit (drums, clap, t + 0.007 * kFs, (s == 15 ? 0.16 : 0.4) * vary (0.05), 0.0);
            if (rideOn && hit (rideP, s))
                addHit (drums, ride_, t, 0.11 * vary (0.1), 0.35);
            if (rimOn && hit (rimP, s))
                addHit (perc, rim, t + jitter (2.0), 0.22 * vary (0.15), -0.5);
            if (tomOn)
            {
                const char* p = bar % 2 ? tomB : tomA;
                if (hit (p, s))
                    addHit (perc, s == 13 ? tomMid : tomLo, t, 0.38, s == 13 ? 0.3 : -0.3);
            }

            // Snare roll build into the drop: 8ths -> 16ths -> 32nds, rising
            if (rollOn)
            {
                const int q = bar - 84;
                const double g = 0.1 + 0.1 * (q * 16 + s) / 64.0;
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
        for (int bar = 48; bar < 136; bar += 2)
        {
            const bool hang = bar == 84;
            const int c = hang ? 3 : (bar / 2) % 4;
            const double len = (hang ? 4 : 2) * barSamples;
            const double g = ride (bar, { { 48, 0.04 }, { 62, 0.09 }, { 64, 0.2 }, { 86, 0.24 }, { 88, 0.12 },
                                          { 120, 0.13 }, { 134, 0.04 } });
            const double cut = ride (bar, { { 48, 500 }, { 62, 1300 }, { 64, 900 }, { 84, 2600 }, { 88, 2200 },
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

    // Chord stab: only in the dubbed-out break
    for (int bar = 64; bar < 72; bar += 2)
        addHit (stabBus, stab, stepTime ((int64_t) bar * 16 + 3), 0.35, 0.0);

    //--------------------------------------------------------------------------
    // The two 303s: the groove lead, and the climber that takes over the break
    acid::AcidEngine lead, climber;

    // Lead: off the kick, syncopated, lots of air. The groove lives in the rests.
    lead.patterns[0].loadFromString ("- 0 0a - - 0 12s 0 - 0 0a - - 3s 0 -");
    lead.patterns[1].loadFromString ("- 0 0a - - 0 12s 15 - 0 0a - - 10s 12 -");
    lead.patterns[2].loadFromString ("- 0 12a 0 - 0s 12 0a - 0 15a - 0 10s 12a -");

    // Climber: one stage per section of the break, each sitting higher than the last.
    // Stages 0-3 on root D2, stages 4-5 an octave up on the A7b9 (C# = 11).
    climber.patterns[0].loadFromString ("0a - - 0 - 3s 0 - - 7 - 0a - - 3s 5");
    climber.patterns[1].loadFromString ("3a - 5 3 - 7s 5 - 3 - 10a 7 - 5s 3 -");
    climber.patterns[2].loadFromString ("7a - 10 7 12s 10 - 7 - 12a 10 7 - 15s 12 10");
    climber.patterns[3].loadFromString ("10a 12 15s 12 10 - 12a 15 17s 15 12 - 15a 17 19s 17");
    climber.patterns[4].loadFromString ("7a - 11 14 - 17s 14 11 7a - 14 17 - 19s 17 14");
    climber.patterns[5].loadFromString ("7a 11 14 17s 19 23 14a 17 19s 20 23 19a 20 22s 23 24");
    climber.patterns[7].loadFromString ("- - - - - - - - - - - - - - - -"); // tacet

    acid::EngineParams lp, cp;
    lp.clock = cp.clock = acid::EngineParams::hostSync;
    lp.root = 38; cp.root = 38;              // D
    lp.shuffle = cp.shuffle = (kSwing - 0.5) / 0.25;

    lp.voice.acidMode = true;
    lp.voice.waveform = 0;
    lp.voice.overdrive = 0.35;
    lp.voice.slideMs = 70.0;
    lp.voice.ampDecayMs = 600.0;
    lp.voice.accentDecayMs = 160.0;
    lp.voice.sub = 0.35;
    lp.voice.volumeDb = -1.5;
    lp.fx.delayBeats = 0.75;
    lp.fx.delayFeedback = 0.5;
    lp.fx.reverbSize = 0.7;

    cp.voice.acidMode = true;
    cp.voice.waveform = 0;
    cp.voice.overdrive = 0.4;
    cp.voice.slideMs = 80.0;
    cp.voice.ampDecayMs = 700.0;
    cp.voice.accentDecayMs = 180.0;
    cp.voice.volumeDb = -5.0;
    cp.fx.delayBeats = 0.75;
    cp.fx.delayFeedback = 0.55;
    cp.fx.reverbSize = 0.8;

    lead.setParams (lp);    lead.prepare (kFs);
    climber.setParams (cp); climber.prepare (kFs);

    Stereo acidBus (total), climbBus (total);
    constexpr int block = 128;
    std::vector<float> bl (block), br (block);

    // Which climber stage plays in a bar (-1 = silent): the break, then a reprise
    // in the build at 112-120 so the second peak gets the same pull
    auto climbStage = [] (int b) {
        if (in (b, 72, 76)) return 0;
        if (in (b, 76, 80)) return 1;
        if (in (b, 80, 82)) return 2;
        if (in (b, 82, 84)) return 3;
        if (in (b, 84, 86)) return 4;
        if (in (b, 86, 88)) return 5;
        if (in (b, 112, 116)) return 3;
        if (in (b, 116, 120)) return 4;
        return -1;
    };

    for (size_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<size_t> (block, total - pos);
        const double ppq = pos / kSamplesPerBeat;
        const double bar = ppq / 4.0;
        const int ibar = (int) bar;
        const int nextBar = (int) ((ppq + 0.05) / 4.0); // pattern/root picked just ahead of the bar line

        // --- Hand-ridden knobs on the lead ---------------------------------
        lp.voice.cutoff    = ride (bar, { { 16, 0.08 }, { 32, 0.24 }, { 48, 0.36 }, { 60, 0.5 }, { 64, 0.3 },
                                          { 72, 0.2 }, { 88, 0.34 }, { 104, 0.5 }, { 112, 0.6 },
                                          { 120, 0.45 }, { 136, 0.15 }, { 144, 0.05 } });
        // Breathing on top of the long ride: a 2-bar hand wobble, the way you'd rock the knob
        lp.voice.cutoff   += 0.04 * std::sin (kTwoPi * bar / 2.0 + 0.7) * ride (bar, { { 32, 0 }, { 40, 1 }, { 136, 1 }, { 140, 0 } });
        lp.voice.resonance = ride (bar, { { 16, 0.55 }, { 48, 0.72 }, { 64, 0.8 }, { 88, 0.7 },
                                          { 112, 0.85 }, { 136, 0.6 } });
        lp.voice.envMod    = ride (bar, { { 16, 0.3 }, { 48, 0.55 }, { 64, 0.7 }, { 88, 0.6 },
                                          { 110, 0.85 }, { 136, 0.4 } });
        lp.voice.decay     = ride (bar, { { 16, 0.2 }, { 56, 0.4 }, { 72, 0.6 }, { 88, 0.3 }, { 112, 0.5 },
                                          { 136, 0.25 } });
        lp.voice.accent    = ride (bar, { { 16, 0.55 }, { 96, 0.9 }, { 136, 0.6 } });
        lp.voice.overdrive = ride (bar, { { 16, 0.25 }, { 88, 0.35 }, { 96, 0.6 }, { 120, 0.6 }, { 136, 0.3 } });

        // Pattern program (changes land at the pattern end, like a 303)
        lp.pattern = in (nextBar, 48, 64) ? 1 : in (nextBar, 96, 104) ? 1 : in (nextBar, 104, 120) ? 2 : 0;

        // Delay throws on the last bar of each 8-bar phrase, washed reverb in the break
        const bool throwBar = ibar % 8 == 7 && ibar >= 24;
        lp.fx.delayMix  = (throwBar ? 0.45 : 0.08) + (in (ibar, 64, 72) ? 0.3 : 0.0);
        lp.fx.reverbMix = ride (bar, { { 60, 0.05 }, { 68, 0.35 }, { 72, 0.4 }, { 88, 0.06 }, { 136, 0.08 }, { 148, 0.4 } });
        lp.fx.lfoDepth  = in (ibar, 64, 72) ? 0.12 : 0.0;
        lp.fx.lfoBeats  = 32.0;

        // --- The climber: every knob goes up with it ----------------------
        const int stage = climbStage (nextBar);
        cp.pattern = stage >= 0 ? stage : 7;
        cp.root    = stage >= 4 ? 50 : 38;
        const bool reprise = ibar >= 100;
        cp.voice.cutoff    = reprise ? ride (bar, { { 112, 0.3 }, { 120, 0.55 } })
                                     : ride (bar, { { 72, 0.14 }, { 80, 0.3 }, { 86, 0.5 }, { 88, 0.62 } });
        cp.voice.resonance = reprise ? 0.8 : ride (bar, { { 72, 0.7 }, { 88, 0.93 } });
        cp.voice.envMod    = reprise ? 0.6 : ride (bar, { { 72, 0.45 }, { 88, 0.85 } });
        cp.voice.decay     = ride (bar, { { 72, 0.5 }, { 88, 0.3 } });
        cp.voice.accent    = ride (bar, { { 72, 0.6 }, { 88, 0.95 } });
        cp.fx.delayMix     = reprise ? 0.15 : ride (bar, { { 72, 0.3 }, { 84, 0.2 } });
        cp.fx.reverbMix    = reprise ? 0.08 : 0.22;
        lead.setParams (lp);
        climber.setParams (cp);

        acid::Transport tr;
        tr.playing = true; tr.hasPpq = true; tr.ppq = ppq; tr.bpm = kBpm;

        lead.beginBlock (tr);
        lead.render (bl.data(), br.data(), n);
        // Mute-in / mute-out arranging on the mixer, with short fades
        const double leadGain = ride (bar, { { 15.9, 0 }, { 16, 1 }, { 70, 1 }, { 72, 0 }, { 87.9, 0 }, { 88, 1 }, { 143.9, 1 }, { 144, 0 } });
        for (int i = 0; i < n; ++i)
        {
            acidBus.l[pos + i] = (float) (bl[i] * leadGain);
            acidBus.r[pos + i] = (float) (br[i] * leadGain);
        }

        climber.beginBlock (tr);
        climber.render (bl.data(), br.data(), n);
        // Tacet outside its bars (rest pattern), so its echoes ring out naturally
        const double cGain = ibar >= 100 ? 0.6 : 1.0;
        for (int i = 0; i < n; ++i)
        {
            climbBus.l[pos + i] = (float) (bl[i] * cGain);
            climbBus.r[pos + i] = (float) (br[i] * cGain);
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
        std::printf ("bus RMS (bars 96-104): drums %.1f  perc %.1f  acid %.1f  climb %.1f  stab %.1f  pad %.1f dB\n",
                     rms (drums), rms (perc), rms (acidBus), rms (climbBus), rms (stabBus), rms (padBus));
    }

    // Climber pitch check: strongest partial per 2 bars of the break (should rise)
    std::printf ("climber, strongest partial per 2 bars:");
    for (int b0 = 72; b0 < 88; b0 += 2)
    {
        const size_t a0 = (size_t) (b0 * barSamples), len = (size_t) (2 * barSamples);
        double best = 0, bestHz = 0;
        for (double hz = 70; hz < 700; hz *= 1.01)
        {
            double re = 0, im = 0;
            for (size_t i = 0; i < len; i += 4)
            {
                const double ph = kTwoPi * hz * i / kFs;
                re += climbBus.l[a0 + i] * std::cos (ph);
                im += climbBus.l[a0 + i] * std::sin (ph);
            }
            if (re * re + im * im > best) { best = re * re + im * im; bestHz = hz; }
        }
        std::printf (" %.0f", bestHz);
    }
    std::printf (" Hz\n");

    Stereo mix (total);
    OnePole dcL, dcR; dcL.setLP (15.0); dcR.setLP (15.0);
    double peak = 0, sumSq = 0;
    for (size_t i = 0; i < total; ++i)
    {
        const double dk = duck[i];
        const double music_l = 1.1 * acidBus.l[i] + 0.9 * climbBus.l[i] + 0.7 * stabBus.l[i] + 0.9 * perc.l[i] + padBus.l[i] + riserBus.l[i];
        const double music_r = 1.1 * acidBus.r[i] + 0.9 * climbBus.r[i] + 0.7 * stabBus.r[i] + 0.9 * perc.r[i] + padBus.r[i] + riserBus.r[i];
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
