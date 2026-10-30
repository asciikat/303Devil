#pragma once

// "Cheat" helpers: scales for scale lock, and a rule-based acid line generator.
// The generator writes a short motif and repeats it with small changes, because
// repetition plus a few surprises is what makes an acid line hook. Plain C++17.

#include "Pattern.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace acid
{
//==============================================================================
enum class Scale { off, minor, phrygian, minorPentatonic, dorian, harmonicMinor };

inline const std::vector<int>& scaleDegrees (Scale s)
{
    static const std::vector<int> chromatic { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static const std::vector<int> minor { 0, 2, 3, 5, 7, 8, 10 };
    static const std::vector<int> phrygian { 0, 1, 3, 5, 7, 8, 10 };
    static const std::vector<int> pentatonic { 0, 3, 5, 7, 10 };
    static const std::vector<int> dorian { 0, 2, 3, 5, 7, 9, 10 };
    static const std::vector<int> harmonic { 0, 2, 3, 5, 7, 8, 11 };

    switch (s)
    {
        case Scale::minor:           return minor;
        case Scale::phrygian:        return phrygian;
        case Scale::minorPentatonic: return pentatonic;
        case Scale::dorian:          return dorian;
        case Scale::harmonicMinor:   return harmonic;
        case Scale::off:             break;
    }
    return chromatic;
}

// Moves a note offset (semitones from the root) to the nearest note of the scale
inline int snapToScale (int note, Scale s)
{
    if (s == Scale::off)
        return note;

    const int octave = (note >= 0 ? note : note - 11) / 12;
    const int degree = note - octave * 12;
    int best = 0, bestDistance = 99;
    for (int d : scaleDegrees (s))
    {
        for (int candidate : { d, d + 12 })
        {
            const int distance = std::abs (candidate - degree);
            if (distance < bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }
    }
    return octave * 12 + best;
}

//==============================================================================
enum class Style { classic, hard, hypnotic, minimal, dark, rave };

class AcidGenerator
{
public:
    explicit AcidGenerator (uint32_t seed) : state (seed == 0 ? 0x303303u : seed) {}

    // scale == off picks the style's own scale. energy 0.5 = the style as it is; higher
    // writes busier lines with more accents, slides and octave jumps.
    // roll 0..1 = how many steps get fast retriggers (x2..x4); 0 writes none
    void generate (Pattern& pattern, Style style, Scale scale, double energy = 0.5, double roll = 0.0)
    {
        auto rules = rulesFor (style);
        const auto e = (float) (2.0 * (std::clamp (energy, 0.0, 1.0) - 0.5));
        rules.density = std::clamp (rules.density + 0.15f * e, 0.3f, 1.0f);
        rules.accent  = std::clamp (rules.accent + 0.12f * e, 0.05f, 0.7f);
        rules.slide   = std::clamp (rules.slide + 0.10f * e, 0.05f, 0.6f);
        rules.octave  = std::clamp (rules.octave + 0.10f * e, 0.0f, 0.7f);
        if (scale == Scale::off)
            scale = rules.scale;

        std::array<Step, kNumSteps> steps {};

        // 1. Write a motif
        for (int i = 0; i < rules.motifLength; ++i)
            steps[(std::size_t) i] = makeStep (i, rules, scale);

        // 2. Repeat it across the bar
        for (int i = rules.motifLength; i < kNumSteps; ++i)
            steps[(std::size_t) i] = steps[(std::size_t) (i % rules.motifLength)];

        // 3. Answer: change a few steps in the second half, and add a fill at the end
        const int changes = 1 + next (3);
        for (int c = 0; c < changes; ++c)
        {
            const int i = 8 + next (8);
            if (i % 4 != 0)
                steps[(std::size_t) i] = makeStep (i, rules, scale);
        }
        if (chance (rules.fillChance))
            writeFill (steps, rules, scale);

        // 4. House rules: start on an accented root, no long silences, enough accents and slides
        steps[0] = { 0, true, true, false };
        for (int i = 0, rests = 0; i < kNumSteps; ++i)
        {
            rests = steps[(std::size_t) i].gate ? 0 : rests + 1;
            if (rests > 2)
            {
                steps[(std::size_t) i] = { 0, true, false, false };
                rests = 0;
            }
        }
        ensureAtLeast (steps, 2, [] (Step& s) -> bool& { return s.accent; });
        ensureAtLeast (steps, 1, [] (Step& s) -> bool& { return s.slide; });

        addRolls (steps, roll);

        for (int i = 0; i < kNumSteps; ++i)
            pattern.set (i, steps[(std::size_t) i]);
    }

private:
    struct Rules
    {
        float density, accent, slide, octave;
        int motifLength;
        Scale scale;
        float fillChance;
        bool offbeatAccents;
    };

    static Rules rulesFor (Style s)
    {
        switch (s)
        {
            case Style::hard:     return { 0.92f, 0.45f, 0.20f, 0.35f, 4, Scale::minor,           0.6f, true };
            case Style::hypnotic: return { 0.78f, 0.25f, 0.30f, 0.20f, 4, Scale::minorPentatonic, 0.3f, false };
            case Style::minimal:  return { 0.50f, 0.30f, 0.15f, 0.12f, 8, Scale::minorPentatonic, 0.2f, false };
            case Style::dark:     return { 0.82f, 0.30f, 0.35f, 0.18f, 8, Scale::phrygian,        0.5f, false };
            case Style::rave:     return { 0.86f, 0.35f, 0.30f, 0.45f, 8, Scale::minorPentatonic, 0.7f, true };
            case Style::classic:  break;
        }
        return { 0.80f, 0.30f, 0.30f, 0.25f, 8, Scale::minor, 0.5f, false };
    }

    Step makeStep (int i, const Rules& r, Scale scale)
    {
        const bool strong = i % 4 == 0;
        Step s;
        s.gate = strong ? chance (0.95f) : chance (r.density);
        s.note = strong && chance (0.6f) ? 0 : pickNote (scale);
        if (chance (r.octave))
            s.note += s.note <= 0 ? 12 : 0;
        else if (chance (0.06f))
            s.note -= 12;

        float accentChance = r.accent * (strong ? 1.4f : 0.6f);
        if (r.offbeatAccents && i % 4 == 2)
            accentChance = r.accent * 1.6f;
        s.accent = s.gate && chance (accentChance);
        s.slide = s.gate && chance (r.slide);
        return s;
    }

    // Rolls land mostly on the last step of a beat and build towards the end of the bar, like a
    // drummer's fill. The downbeat stays a single clean hit, and slides never roll.
    void addRolls (std::array<Step, kNumSteps>& steps, double roll)
    {
        const auto amount = (float) std::clamp (roll, 0.0, 1.0);
        if (amount <= 0.0f)
            return;

        for (int i = 1; i < kNumSteps; ++i)
        {
            auto& s = steps[(std::size_t) i];
            if (! s.gate || s.slide)
                continue;

            float p = 0.5f * amount;
            if (i % 4 == 3) p *= 1.8f;
            if (i >= 12)    p *= 1.5f;
            if (chance (std::min (p, 0.95f)))
                s.ratchet = chance (0.75f - 0.4f * amount) ? 1 : (chance (0.5f) ? 2 : 3);
        }
    }

    int pickNote (Scale scale)
    {
        // Weighted towards root, fifth, minor third, minor seventh and the dark flat second
        const auto& degrees = scaleDegrees (scale);
        float total = 0.0f;
        std::array<float, 12> weights {};
        for (int d : degrees)
        {
            float w = 1.0f;
            if (d == 0) w = 5.0f;
            else if (d == 7) w = 2.5f;
            else if (d == 3 || d == 10) w = 2.0f;
            else if (d == 1) w = 1.8f;
            weights[(std::size_t) d] = w;
            total += w;
        }

        float x = uniform() * total;
        for (int d : degrees)
        {
            x -= weights[(std::size_t) d];
            if (x <= 0.0f)
                return d;
        }
        return 0;
    }

    void writeFill (std::array<Step, kNumSteps>& steps, const Rules& r, Scale scale)
    {
        switch (next (3))
        {
            case 0: // octave jump with a slide back down
                steps[13] = { 12, true, true, true };
                steps[14] = { 0, true, false, false };
                steps[15] = { 12, true, false, false };
                break;
            case 1: // climbing run
            {
                const auto& d = scaleDegrees (scale);
                for (int k = 0; k < 4; ++k)
                    steps[(std::size_t) (12 + k)] = { d[(std::size_t) std::min ((int) d.size() - 1, k + 1)], true, k == 3, k < 3 && chance (r.slide) };
                break;
            }
            default: // stab and rest
                steps[14] = { 12, true, true, false };
                steps[15] = { 0, false, false, false };
                break;
        }
    }

    template <typename Field>
    void ensureAtLeast (std::array<Step, kNumSteps>& steps, int count, Field field)
    {
        int have = 0;
        for (auto& s : steps)
            have += (s.gate && field (s)) ? 1 : 0;
        for (int tries = 0; have < count && tries < 64; ++tries)
        {
            auto& s = steps[(std::size_t) next (kNumSteps)];
            if (s.gate && ! field (s))
            {
                field (s) = true;
                ++have;
            }
        }
    }

    // xorshift32
    uint32_t nextRaw() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float uniform() noexcept { return (float) (nextRaw() >> 8) / 16777216.0f; }
    bool chance (float p) noexcept { return uniform() < p; }
    int next (int n) noexcept { return (int) (nextRaw() % (uint32_t) n); }

    uint32_t state;
};
} // namespace acid
