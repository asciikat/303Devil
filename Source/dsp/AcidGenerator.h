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

// What the line is for: the main acid (driver), a bass under the track, a gentle overlay, or an
// arp of the chord being followed
enum class Role { driver, bass, overlay, arp };

class AcidGenerator
{
public:
    explicit AcidGenerator (uint32_t seed) : state (seed == 0 ? 0x303303u : seed) {}

    // scale == off picks the style's own scale. energy 0.5 = the style as it is; higher
    // writes busier lines with more accents, slides and octave jumps.
    // roll 0..1 = how many steps get fast retriggers (x2..x4); 0 writes none
    // role: driver = the style's acid line; bass, overlay and arp have their own writers (style unused)
    void generate (Pattern& pattern, Style style, Scale scale, double energy = 0.5, double roll = 0.0, Role role = Role::driver)
    {
        if (role != Role::driver)
        {
            std::array<Step, kNumSteps> steps {};
            for (auto& s : steps)
                s.gate = false;
            const auto e = (float) (2.0 * (std::clamp (energy, 0.0, 1.0) - 0.5));
            if (role == Role::bass)
                writeBass (steps, e);
            else if (role == Role::arp)
                writeArp (steps, e);
            else
                writeOverlay (steps, e);
            addRolls (steps, roll);
            for (int i = 0; i < kNumSteps; ++i)
                pattern.set (i, steps[(std::size_t) i]);
            return;
        }

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

    // Rolling techno bass: the kick owns every beat, the line rolls through the three 16ths
    // after it with the accent on the off-beat 8th. One beat is written and repeated, then the
    // bar gets octave pops and a pick-up into the next bar. e = energy, -1..1.
    void writeBass (std::array<Step, kNumSteps>& steps, float e)
    {
        std::array<Step, 4> beat {};
        beat[0] = { 0, false, false, false };   // the kick's
        beat[1] ={ 0, chance (0.6f + 0.2f * e), false, false };
        beat[2] = { 0, true, chance (0.55f + 0.15f * e), false };
        beat[3] = { 0, chance (0.8f), false, false };
        if (! beat[1].gate && ! beat[3].gate)
            beat[3].gate = true;

        for (int i = 0; i < kNumSteps; ++i)
            steps[(std::size_t) i] = beat[(std::size_t) (i % 4)];

        for (int i = 1; i < kNumSteps; ++i)
        {
            auto& s = steps[(std::size_t) i];
            if (! s.gate || i % 4 == 2)
                continue;
            if (chance (0.15f + 0.1f * e))
                s.note = 12;
            else if (chance (0.1f))
                s.note = chance (0.5f) ? 7 : 10;
        }

        // Pick-up: the last 16ths climb or drop, and one slide may glide into it
        steps[15] = { chance (0.5f) ? 12 : (chance (0.5f) ? 7 : 10), true, false, false };
        if (steps[14].gate && steps[14].note != steps[15].note && chance (0.35f))
            steps[14].slide = true;
    }

    // Gentle overlay: about half the steps, an octave up, walking up and down a chord-tone
    // ladder with long slides and few accents: the winding arpeggio. Eight steps are written
    // and answered with a couple of changes. e = energy, -1..1.
    void writeOverlay (std::array<Step, kNumSteps>& steps, float e)
    {
        static constexpr int ladder[] = { 7, 10, 12, 15, 19, 22, 24, 27 };
        constexpr int top = (int) (sizeof (ladder) / sizeof (ladder[0])) - 1;
        const float density = std::clamp (0.5f + 0.15f * e, 0.3f, 0.7f);

        int pos = 2 + next (3), direction = chance (0.5f) ? 1 : -1;
        auto walk = [&]
        {
            if (chance (0.25f))
                direction = -direction;
            pos += direction * (chance (0.7f) ? 1 : 2);
            if (pos < 0 || pos > top)
            {
                direction = -direction;
                pos = std::clamp (pos, 0, top);
            }
            return ladder[pos];
        };

        for (int i = 0; i < 8; ++i)
        {
            if (i == 0 || chance (density))
                steps[(std::size_t) i] = { walk(), true, chance (0.12f), chance (0.35f) };
        }
        for (int i = 8; i < kNumSteps; ++i)
            steps[(std::size_t) i] = steps[(std::size_t) (i - 8)];
        for (int c = 0; c < 2; ++c)
        {
            auto& s = steps[(std::size_t) (9 + next (7))];
            s = { walk(), true, false, chance (0.35f) };
        }

        // Keep it sparse but not empty: 4 to 11 notes
        int gated = 0;
        for (auto& s : steps)
            gated += s.gate ? 1 : 0;
        for (int tries = 0; gated > 11 && tries < 64; ++tries)
        {
            auto& s = steps[(std::size_t) (1 + next (kNumSteps - 1))];
            if (s.gate) { s.gate = false; --gated; }
        }
        for (int tries = 0; gated < 4 && tries < 64; ++tries)
        {
            auto& s = steps[(std::size_t) next (kNumSteps)];
            if (! s.gate) { s = { walk(), true, false, false }; ++gated; }
        }
    }

    // Arp rhythm: the chord gives the notes, so this is the groove. Most steps play, the
    // downbeat is accented with a few more accents on the off-beat 8ths, a quarter of the steps
    // slide (the 303 winding between chord notes), and the odd note jumps an octave (12).
    // Eight steps are written and answered with two changes. e = energy, -1..1.
    void writeArp (std::array<Step, kNumSteps>& steps, float e)
    {
        const float density = std::clamp (0.8f + 0.12f * e, 0.65f, 0.95f);
        auto makeArpStep = [&] (int i)
        {
            Step s { 0, i == 0 || chance (density), false, false };
            s.accent = s.gate && (i == 0 || chance (i % 4 == 2 ? 0.35f : 0.12f));
            s.slide = s.gate && chance (0.25f);
            s.note = s.gate && chance (0.12f + 0.06f * e) ? 12 : 0;
            return s;
        };
        for (int i = 0; i < 8; ++i)
            steps[(std::size_t) i] = makeArpStep (i);
        for (int i = 8; i < kNumSteps; ++i)
            steps[(std::size_t) i] = steps[(std::size_t) (i - 8)];
        for (int c = 0; c < 2; ++c)
        {
            const int i = 9 + next (7);
            steps[(std::size_t) i] = makeArpStep (i);
        }

        // Busy: at least 10 of the 16 steps play, never more than two rests in a row
        int gated = 0;
        for (auto& s : steps)
            gated += s.gate ? 1 : 0;
        for (int tries = 0; gated < 10 && tries < 64; ++tries)
        {
            auto& s = steps[(std::size_t) next (kNumSteps)];
            if (! s.gate) { s = { 0, true, false, false }; ++gated; }
        }
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
