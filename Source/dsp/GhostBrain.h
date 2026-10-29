#pragma once

// GHOST HANDS, phase 2: the brain. Cuts your performance into one-beat moves, sorts them
// into move types (k-means), and counts which type you play after which, in which
// situation: where you are in the bar and the 4-bar phrase, and whether the pattern has
// accents or slides on that beat. The ghost improvises from those counts, playing real
// beats of yours, so it sounds like your hands. Plain statistics, no neural net.

#include "Tb303Voice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace acid
{
namespace ghost
{
enum Lane { cutoff, resonance, envMod, decay, accent, drive, kNumLanes };

constexpr int kTicksPerBeat = 24;  // ~50 snapshots a second at 130 BPM
constexpr int kBeatsPerBar  = 4;
constexpr int kTicksPerBar  = kTicksPerBeat * kBeatsPerBar;
constexpr int kMaxBars      = 16;
constexpr int kMaxTicks     = kMaxBars * kTicksPerBar;
constexpr int kLengthChoices[] = { 1, 2, 4, 8, 16 };
constexpr int kNumLengthChoices = (int) (sizeof (kLengthChoices) / sizeof (kLengthChoices[0]));

constexpr int kBeatValues      = kTicksPerBeat * kNumLanes;
constexpr int kMaxTakeBeats    = kMaxBars * kBeatsPerBar;
constexpr int kMaxLearnedBeats = 64;   // beats learned live, when you grab a knob
constexpr int kMaxBeats        = kMaxTakeBeats + kMaxLearnedBeats;
constexpr int kMaxMoveTypes    = 16;
constexpr int kNumContexts     = 32;   // 8 positions in the phrase x 4 pattern flag combos

enum BeatFlags { accentFlag = 1, slideFlag = 2 };   // the pattern has an accent / slide on this beat

inline double& laneValue (VoiceParams& v, int lane) noexcept
{
    switch (lane)
    {
        case cutoff:    return v.cutoff;
        case resonance: return v.resonance;
        case envMod:    return v.envMod;
        case decay:     return v.decay;
        case accent:    return v.accent;
        default:        return v.overdrive;
    }
}

inline int64_t floorDiv (int64_t a, int64_t b) noexcept
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

inline int wrap (int64_t a, int n) noexcept
{
    return (int) (((a % n) + n) % n);
}

// Situation of a beat: beat in the bar, whether it's in the last bar of a 4-bar phrase
// (where people build up or close down), and the pattern's accents and slides on it
inline int makeContext (int64_t beat, int flags) noexcept
{
    const int beatInBar = wrap (beat, kBeatsPerBar);
    const bool phraseEnd = wrap (floorDiv (beat, kBeatsPerBar), 4) == 3;
    return ((phraseEnd ? kBeatsPerBar : 0) + beatInBar) * 4 + (flags & 3);
}

//==============================================================================
struct BeatSample
{
    std::array<float, kBeatValues> values {};   // tick by tick, lanes interleaved
    uint8_t context = 0;
    bool followsPrevious = false;               // played straight after the sample before it

    float get (int tick, int lane) const noexcept { return values[(std::size_t) (tick * kNumLanes + lane)]; }
    void set (int tick, int lane, float v) noexcept { values[(std::size_t) (tick * kNumLanes + lane)] = v; }
};

//==============================================================================
class Brain
{
public:
    // Fill these in, then call learn()
    int numBeats = 0, numTakeBeats = 0;
    std::array<BeatSample, kMaxBeats> beats;
    std::array<int16_t, kMaxBeats> next {};     // the beat you played after this one, or -1

    // Results
    int numTypes = 0;
    std::array<int8_t, kMaxBeats> type {};

    void learn() noexcept
    {
        byContextAndPrev.fill (0.0f);
        byContext.fill (0.0f);
        byFlags.fill (0.0f);
        byPrev.fill (0.0f);
        overall.fill (0.0f);
        numTypes = 0;
        if (numBeats < 2)
            return;

        numTypes = std::clamp (numBeats / 3, 2, kMaxMoveTypes);
        extractFeatures();
        cluster();

        for (int j = 0; j < numBeats; ++j)
        {
            const int t = type[(std::size_t) j], ctx = beats[(std::size_t) j].context;
            byContext[(std::size_t) (ctx * kMaxMoveTypes + t)] += 1.0f;
            byFlags[(std::size_t) ((ctx & 3) * kMaxMoveTypes + t)] += 1.0f;
            overall[(std::size_t) t] += 1.0f;
        }
        for (int i = 0; i < numBeats; ++i)
        {
            const int j = next[(std::size_t) i];
            if (j < 0)
                continue;
            const int from = type[(std::size_t) i], to = type[(std::size_t) j], ctx = beats[(std::size_t) j].context;
            byContextAndPrev[(std::size_t) ((ctx * kMaxMoveTypes + from) * kMaxMoveTypes + to)] += 1.0f;
            byPrev[(std::size_t) (from * kMaxMoveTypes + to)] += 1.0f;
        }
    }

    // Next move type. Blends the specific counts (this situation, after this move) with
    // broader ones, so situations you never played still get a sensible answer.
    // temperature < 1 plays your favourite moves, > 1 gets adventurous. u is 0..1.
    int chooseType (int previous, int context, double temperature, double u) const noexcept
    {
        std::array<double, kMaxMoveTypes> p {};
        auto addRow = [&] (const float* row, double weight)
        {
            double sum = 0.0;
            for (int k = 0; k < numTypes; ++k)
                sum += row[k];
            if (sum > 0.0)
                for (int k = 0; k < numTypes; ++k)
                    p[(std::size_t) k] += weight * row[k] / sum;
        };

        if (previous >= 0)
            addRow (&byContextAndPrev[(std::size_t) ((context * kMaxMoveTypes + previous) * kMaxMoveTypes)], 0.35);
        addRow (&byContext[(std::size_t) (context * kMaxMoveTypes)], 0.20);
        addRow (&byFlags[(std::size_t) ((context & 3) * kMaxMoveTypes)], 0.25);
        if (previous >= 0)
            addRow (&byPrev[(std::size_t) (previous * kMaxMoveTypes)], 0.12);
        addRow (overall.data(), 0.08);

        double total = 0.0;
        for (int k = 0; k < numTypes; ++k)
        {
            auto& x = p[(std::size_t) k];
            x = x > 0.0 ? std::pow (x, 1.0 / std::max (0.05, temperature)) : 0.0;
            total += x;
        }
        if (total <= 0.0)
            return 0;

        double x = u * total;
        for (int k = 0; k < numTypes; ++k)
        {
            x -= p[(std::size_t) k];
            if (x <= 0.0)
                return k;
        }
        return numTypes - 1;
    }

    // A real beat of yours of that type: often the one you actually played next, else
    // one from the same situation, else one with the same accents/slides, else any.
    int chooseExemplar (int moveType, int context, int after, double u1, double u2) const noexcept
    {
        if (after >= 0)
        {
            const int n = next[(std::size_t) after];
            if (n >= 0 && type[(std::size_t) n] == moveType && u1 < 0.6)
                return n;
        }

        auto pick = [&] (auto matches) -> int
        {
            int count = 0;
            for (int i = 0; i < numBeats; ++i)
                count += (type[(std::size_t) i] == moveType && matches (i)) ? 1 : 0;
            if (count == 0)
                return -1;
            int target = std::min (count - 1, (int) (u2 * count));
            for (int i = 0; i < numBeats; ++i)
                if (type[(std::size_t) i] == moveType && matches (i) && target-- == 0)
                    return i;
            return -1;
        };

        const bool strict = u1 < 0.85;
        int found = strict ? pick ([&] (int i) { return beats[(std::size_t) i].context == context; }) : -1;
        if (found < 0 && strict)
            found = pick ([&] (int i) { return (beats[(std::size_t) i].context & 3) == (context & 3); });
        if (found < 0)
            found = pick ([] (int) { return true; });
        return std::max (0, found);
    }

private:
    // A move = the six knobs at 7 points through the beat
    static constexpr int kPoints = 7;
    static constexpr int kDims = kPoints * kNumLanes;
    using Feature = std::array<float, kDims>;

    void extractFeatures() noexcept
    {
        static constexpr int pointTicks[kPoints] = { 0, 4, 8, 12, 16, 20, 23 };
        for (int i = 0; i < numBeats; ++i)
            for (int k = 0; k < kPoints; ++k)
                for (int lane = 0; lane < kNumLanes; ++lane)
                    features[(std::size_t) i][(std::size_t) (k * kNumLanes + lane)] = beats[(std::size_t) i].get (pointTicks[k], lane);
    }

    static float distance (const Feature& a, const Feature& b) noexcept
    {
        float d = 0.0f;
        for (int k = 0; k < kDims; ++k)
            d += (a[(std::size_t) k] - b[(std::size_t) k]) * (a[(std::size_t) k] - b[(std::size_t) k]);
        return d;
    }

    // k-means with k-means++ seeding. Seeded RNG, so the same take always learns the same.
    void cluster() noexcept
    {
        uint32_t rng = 0x9e3779b9u;
        auto uniform = [&rng]
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return (float) (rng >> 8) / 16777216.0f;
        };

        std::array<float, kMaxBeats> nearest {};
        centroids[0] = features[0];
        for (int c = 1; c < numTypes; ++c)
        {
            float total = 0.0f;
            for (int i = 0; i < numBeats; ++i)
            {
                float d = distance (features[(std::size_t) i], centroids[0]);
                for (int k = 1; k < c; ++k)
                    d = std::min (d, distance (features[(std::size_t) i], centroids[(std::size_t) k]));
                nearest[(std::size_t) i] = d;
                total += d;
            }
            int chosen = (int) (uniform() * numBeats) % numBeats;
            if (total > 0.0f)
            {
                float x = uniform() * total;
                for (int i = 0; i < numBeats; ++i)
                    if ((x -= nearest[(std::size_t) i]) <= 0.0f) { chosen = i; break; }
            }
            centroids[(std::size_t) c] = features[(std::size_t) chosen];
        }

        for (int iteration = 0; iteration < 30; ++iteration)
        {
            bool changed = false;
            for (int i = 0; i < numBeats; ++i)
            {
                int best = 0;
                float bestD = distance (features[(std::size_t) i], centroids[0]);
                for (int c = 1; c < numTypes; ++c)
                {
                    const float d = distance (features[(std::size_t) i], centroids[(std::size_t) c]);
                    if (d < bestD) { bestD = d; best = c; }
                }
                nearest[(std::size_t) i] = bestD;
                if (iteration == 0 || type[(std::size_t) i] != best)
                    changed = true;
                type[(std::size_t) i] = (int8_t) best;
            }
            if (! changed)
                break;

            for (int c = 0; c < numTypes; ++c)
            {
                Feature sum {};
                int count = 0;
                for (int i = 0; i < numBeats; ++i)
                {
                    if (type[(std::size_t) i] != c)
                        continue;
                    for (int k = 0; k < kDims; ++k)
                        sum[(std::size_t) k] += features[(std::size_t) i][(std::size_t) k];
                    ++count;
                }

                if (count > 0)
                {
                    for (int k = 0; k < kDims; ++k)
                        centroids[(std::size_t) c][(std::size_t) k] = sum[(std::size_t) k] / (float) count;
                }
                else
                {
                    // Empty type: restart it on the beat that fits its own type worst
                    const auto worst = std::max_element (nearest.begin(), nearest.begin() + numBeats) - nearest.begin();
                    centroids[(std::size_t) c] = features[(std::size_t) worst];
                    nearest[(std::size_t) worst] = 0.0f;
                }
            }
        }
    }

    // Counts of the type played: in this situation after that type, in this situation,
    // with these pattern flags, after that type, and overall
    std::array<float, kNumContexts * kMaxMoveTypes * kMaxMoveTypes> byContextAndPrev {};
    std::array<float, kNumContexts * kMaxMoveTypes> byContext {};
    std::array<float, 4 * kMaxMoveTypes> byFlags {};
    std::array<float, kMaxMoveTypes * kMaxMoveTypes> byPrev {};
    std::array<float, kMaxMoveTypes> overall {};

    std::array<Feature, kMaxBeats> features {};
    std::array<Feature, kMaxMoveTypes> centroids {};
};
} // namespace ghost
} // namespace acid
