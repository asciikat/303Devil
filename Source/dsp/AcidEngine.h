#pragma once

// Voice + 16-step sequencer + MIDI note handling. No JUCE: the plugin and the offline
// renderer both drive this class.

#include "AcidGenerator.h"
#include "Effects.h"
#include "Energy.h"
#include "Follow.h"
#include "GhostHands.h"
#include "Pattern.h"
#include "Tb303Voice.h"

#include <cstdint>
#include <limits>

namespace acid
{
struct Transport
{
    bool   playing = false;
    bool   hasPpq  = false;
    double ppq     = 0.0;   // host position in quarter notes at the start of the block
    double bpm     = 0.0;   // 0 = unknown
};

// Main-stage extras. All timing is in beats (quarter notes) and locks to the clock.
struct FxParams
{
    double delayMix      = 0.0;   // 0..1
    double delayBeats    = 0.75;  // 3/16 = dotted eighth, the classic acid echo
    double delayFeedback = 0.45;  // 0..0.9
    double reverbMix     = 0.0;   // 0..1
    double reverbSize    = 0.6;   // 0..1
    double pump          = 0.0;   // 0..1, sidechain-style duck on every beat
    double lfoDepth      = 0.0;   // 0..1 = up to +-2 octaves of cutoff
    double lfoBeats      = 16.0;  // LFO cycle length
};

struct EngineParams
{
    enum Source { internalSequencer = 0, midiInput = 1, followMidi = 2 };
    enum Clock  { hostSync = 0, freeRun = 1 };

    VoiceParams voice;
    FxParams    fx;
    int    source         = internalSequencer;
    int    clock          = hostSync;
    bool   run            = true;
    double bpm            = 130.0;  // used in free run, and if the host gives no tempo
    int    root           = 36;     // MIDI note of pattern offset 0
    double shuffle        = 0.0;    // 0..1 -> swing 50%..75%
    int    length         = kNumSteps;
    int    pattern        = 0;      // requested bank slot; switches at the end of the playing pattern
    int    accentVelocity = 100;    // MIDI input: velocity at or above this = accent
    int    role           = 0;      // acid::Role: how followed notes are placed

    // Cheat panel
    double squelch    = 0.0;        // one-knob acid macro, 0..1
    double tweak      = 0.0;        // auto knob-tweaking amount, 0..1
    double tweakBeats = 8.0;        // time between auto-tweak moves
    int    scale      = 0;          // acid::Scale; non-off snaps every played note into key

    GhostParams ghost;

    // Energy: 0.5 = neutral. energyArc > 0 = an arc drives it from the song position.
    double energy    = 0.5;
    int    energyArc = 0;
};

class AcidEngine
{
public:
    static constexpr double kGateLength = 0.5; // 303 gate is high for the first half of a step

    std::array<Pattern, kNumPatterns> patterns;
    GhostHands ghost;

    AcidEngine()
    {
        for (int i = 0; i < kNumPatterns; ++i)
            patterns[(std::size_t) i].loadFromString (kFactoryPatterns[i]);
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        voice.setParams (params.voice);
        voice.prepare (sampleRate);
        delay.prepare (sampleRate);
        reverb.prepare (sampleRate);
        fxCoef = detail::approachCoef (30.0, sampleRate);
        stopSequencer();
        freeRunning = false;
        numHeld = 0;
        ghost.reset();
    }

    void setParams (const EngineParams& p) noexcept
    {
        if (p.source != params.source || p.clock != params.clock)
        {
            stopSequencer();
            releaseAllMidiNotes();
        }
        if (p.source != params.source)
            harmony.reset();

        params = p;
        voice.setParams (p.voice);
    }

    void beginBlock (const Transport& t) noexcept
    {
        transport = t;
        sampleInBlock = 0;
    }

