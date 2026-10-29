#pragma once

// Post effects for the "main stage" sound: tempo-synced ping-pong delay and a
// Freeverb-style reverb. Plain C++17, no JUCE.

#include "Tb303Voice.h"

#include <vector>

namespace acid
{
// Ping-pong delay with high/low cut in the feedback path, so repeats get darker and
// never muddy the bass.
class PingPongDelay
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        size = (int) (sampleRate * 4.0) + 4;
        bufL.assign ((std::size_t) size, 0.0f);
        bufR.assign ((std::size_t) size, 0.0f);
        lowCutL.setCutoff (220.0, fs);
        lowCutR.setCutoff (220.0, fs);
        highCutL.setCutoff (3800.0, fs);
        highCutR.setCutoff (3800.0, fs);
        timeCoef = detail::approachCoef (80.0, fs);
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        lowCutL.reset(); lowCutR.reset(); highCutL.reset(); highCutR.reset();
        writePos = 0;
        smoothedDelay = -1.0;
    }

    // Returns the wet signal for one mono input sample
    void process (float in, double delaySamples, double feedback, float& outL, float& outR) noexcept
    {
        delaySamples = std::clamp (delaySamples, 1.0, (double) size - 2.0);
        if (smoothedDelay < 0.0)
            smoothedDelay = delaySamples;
        smoothedDelay += (delaySamples - smoothedDelay) * timeCoef;

        const float dl = read (bufL, smoothedDelay);
        const float dr = read (bufR, smoothedDelay);

        // Input goes left, each side feeds the other: echoes bounce L, R, L, ...
        const auto fbL = (float) highCutL.lp (lowCutL.hp (dr * feedback));
        const auto fbR = (float) highCutR.lp (lowCutR.hp (dl * feedback));
        bufL[(std::size_t) writePos] = in + fbL;
        bufR[(std::size_t) writePos] = fbR;
        if (++writePos >= size)
            writePos = 0;

        outL = dl;
        outR = dr;
    }

private:
    float read (const std::vector<float>& buf, double delay) const noexcept
    {
        double pos = (double) writePos - delay;
        while (pos < 0.0)
            pos += size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const auto frac = (float) (pos - i0);
        return buf[(std::size_t) i0] + (buf[(std::size_t) i1] - buf[(std::size_t) i0]) * frac;
    }

    double fs = 44100.0, timeCoef = 0.0, smoothedDelay = -1.0;
    int size = 0, writePos = 0;
    std::vector<float> bufL, bufR;
    detail::OnePole lowCutL, lowCutR, highCutL, highCutR;
};

//==============================================================================
// Freeverb (Jezar, public domain): 8 parallel combs + 4 series allpasses per side
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        static constexpr int combTunings[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static constexpr int allpassTunings[] = { 556, 441, 341, 225 };
        constexpr int stereoSpread = 23;
        const double scale = sampleRate / 44100.0;

        for (int i = 0; i < 8; ++i)
        {
            combsL[(std::size_t) i].setSize ((int) (combTunings[i] * scale));
            combsR[(std::size_t) i].setSize ((int) ((combTunings[i] + stereoSpread) * scale));
        }
        for (int i = 0; i < 4; ++i)
        {
            allpassL[(std::size_t) i].setSize ((int) (allpassTunings[i] * scale));
            allpassR[(std::size_t) i].setSize ((int) ((allpassTunings[i] + stereoSpread) * scale));
        }
    }

    // size: 0..1
    void process (float in, double roomSize, float& outL, float& outR) noexcept
    {
        const float feedback = (float) (0.7 + 0.28 * std::clamp (roomSize, 0.0, 1.0));
        const float input = in * 0.015f;
        float l = 0.0f, r = 0.0f;

        for (auto& c : combsL) l += c.process (input, feedback);
        for (auto& c : combsR) r += c.process (input, feedback);
        for (auto& a : allpassL) l = a.process (l);
        for (auto& a : allpassR) r = a.process (r);

        outL = l * 3.0f;
        outR = r * 3.0f;
    }

private:
    struct Comb
    {
        std::vector<float> buf;
        int pos = 0;
        float store = 0.0f;

        void setSize (int n) { buf.assign ((std::size_t) std::max (1, n), 0.0f); pos = 0; store = 0.0f; }

        float process (float in, float feedback) noexcept
        {
            constexpr float damp = 0.35f;
            const float out = buf[(std::size_t) pos];
            store = out * (1.0f - damp) + store * damp;
            buf[(std::size_t) pos] = in + store * feedback;
            if (++pos >= (int) buf.size())
                pos = 0;
            return out;
        }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;

        void setSize (int n) { buf.assign ((std::size_t) std::max (1, n), 0.0f); pos = 0; }

        float process (float in) noexcept
        {
            const float b = buf[(std::size_t) pos];
            buf[(std::size_t) pos] = in + b * 0.5f;
            if (++pos >= (int) buf.size())
                pos = 0;
            return b - in;
        }
    };

    std::array<Comb, 8> combsL, combsR;
    std::array<Allpass, 4> allpassL, allpassR;
};
} // namespace acid
