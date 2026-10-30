#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace acid
{
constexpr int kNumSteps = 16;
constexpr int kNumPatterns = 8;
constexpr int kMinNoteOffset = -24;
constexpr int kMaxNoteOffset = 24;

struct Step
{
    int  note   = 0;     // semitones relative to the root note
    bool gate   = true;  // false = rest
    bool accent = false;
    bool slide  = false; // hold the gate and glide into the next step (303 behaviour)
    int  ratchet = 0;    // 0..3 = the step is played as 1..4 fast hits (a roll); ignored on slides and rests

    int hits() const noexcept { return gate && ! slide ? ratchet + 1 : 1; }
};

// 16-step pattern shared by the GUI (writer) and the audio thread (reader).
// Each step is packed into one atomic word so a step is never read half-updated.
class Pattern
{
public:
    Pattern() { clear(); }

    Step get (int index) const noexcept     { return unpack (steps[(std::size_t) index].load (std::memory_order_relaxed)); }
    void set (int index, const Step& s) noexcept { steps[(std::size_t) index].store (pack (s), std::memory_order_relaxed); }

    // Every step on, root note, no accent or slide
    void clear() noexcept
    {
        for (int i = 0; i < kNumSteps; ++i)
            set (i, Step {});
    }

    // Steps separated by spaces: "-" = rest, otherwise a semitone offset followed by
    // optional 'a' (accent), 's' (slide) and/or 'r2'..'r4' (roll: that many hits in the step),
    // e.g. "0a 12s 3 - -2as 7r4 ..."
    void loadFromString (const char* text) noexcept
    {
        clear();
        const char* p = text;
        for (int i = 0; i < kNumSteps; ++i)
        {
            while (*p == ' ')
                ++p;
            if (*p == '\0')
                break;

            Step s;
            if (*p == '-' && (p[1] == ' ' || p[1] == '\0'))
            {
                s.gate = false;
                ++p;
            }
            else
            {
                const bool negative = *p == '-';
                if (negative)
                    ++p;
                int n = 0;
                while (*p >= '0' && *p <= '9')
                    n = n * 10 + (*p++ - '0');
                s.note = negative ? -n : n;
                for (; *p == 'a' || *p == 's' || (*p == 'r' && p[1] >= '2' && p[1] <= '4'); ++p)
                {
                    if (*p == 'r')
                        s.ratchet = *++p - '2' + 1;
                    else
                        (*p == 'a' ? s.accent : s.slide) = true;
                }
            }
            set (i, s);
        }
    }

private:
    static uint32_t pack (const Step& s) noexcept
    {
        const int n = s.note < kMinNoteOffset ? kMinNoteOffset : (s.note > kMaxNoteOffset ? kMaxNoteOffset : s.note);
        return (uint32_t) (n + 128)
             | (s.gate   ? 1u << 8  : 0u)
             | (s.accent ? 1u << 9  : 0u)
             | (s.slide  ? 1u << 10 : 0u)
             | (uint32_t) (s.ratchet < 0 ? 0 : (s.ratchet > 3 ? 3 : s.ratchet)) << 11;
    }

    static Step unpack (uint32_t w) noexcept
    {
        return { (int) (w & 0xffu) - 128, (w & (1u << 8)) != 0, (w & (1u << 9)) != 0, (w & (1u << 10)) != 0,
                 (int) ((w >> 11) & 3u) };
    }

    std::array<std::atomic<uint32_t>, kNumSteps> steps {};
};

// Factory pattern bank, root note = C. See Pattern::loadFromString for the format.
inline constexpr const char* kFactoryPatterns[kNumPatterns] = {
    "0a 12s 3 - -2a 5s 7 - 0a 10s 3 - 12a 7s 3 -",       // 1 Classic minor line
    "0a 12 0 12s 0 12a 0 12 0a 12 0 12s 0 15 0 12a",     // 2 Octave pump
    "0 0s 12 0 0a 3 0s 7 0 0 10s 12a 0 5 0s 3",          // 3 Squelch run
    "0a 3 5 7s 10 7 5s 3a 0 3 5 7a 12s 10 7s 5",         // 4 Minor climb
    "0a - - 0s 7 - -5 - 0a - 3s 5 - - -2 -",             // 5 Sparse dub
    "0s 7s 12a 10s 7 - 0s 3s 5a 3s 0 - 12s 15as 12s 10", // 6 Slide heavy
    "0 0 0a 0 0 0 12a 0 0 0 0a 0 3 0 15a 0",             // 7 Sixteenth drive
    "0a 0 7s 3 - 0 10s 7a 0 - 5 3s 0a 0 -2 0",           // 8 Hypnotic
};

// Library of typical acid techno figures, loadable into any bank slot.
// Same format as kFactoryPatterns; root note = 0.
struct LibraryPattern
{
    const char* name;
    const char* steps;
};

inline constexpr LibraryPattern kPatternLibrary[] = {
    // Pulses: one note hammered, movement from accents and slides
    { "Root Pulse",          "0a 0 0 0s 0a 0 0 12s 0a 0 0 0s 0a 0 12 0" },
    { "Offbeat Accents",     "0 0a 0 0a 0 0a 0 12as 0 0a 0 0a 0 0a 7 12as" },
    { "Syncopated Bass",     "0a - 0 0 - 0a - 0 0 - 0a - 0 12s 0 -" },
    { "Minimal Tick",        "0a - - - 0 - - 0s 0a - - - 0 - 12 -" },
    { "Stutter",             "0a 0a 0 - 0 0a 0 - 0a 0a 12 - 0 0a 3s 0" },
    { "Gallop",              "0a 0 0 - 0a 0 0 - 0a 0 0 - 12a 12 10s 7" },
    // Octave work
    { "Octave Bounce",       "0 12 0 12a 0 12 0s 12 0 12a 0 12 0 12s 0a 12" },
    { "Big Octave Slide",    "0a 0 0 0 0 0 0s 24a 12 0 0 0 0 0 12s 0a" },
    { "Rolling Octaves",     "0 0s 12 0 3 0s 12 0 5 0s 12 0 3a 0s 12 0" },
    { "Ravey Minor",         "0a 12 10 12 7s 12 5 12a 3 12 5 12 7as 12 10 12" },
    // Scales and runs
    { "Minor Pent Run",      "0a 3 5 7 10s 12 10 7a 5 3s 0 3 5a 7 3s 0" },
    { "Slide Staircase",     "0s 3s 5s 7a 0s 3s 5s 10a 0s 3s 5s 12a 0s 3s 7s 15a" },
    { "Rising Tension",      "0 0 0 0a 1 1 1 1a 3 3 3 3a 5 5s 7s 12a" },
    { "Call and Response",   "0a 0 3 0 5s 7 - - 12a 10 7 5 3s 0 - -" },
    { "Acid Anthem",         "0a 0 12s 0 10 0 7s 0a 0 0 12s 0 15a 12s 10 7s" },
    // Dark intervals: phrygian half-step and tritone
    { "Phrygian Creep",      "0a 1 0 1s 0 3 1s 0 0a 1 0 7s 6 3 1s 0" },
    { "Acid Trill",          "0 1s 0 1s 0a 3 0 1s 0 1s 0a 3 7s 6 5s 3" },
    { "Tritone Tension",     "0a 6 0 6s 0 6a 12 6 0a 6 0 6s 7 6a 0 -6" },
    { "Dark Drone",          "0a 0s 0s 0 -1 0s 0s 0 0a 0s 0s 0 -2 0s 0s 0" },
    { "Deep Groove",         "-5a -5 0s -5 -5 3 -5s 0a -5 -5 0s -5 7 5s 3 0" },
    // Hypnotic loops
    { "Hypno Repeat",        "0a 7 0 7s 12 7 0 7a 0 7 0 7s 12 7 0 10a" },
    { "Three Over Four",     "0a 12 7s 0a 12 7s 0a 12 7s 0a 12 7s 0a 12 7s 0a" },
    { "Half-time Wobble",    "0as 0s 0s 0 12as 12s 12s 12 10as 10s 10s 10 7as 7s 3s 0" },
    { "Squelch Stabs",       "0a - 0a - 12as 0 - 0a - 0a 0 - 12as 0 - 7" },
};

inline constexpr int kNumLibraryPatterns = (int) (sizeof (kPatternLibrary) / sizeof (kPatternLibrary[0]));
} // namespace acid