    void render (float* left, float* right, int numSamples) noexcept
    {
        const auto& fx = params.fx;
        const double bpm = currentBpm();
        const double delaySamples = fx.delayBeats * 60.0 / bpm * fs;

        for (int i = 0; i < numSamples; ++i, ++sampleInBlock)
        {
            double ppq = 0.0;
            const bool clockRunning = tickClock (ppq);

            if (params.source != EngineParams::midiInput)
            {
                if (clockRunning)
                {
                    wasRunning = true;
                    advance (ppq);
                }
                else if (wasRunning)
                {
                    stopSequencer();
                }
            }

            if ((controlCounter++ & 31) == 0)
            {
                updateEnergy (clockRunning, ppq);
                voice.setParams (performParams (clockRunning, ppq));
            }

            // Bar-locked filter LFO
            const double lfo = clockRunning ? std::sin (2.0 * kPi * ppq / std::max (0.25, fx.lfoBeats)) : 0.0;
            voice.setCutoffModOct (fx.lfoDepth * 2.0 * lfo);

            double y = voice.process();

            // Pump: duck hard on each beat and recover, like a sidechain from the kick
            const double pump = std::min (1.0, fx.pump + energyPump);
            if (clockRunning && pump > 0.0)
            {
                const double beatFrac = ppq - std::floor (ppq);
                const double env = beatFrac < 0.02 ? beatFrac / 0.02 : std::exp (-(beatFrac - 0.02) * 6.0);
                y *= 1.0 - 0.9 * pump * env;
            }

            delayMix += (std::min (1.0, fx.delayMix + energyThrow) - delayMix) * fxCoef;
            reverbMix += (std::min (1.0, fx.reverbMix + energyWash) - reverbMix) * fxCoef;

            float dl = 0.0f, dr = 0.0f, rl = 0.0f, rr = 0.0f;
            delay.process ((float) (y * delayMix), delaySamples, std::clamp (fx.delayFeedback, 0.0, 0.9), dl, dr);
            reverb.process ((float) (y * reverbMix + 0.5 * (dl + dr) * reverbMix), fx.reverbSize, rl, rr);

            left[i]  = (float) y + dl + rl;
            right[i] = (float) y + dr + rr;
        }

        if (params.source == EngineParams::followMidi)
            harmony.advance (numSamples * bpm / (60.0 * fs));
    }

    // Mono convenience for tests
    void render (float* out, int numSamples) noexcept
    {
        float scratch[256];
        for (int done = 0; done < numSamples;)
        {
            const int n = std::min (256, numSamples - done);
            render (out + done, scratch, n);
            done += n;
        }
    }

    // MIDI input, applied at the sample position where render() stopped
    void midiNoteOn (int note, int velocity) noexcept
    {
        if (params.source == EngineParams::followMidi)
        {
            harmony.noteOn (note);   // heard, not played
            return;
        }
        if (params.source != EngineParams::midiInput)
            return;

        const bool legato = numHeld > 0;
        removeHeld (note);
        if (numHeld == (int) held.size())
            removeHeld (held[0]);
        held[(std::size_t) numHeld++] = note;

        lastMidiAccent = velocity >= params.accentVelocity;
        voice.noteOn (inKey (note), lastMidiAccent, legato);
    }

    void midiNoteOff (int note) noexcept
    {
        if (params.source == EngineParams::followMidi)
        {
            harmony.noteOff (note);
            return;
        }
        if (params.source != EngineParams::midiInput || numHeld == 0)
            return;

        const bool wasCurrent = held[(std::size_t) (numHeld - 1)] == note;
        if (! removeHeld (note))
            return;

        if (numHeld == 0)
            voice.noteOff();
        else if (wasCurrent)
            voice.noteOn (inKey (held[(std::size_t) (numHeld - 1)]), lastMidiAccent, true); // glide back to the held key
    }

    void midiAllNotesOff() noexcept
    {
        releaseAllMidiNotes();
        for (int n = 0; n < 128; ++n)
            harmony.noteOff (n);
    }

    const FollowHarmony& getHarmony() const noexcept { return harmony; }

    int getCurrentStep() const noexcept       { return currentStep; }
    int getCurrentNote() const noexcept       { return currentNote; }
    double getEnergy() const noexcept         { return energyNow; }
    // 0..1 through the current energy arc's cycle, or -1 in manual
    double getArcPosition() const noexcept
    {
        const double len = kEnergyArcs[std::clamp (params.energyArc, 0, kNumEnergyArcs - 1)].bars;
        return len > 0.0 ? (arcBars - std::floor (arcBars / len) * len) / len : -1.0;
    }
    int getPlayingPattern() const noexcept    { return currentStep >= 0 ? playingPattern : -1; }
    const Tb303Voice& getVoice() const noexcept { return voice; }

private:
    static constexpr int64_t kNoStep = std::numeric_limits<int64_t>::min();

