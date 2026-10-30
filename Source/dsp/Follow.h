#pragma once

// FOLLOW: listens to MIDI from another part (chords, pads, a bassline) and works out the
// chord being played and the key of the track, so the sequencer's own pattern can be placed
// over it. The MIDI is never played, only heard. Plain C++17, no JUCE.

#include "AcidGenerator.h"

#include <array>
#include <cmath>
#include <string>

namespace acid
{
namespace follow
{
inline constexpr double kLatchBeats    = 4.0;    // a released chord holds for a bar, then the key's tonic takes over
inline constexpr double kKeyHalfLife   = 64.0;   // beats; old harmony fades so the key can move with the track
inline constexpr double kKeyMinBeats   = 16.0;   // the first chord's guess stands until this much has been heard
inline constexpr double kKeyMargin     = 0.1;    // another key must fit this much better (correlation) to take over
inline constexpr int    kMajorScale    = 0b101010110101;   // 0 2 4 5 7 9 11
inline constexpr int    kMinorScale    = 0b010110101101;   // 0 2 3 5 7 8 10

inline int rotate (int mask, int by) noexcept
{
    by = ((by % 12) + 12) % 12;
    return ((mask << by) | (mask >> (12 - by))) & 0xfff;
}

inline int pitchClass (int note) noexcept { return ((note % 12) + 12) % 12; }

// The note with pitch class pc that is closest to home (the lower one on a tie)
inline int nearestWithClass (int home, int pc) noexcept
{
    const int below = home - pitchClass (home - pc);
    return home - below <= below + 12 - home ? below : below + 12;
}

// The note closest to note whose pitch class is in mask (the lower one on a tie)
inline int snapToMask (int note, int mask) noexcept
{
    if ((mask & 0xfff) == 0)
        return note;
    for (int d = 0; d < 12; ++d)
    {
        if (mask & (1 << pitchClass (note - d))) return note - d;
        if (mask & (1 << pitchClass (note + d))) return note + d;
    }
    return note;
}

inline int scaleMask (Scale s) noexcept
{
    int m = 0;
    for (int d : scaleDegrees (s))
        m |= 1 << d;
    return m;
}

inline const char* noteName (int pc) noexcept
{
    static const char* const names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[pitchClass (pc)];
}

// The root of a chord from its shape, so inversions work (pads are rarely played root
// position): a fifth above counts most, then a third, then a seventh. The lowest note wins a tie.
inline int chordRootOf (int mask, int lowestPc) noexcept
{
    int best = lowestPc;
    double bestScore = -1.0;
    for (int r = 0; r < 12; ++r)
    {
        if ((mask & (1 << r)) == 0)
            continue;
        const int m = rotate (mask, -r);
        const auto has = [m] (int interval) { return (m & (1 << interval)) != 0; };
        const double score = (has (7) ? 4.0 : 0.0) + (has (3) || has (4) ? 3.0 : 0.0) + (has (10) || has (11) ? 1.0 : 0.0)
                             + (r == lowestPc ? 1.5 : 0.0);
        if (score > bestScore)
        {
            best = r;
            bestScore = score;
        }
    }
    return best;
}

// "Ab", "Cm", "Fm7", "G7"; anything else is named after its root
inline std::string chordName (int mask, int root)
{
    const auto has = [&] (int interval) { return (mask & (1 << pitchClass (root + interval))) != 0; };
    std::string name = noteName (root);
    if (has (3) && ! has (4)) name += "m";
    if (has (10)) name += "7";
    return name;
}

// The editor's readout. key = root pitch class (+12 if minor), chord = mask | root << 12; -1 = none
inline std::string describe (int key, int chord)
{
    if (key < 0)
        return "waiting for MIDI";
    std::string text = std::string (noteName (key % 12)) + (key >= 12 ? " minor" : " major");
    if (chord >= 0)
        text += ", chord " + chordName (chord & 0xfff, (chord >> 12) % 12);
    return text;
}
} // namespace follow

class FollowHarmony
{
public:
    void reset() noexcept { *this = {}; }

    void noteOn (int note) noexcept
    {
        if (note < 0 || note > 127 || held[(std::size_t) note])
            return;
        if (numHeld == 0)
        {
            chord = 0;                       // a fresh chord after a release
            lowest = note;
        }
        held[(std::size_t) note] = true;
        ++numHeld;
        releasing = false;
        chord |= 1 << follow::pitchClass (note);
        lowest = std::min (lowest, note);
        root = follow::chordRootOf (chord, follow::pitchClass (lowest));
        latched = true;
        silentBeats = 0.0;
        if (heardBeats <= 0.0)
            updateKey();   // still the first chord: guess again with this note in it
    }

    void noteOff (int note) noexcept
    {
        if (note < 0 || note > 127 || ! held[(std::size_t) note])
            return;
        held[(std::size_t) note] = false;
        --numHeld;

        // Notes let go at the same moment are one release: if they all go, the whole chord is
        // remembered, not just the last note standing
        if (! releasing)
        {
            releasing = true;
            chordBeforeRelease = chord;
            rootBeforeRelease = root;
        }
        if (numHeld == 0)
        {
            chord = chordBeforeRelease;
            root = rootBeforeRelease;
        }
        else
        {
            // What is still held is the chord now (a new chord that came in before the old one let go)
            chord = 0;
            lowest = 128;
            for (int n = 0; n < 128; ++n)
                if (held[(std::size_t) n])
                {
                    chord |= 1 << follow::pitchClass (n);
                    lowest = std::min (lowest, n);
                }
            root = follow::chordRootOf (chord, follow::pitchClass (lowest));
        }
    }

