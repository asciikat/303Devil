#pragma once

// ENERGY: one macro for a whole set. 0.5 is neutral (nothing changes); lower thins the line
// out and washes it in reverb, higher opens the filter, adds accents, slides and fills,
// pumps harder and throws the delay at the end of each phrase. An arc moves it for you
// over 8-64 bars, following the song position. Plain C++17, no JUCE.

#include "Pattern.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace acid
{
struct EnergyArc
{
    const char* name;
    double bars;   // cycle length; 0 = manual (the ENERGY fader)
};

inline constexpr EnergyArc kEnergyArcs[] = {
    { "Manual (fader)",              0.0 },
    { "Build up (16 bars)",          16.0 },
    { "Build-Peak-Break-Drop (32)",  32.0 },
    { "Slow journey (64 bars)",      64.0 },
    { "Wave (8 bars)",                8.0 },
};
inline constexpr int kNumEnergyArcs = (int) (sizeof (kEnergyArcs) / sizeof (kEnergyArcs[0]));

namespace energy
{
struct Key { double bar, value; };

// Eased between keys; two keys on (almost) the same bar make a jump, like a drop
template <std::size_t N>
double follow (const Key (&keys)[N], double bar) noexcept
{
    if (bar <= keys[0].bar)
        return keys[0].value;
    for (std::size_t i = 1; i < N; ++i)
    {
        if (bar <= keys[i].bar)
        {
            const double span = keys[i].bar - keys[i - 1].bar;
            double x = span > 0.0 ? (bar - keys[i - 1].bar) / span : 1.0;
            x = x * x * (3.0 - 2.0 * x);
            return keys[i - 1].value + (keys[i].value - keys[i - 1].value) * x;
        }
    }
    return keys[N - 1].value;
}
} // namespace energy

// Energy of an arc at a song position in bars (loops every cycle). Arc 0 has no curve.
inline double arcEnergy (int arc, double bars) noexcept
{
    arc = std::clamp (arc, 0, kNumEnergyArcs - 1);
    const double length = kEnergyArcs[arc].bars;
    if (length <= 0.0)
        return 0.5;

    const double b = bars - std::floor (bars / length) * length;
    using energy::Key;
    switch (arc)
    {
        case 1:
        {
            static constexpr Key keys[] = { { 0, 0.15 }, { 15, 1.0 }, { 16, 1.0 } };
            return energy::follow (keys, b);
        }
        case 2:
        {
            static constexpr Key keys[] = { { 0, 0.25 }, { 12, 0.85 }, { 14, 0.95 }, { 20, 1.0 }, { 21, 0.15 },
                                            { 23.99, 0.1 }, { 24, 1.0 }, { 32, 0.8 } };
            return energy::follow (keys, b);
        }
        case 3:
        {
            static constexpr Key keys[] = { { 0, 0.2 }, { 24, 0.6 }, { 48, 1.0 }, { 50, 1.0 }, { 51, 0.15 },
                                            { 55.99, 0.12 }, { 56, 1.0 }, { 64, 0.9 } };
            return energy::follow (keys, b);
        }
        default:
            return 0.6 + 0.3 * std::sin (6.283185307179586 * b / length - 1.5707963267948966);
    }
}

// The pattern as played at this energy. The stored pattern never changes. Low energy
// drops unaccented off-beat notes; high energy adds accents, slides and octave fills.
// The dice are the same for every phrase, so the groove repeats, except at peak
// energy, where every 4-bar phrase gets a new roll (live mutation).
inline Step energiseStep (Step s, int index, int64_t absStep, double e) noexcept
{
    if (e > 0.35 && e < 0.65)
        return s;

    const auto phrase = (uint32_t) (e > 0.85 ? (absStep >= 0 ? absStep : absStep - 4 * kNumSteps + 1) / (4 * kNumSteps) : 0);
    auto dice = [&] (uint32_t salt)
    {
        uint32_t h = (uint32_t) index * 7919u + salt * 104729u + phrase * 2654435761u;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return (double) (h & 0xffffffu) / 16777216.0;
    };

    if (e <= 0.35)
    {
        const double thin = (0.35 - e) / 0.35;
        if (s.gate && ! s.accent && index % 4 != 0 && dice (1) < 0.8 * thin)
            s.gate = false;
    }
    else
    {
        const double heat = (e - 0.65) / 0.35;
        if (s.gate)
        {
            if (! s.accent && dice (2) < 0.35 * heat) s.accent = true;
            if (! s.slide && dice (3) < 0.25 * heat)  s.slide = true;
            // Peak energy: the odd step rolls (x2, sometimes x3 or x4)
            if (! s.slide && s.ratchet == 0 && dice (5) < 0.18 * heat)
                s.ratchet = dice (6) < 0.6 ? 1 : (dice (7) < 0.5 ? 2 : 3);
        }
        else if (index % 2 == 1 && dice (4) < 0.4 * heat)
        {
            s = { 12, true, false, false };
        }
    }
    return s;
}
} // namespace acid