    // Ghost Hands records the panel knobs, or plays its take in their place. Then the
    // cheat panel goes on top: the squelch macro and the auto tweak, a smooth random
    // walk of the main knobs that moves in time with the beat.
    VoiceParams performParams (bool clockRunning, double ppq) noexcept
    {
        VoiceParams v = params.voice;
        const double e = 2.0 * (energyNow - 0.5);   // -1..1

        GhostParams g = params.ghost;
        g.variation = detail::clamp01 (g.variation + 0.3 * e);   // wilder ghost at high energy
        ghost.process (v, clockRunning, ppq, ghostBeatFlags (clockRunning, ppq), g);

        v.cutoff    += 0.18 * e;
        v.envMod    += 0.15 * e;
        v.resonance += 0.10 * e;
        v.accent    += 0.15 * e;
        v.decay     += 0.10 * e;
        if (v.acidMode)
        {
            v.overdrive  += 0.25 * e;
            v.distortion += 0.20 * e;
        }

        const double m = detail::clamp01 (params.squelch);
        v.cutoff    += 0.30 * m;
        v.envMod    += 0.35 * m;
        v.resonance += 0.30 * m;
        v.accent    += 0.30 * m;
        v.decay     += 0.10 * m;
        if (v.acidMode)
        {
            v.overdrive  += 0.35 * m;
            v.distortion += 0.25 * m;
        }

        const double t = detail::clamp01 (params.tweak);
        if (clockRunning && t > 0.0)
        {
            const double beats = std::max (1.0, params.tweakBeats);
            const auto segment = (int64_t) std::floor (ppq / beats);
            if (segment != tweakSegment)
            {
                const bool contiguous = segment == tweakSegment + 1;
                for (std::size_t k = 0; k < tweakFrom.size(); ++k)
                {
                    tweakFrom[k] = contiguous ? tweakTo[k] : randomBipolar();
                    tweakTo[k] = randomBipolar();
                }
                tweakSegment = segment;
            }
            const double f = ppq / beats - (double) segment;
            const double ease = f * f * (3.0 - 2.0 * f);
            auto walk = [&] (std::size_t k) { return tweakFrom[k] + (tweakTo[k] - tweakFrom[k]) * ease; };
            v.cutoff    += 0.28 * t * walk (0);
            v.envMod    += 0.25 * t * walk (1);
            v.decay     += 0.30 * t * walk (2);
            v.resonance += 0.15 * t * walk (3);
            v.accent    += 0.20 * t * walk (4);
        }

        for (double* x : { &v.cutoff, &v.envMod, &v.resonance, &v.accent, &v.decay, &v.overdrive, &v.distortion })
            *x = detail::clamp01 (*x);
        return v;
    }

    void updateEnergy (bool clockRunning, double ppq) noexcept
    {
        double e = detail::clamp01 (params.energy);
        if (params.energyArc > 0)
        {
            if (clockRunning)
                arcBars = ppq / 4.0;
            e = arcEnergy (params.energyArc, arcBars);
        }
        energyNow = e;

        const double up = std::max (0.0, 2.0 * (e - 0.5)), down = std::max (0.0, 2.0 * (0.5 - e));
        energyPump = 0.35 * up;
        energyWash = 0.30 * down;
        // Delay throw on the last beat of every 4-bar phrase when it's going off
        const bool phraseEnd = clockRunning && ghost::wrap ((int64_t) std::floor (ppq), 16) == 15;
        energyThrow = phraseEnd ? 0.45 * detail::clamp01 ((e - 0.7) / 0.3) : 0.0;
    }

    Step energise (Step s, int index, int64_t absStep) const noexcept
    {
        return energiseStep (s, index, absStep, energyNow);
    }

    // Accents and slides the pattern has on the current beat, so Ghost Hands can follow it
    int ghostBeatFlags (bool clockRunning, double ppq) const noexcept
    {
        if (! clockRunning || params.source != EngineParams::internalSequencer)
            return 0;

        const int len = std::clamp (params.length, 1, kNumSteps);
        const auto firstStep = (int64_t) std::floor (ppq) * 4;
        int flags = 0;
        for (int k = 0; k < 4; ++k)
        {
            const Step s = patterns[(std::size_t) playingPattern].get ((int) (((firstStep + k) % len + len) % len));
            if (s.gate && s.accent) flags |= ghost::accentFlag;
            if (s.gate && s.slide)  flags |= ghost::slideFlag;
        }
        return flags;
    }

        // Scale lock for MIDI input, relative to the root note
    int inKey (int midiNote) const noexcept
    {
        return std::clamp (params.root + snapToScale (midiNote - params.root, (Scale) params.scale), 0, 127);
    }

    double randomBipolar() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return (double) (rngState >> 8) / 8388608.0 - 1.0;
    }

    double currentBpm() const noexcept
    {
        const bool hostTempo = params.clock == EngineParams::hostSync || params.source == EngineParams::midiInput;
        return std::max (20.0, hostTempo && transport.bpm > 0.0 ? transport.bpm : params.bpm);
    }

    // Musical position for this sample. False when nothing is running (sequencer
    // stopped, or host stopped in host-sync / MIDI mode).
    bool tickClock (double& ppq) noexcept
    {
        const bool midi = params.source == EngineParams::midiInput;

        if (midi || params.clock == EngineParams::hostSync)
        {
            freeRunning = false;
            if (! (transport.playing && transport.hasPpq && (midi || params.run)))
                return false;
            ppq = transport.ppq + sampleInBlock * currentBpm() / (60.0 * fs);
            return true;
        }

        if (! params.run)
        {
            freeRunning = false;
            return false;
        }

        if (! freeRunning)
            freePpq = 0.0;
        freeRunning = true;
        ppq = freePpq;
        freePpq += params.bpm / (60.0 * fs);
        return true;
    }

