#pragma once

// GHOST HANDS, phase 3: factory ghosts. Each is a hand-written performance of the six
// knobs, beat by beat, with the accents and slides the pattern had on each beat, so LEARN
// mode has real situations to learn from. Written as maths rather than recorded, so they
// stay small and tweakable. Plain C++17, no JUCE.

#include "GhostHands.h"

#include <cmath>

namespace acid
{
struct FactoryGhost
{
    const char* name;
    const char* description;
    int bars;
};

inline constexpr FactoryGhost kFactoryGhosts[] = {
    { "Slow Opener",      "opens the filter over 8 bars, snaps shut at the end",       8 },
    { "Accent Chaser",    "stabs the cutoff open on every accent, shut in between",     4 },
    { "Breakdown Closer", "squelchy for 3 bars, closes everything down on the 4th",     4 },
    { "Peak-Time Maniac", "eighth-note wobble, screaming resonance, a riser every 4 bars", 4 },
};
inline constexpr int kNumFactoryGhosts = (int) (sizeof (kFactoryGhosts) / sizeof (kFactoryGhosts[0]));

namespace ghost
{
// Accent / slide layout per beat: 4 different bars, so the brain sees accents and slides
// on every beat of the bar
inline int factoryBeatFlags (int beat) noexcept
{
    static constexpr int bars[4][4] = {
        { accentFlag, 0, accentFlag, slideFlag },
        { 0, accentFlag | slideFlag, 0, accentFlag },
        { accentFlag, accentFlag, slideFlag, 0 },
        { slideFlag, 0, accentFlag, accentFlag | slideFlag },
    };
    return bars[(beat / kBeatsPerBar) % 4][beat % kBeatsPerBar];
}

inline double smooth (double x) noexcept
{
    x = std::clamp (x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

// Knob values for one tick. beat counts from the start of the ghost, u is 0..1 through the beat.
inline void factoryKnobs (int index, int beat, double u, int numBeats, int flags, double (&k)[kNumLanes]) noexcept
{
    constexpr double twoPi = 6.283185307179586;
    const bool accented = (flags & accentFlag) != 0, sliding = (flags & slideFlag) != 0;
    const int beatInBar = beat % kBeatsPerBar, bar = beat / kBeatsPerBar;
    const double x = (beat + u) / numBeats;   // 0..1 through the whole ghost

    switch (index)
    {
        case 0: // Slow Opener
        {
            double open = smooth (x / 0.95);
            if (beat == numBeats - 1)
                open *= 1.0 - smooth (u * 1.5);   // snap shut on the last beat
            k[cutoff]    = 0.12 + 0.62 * open;
            k[envMod]    = 0.35 + 0.35 * open;
            k[resonance] = 0.62 + 0.18 * open;
            k[decay]     = 0.28 + 0.27 * open;
            k[accent]    = 0.60 + (accented ? 0.10 : 0.0);
            k[drive]     = 0.25 + 0.30 * open;
            break;
        }

        case 1: // Accent Chaser
        {
            const double stab = accented ? std::exp (-3.5 * u) : 0.0;
            k[cutoff]    = 0.20 + 0.50 * stab;
            k[envMod]    = 0.45 + (accented ? 0.30 : 0.0);
            k[resonance] = 0.74 + 0.12 * stab;
            k[decay]     = sliding ? 0.60 : 0.30;
            k[accent]    = accented ? 0.92 : 0.55;
            k[drive]     = 0.35 + 0.25 * stab;
            break;
        }

        case 2: // Breakdown Closer
        {
            const double wobble = 0.08 * std::sin (twoPi * (beatInBar + u) / kBeatsPerBar);
            const double close = (bar % 4) == 3 ? smooth ((beatInBar + u) / kBeatsPerBar) : 0.0;
            k[cutoff]    = (0.50 + wobble + (accented ? 0.06 : 0.0)) * (1.0 - close) + 0.08 * close;
            k[envMod]    = 0.62 - 0.32 * close;
            k[resonance] = 0.70 + 0.22 * close;
            k[decay]     = 0.45 - 0.33 * close;
            k[accent]    = 0.70;
            k[drive]     = 0.50 - 0.30 * close;
            break;
        }

        default: // Peak-Time Maniac
        {
            const double riser = (bar % 4) == 3 ? smooth ((beatInBar + u) / kBeatsPerBar) : 0.0;
            const double wob = 0.5 + 0.5 * std::sin (twoPi * 2.0 * u - 1.5);   // two wobbles a beat
            k[cutoff]    = 0.32 + (accented ? 0.30 : 0.20) * wob + 0.45 * riser;
            k[envMod]    = 0.78 + 0.12 * wob;
            k[resonance] = sliding ? 0.98 : 0.88 + 0.06 * wob;
            k[decay]     = 0.55 + 0.25 * std::sin (twoPi * (beatInBar + u) / kBeatsPerBar);
            k[accent]    = accented ? 0.95 : 0.75;
            k[drive]     = 0.72 + 0.20 * riser;
            break;
        }
    }

    for (double& v : k)
        v = std::clamp (v, 0.0, 1.0);
}
} // namespace ghost

// Writes a factory ghost into the take
inline void writeFactoryGhost (int index, GhostTake& take) noexcept
{
    using namespace ghost;
    index = std::clamp (index, 0, kNumFactoryGhosts - 1);
    const int bars = kFactoryGhosts[index].bars;
    const int numBeats = bars * kBeatsPerBar;

    for (int beat = 0; beat < numBeats; ++beat)
    {
        const int flags = factoryBeatFlags (beat);
        take.setBeatFlags (beat, flags);
        for (int t = 0; t < kTicksPerBeat; ++t)
        {
            double k[kNumLanes];
            factoryKnobs (index, beat, (double) t / kTicksPerBeat, numBeats, flags, k);
            for (int lane = 0; lane < kNumLanes; ++lane)
                take.set (beat * kTicksPerBeat + t, lane, (float) k[lane]);
        }
    }
    take.commit (bars, 0, 0);
}
} // namespace acid