    // Time passes: the held notes count towards the key, and a released chord ages
    void advance (double beats) noexcept
    {
        if (beats <= 0.0)
            return;
        releasing = false;

        const double fade = std::exp2 (-beats / follow::kKeyHalfLife);
        for (auto& w : weight)
            w *= fade;
        heardBeats *= fade;
        if (numHeld > 0)
        {
            heardBeats += beats;
            for (int pc = 0; pc < 12; ++pc)
                if (chord & (1 << pc))
                    weight[(std::size_t) pc] += beats;
        }

        if (numHeld == 0 && latched)
        {
            silentBeats += beats;
            if (silentBeats > follow::kLatchBeats)
                latched = false;
        }

        sinceKeyUpdate += beats;
        if (sinceKeyUpdate >= 1.0 || ! keyKnown)
            updateKey();
    }

    bool hasChord() const noexcept  { return latched; }
    int chordMask() const noexcept  { return chord; }
    int chordRoot() const noexcept  { return root; }
    bool hasKey() const noexcept    { return keyKnown; }
    int keyRoot() const noexcept    { return key; }
    bool keyMinor() const noexcept  { return minor; }
    std::string keyName() const     { return keyKnown ? std::string (follow::noteName (key)) + (minor ? " minor" : " major") : std::string(); }

    // The note to play for a pattern step: offset = the step's note (semitones from the
    // pattern's root), home = the Root Note. Before anything has been heard this is the
    // internal sequencer's note. Driver and bass move with the chord root; the overlay stays
    // in the key and winds through the chord. Notes land on chord tones or on scale tones
    // that don't rub a semitone against the chord; the bass takes chord tones only.
    int noteFor (int offset, int home, Role role, Scale fallbackScale) const noexcept
    {
        if (! keyKnown && ! latched)
            return home + snapToScale (offset, fallbackScale);

        const int keyPc = keyKnown ? key : follow::pitchClass (home);
        const int scale = keyKnown ? follow::rotate (minor ? follow::kMinorScale : follow::kMajorScale, key)
                                   : follow::rotate (follow::scaleMask (fallbackScale), keyPc);
        const int chordNow = latched ? chord : follow::rotate (minor ? 0b10001001 : 0b10010001, keyPc);   // tonic triad
        const int rootPc = latched ? root : keyPc;

        const int rub = follow::rotate (chordNow, 1) | follow::rotate (chordNow, -1);
        const int allowed = role == Role::bass ? chordNow : (chordNow | (scale & ~rub));
        const int base = follow::nearestWithClass (home, role == Role::overlay ? keyPc : rootPc);
        return std::clamp (follow::snapToMask (base + offset, allowed), 0, 127);
    }

private:
    // Krumhansl-Kessler key profiles, correlated with how long each pitch class has sounded.
    // Holds on to the key it has unless another one clearly fits better, so a bar or two of
    // a different chord doesn't flip it.
    void updateKey() noexcept
    {
        sinceKeyUpdate = 0.0;
        double total = 0.0;
        for (double w : weight)
            total += w;
        if (total <= 0.0)
        {
            // Nothing has sounded for any time yet: guess from the first chord
            if (numHeld > 0)
            {
                key = root;
                minor = (chord & (1 << follow::pitchClass (root + 3))) != 0;
                keyKnown = true;
            }
            return;
        }

        static constexpr double major[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static constexpr double minorProfile[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto correlate = [this] (const double* profile, int tonic)
        {
            double mw = 0.0, mp = 0.0;
            for (int i = 0; i < 12; ++i) { mw += weight[(std::size_t) i]; mp += profile[i]; }
            mw /= 12.0; mp /= 12.0;
            double num = 0.0, dw = 0.0, dp = 0.0;
            for (int i = 0; i < 12; ++i)
            {
                const double a = weight[(std::size_t) ((i + tonic) % 12)] - mw, b = profile[i] - mp;
                num += a * b; dw += a * a; dp += b * b;
            }
            return dw > 0.0 && dp > 0.0 ? num / std::sqrt (dw * dp) : -1.0;
        };

        if (keyKnown && heardBeats < follow::kKeyMinBeats)
            return;

        double best = -2.0;
        int bestKey = key;
        bool bestMinor = minor;
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            for (bool isMinor : { false, true })
            {
                const double r = correlate (isMinor ? minorProfile : major, tonic);
                if (r > best)
                {
                    best = r;
                    bestKey = tonic;
                    bestMinor = isMinor;
                }
            }
        }
        if (keyKnown && best < correlate (minor ? minorProfile : major, key) + follow::kKeyMargin)
            return;
        key = bestKey;
        minor = bestMinor;
        keyKnown = true;
    }

    std::array<bool, 128> held {};
    int numHeld = 0;
    int chord = 0, lowest = 128, root = 0;
    int chordBeforeRelease = 0, rootBeforeRelease = 0;
    bool latched = false, releasing = false;
    double silentBeats = 0.0;

    std::array<double, 12> weight {};
    double heardBeats = 0.0;   // how long notes have been held, fading like the weights
    double sinceKeyUpdate = 0.0;
    bool keyKnown = false, minor = true;
    int key = 0;
};
} // namespace acid