    void advance (double ppq) noexcept
    {
        // Steps are 16th notes. Shuffle delays every second 16th inside each 8th.
        const double eighths = ppq * 2.0;
        const double eighthIndex = std::floor (eighths);
        const double inEighth = eighths - eighthIndex;
        const double swing = 0.5 + 0.25 * detail::clamp01 (params.shuffle);

        int64_t absStep = (int64_t) eighthIndex * 2;
        double stepFrac;
        if (inEighth < swing)
        {
            stepFrac = inEighth / swing;
        }
        else
        {
            ++absStep;
            stepFrac = (inEighth - swing) / (1.0 - swing);
        }

        if (absStep != lastAbsStep)
        {
            const bool contiguous = lastAbsStep != kNoStep && absStep == lastAbsStep + 1;
            const int len = std::clamp (params.length, 1, kNumSteps);
            const int index = (int) (((absStep % len) + len) % len);

            // Like the 303, a newly selected pattern takes over when the current one ends
            if (index == 0 || ! contiguous)
                playingPattern = std::clamp (params.pattern, 0, kNumPatterns - 1);

            const Step step = energise (patterns[(std::size_t) playingPattern].get (index), index, absStep);

            // 303 slide: a slide on the previous step held its gate open, so this note
            // glides in without retriggering the envelopes.
            const bool slideIn = contiguous && gateHigh && currentStepData.gate && currentStepData.slide;

            currentHit = 0;
            if (step.gate)
            {
                if (params.source == EngineParams::followMidi)
                    currentNote = harmony.noteFor (step.note, params.root, (Role) params.role, (Scale) params.scale);
                else
                    currentNote = std::clamp (params.root + snapToScale (step.note, (Scale) params.scale), 0, 127);
                voice.noteOn (currentNote, step.accent, slideIn);
                gateHigh = true;
            }
            else if (gateHigh)
            {
                voice.noteOff();
                gateHigh = false;
            }

            currentStepData = step;
            currentStep = index;
            lastAbsStep = absStep;
        }

        // A rolled step is played as several quick hits: the first keeps the step's accent,
        // the rest are plain and each has its own half-length gate.
        const int hits = currentStepData.hits();
        const double scaled = stepFrac * hits;
        const int hit = std::min (hits - 1, (int) scaled);
        if (hit != currentHit)
        {
            currentHit = hit;
            voice.noteOn (currentNote, false, false);
            gateHigh = true;
        }

        if (gateHigh && ! currentStepData.slide && scaled - hit >= kGateLength)
        {
            voice.noteOff();
            gateHigh = false;
        }
    }

    void stopSequencer() noexcept
    {
        if (gateHigh)
            voice.noteOff();
        gateHigh = false;
        wasRunning = false;
        lastAbsStep = kNoStep;
        currentStep = -1;
        currentStepData = {};
    }

    bool removeHeld (int note) noexcept
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (held[(std::size_t) i] == note)
            {
                for (int j = i + 1; j < numHeld; ++j)
                    held[(std::size_t) (j - 1)] = held[(std::size_t) j];
                --numHeld;
                return true;
            }
        }
        return false;
    }

    void releaseAllMidiNotes() noexcept
    {
        if (numHeld > 0)
            voice.noteOff();
        numHeld = 0;
    }

    Tb303Voice voice;
    FollowHarmony harmony;
    PingPongDelay delay;
    Reverb reverb;
    double fxCoef = 0.0, delayMix = 0.0, reverbMix = 0.0;
    uint32_t controlCounter = 0, rngState = 0x303ac1d5u;
    double energyNow = 0.5, arcBars = 0.0, energyPump = 0.0, energyWash = 0.0, energyThrow = 0.0;
    int64_t tweakSegment = std::numeric_limits<int64_t>::min();
    std::array<double, 5> tweakFrom {}, tweakTo {};
    bool freeRunning = false;
    EngineParams params;
    Transport transport;
    double fs = 44100.0;
    int sampleInBlock = 0;

    // Sequencer
    double freePpq = 0.0;
    bool wasRunning = false, gateHigh = false;
    int64_t lastAbsStep = kNoStep;
    int currentStep = -1;
    int currentHit = 0, currentNote = 0;   // which hit of a rolled step is playing, and its pitch
    int playingPattern = 0;
    Step currentStepData {};

    // MIDI input: held keys, last-note priority
    std::array<int, 16> held {};
    int numHeld = 0;
    bool lastMidiAccent = false;
};
} // namespace acid
