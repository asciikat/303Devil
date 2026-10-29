#pragma once

// GHOST HANDS: records how you ride the six performance knobs in time with the beat.
//  - LOOP plays the take back, with some variation so it never repeats exactly.
//  - LEARN improvises new moves in your style from what GhostBrain learned, following
//    the pattern's accents and slides and the shape of the 4-bar phrase.
// Grab a knob while the ghost plays and it lets go of that knob; leave it for 2 bars
// and the ghost takes it back, and learns from what you did. Everything is stored in
// beats, so a take recorded at 130 BPM plays back in time at 140. Plain C++17, no JUCE.

#include "GhostBrain.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace acid
{
struct GhostParams
{
    enum Mode { loop = 0, learn = 1 };

    bool   perform   = false;
    int    mode      = learn;
    double amount    = 1.0;   // 0 = your knobs, 1 = the ghost's
    double variation = 0.3;   // LOOP: bar swaps and depth. LEARN: how adventurous.
    int    bars      = 4;     // take length when recording
};

//==============================================================================
// A recorded take. Written by the audio thread when a recording ends, or by the
// message thread when a project loads; read by both. Values are knob positions 0..1.
class GhostTake
{
public:
    float get (int tick, int lane) const noexcept
    {
        return values[(std::size_t) (tick * ghost::kNumLanes + lane)].load (std::memory_order_relaxed);
    }

    void set (int tick, int lane, float v) noexcept
    {
        values[(std::size_t) (tick * ghost::kNumLanes + lane)].store (v, std::memory_order_relaxed);
    }

    // The pattern's accents and slides on each beat while you recorded (ghost::BeatFlags)
    int getBeatFlags (int beat) const noexcept { return flags[(std::size_t) beat].load (std::memory_order_relaxed); }
    void setBeatFlags (int beat, int f) noexcept { flags[(std::size_t) beat].store ((uint8_t) f, std::memory_order_relaxed); }

    int getBars() const noexcept    { return bars.load (std::memory_order_acquire); }
    int getPhase() const noexcept   { return phase.load (std::memory_order_relaxed); }
    int getPhrase() const noexcept  { return phrase.load (std::memory_order_relaxed); }
    int getVersion() const noexcept { return version.load(); }

    // Publishes the values set so far. phaseBars = which bar of the loop the take starts
    // on; phraseBars = which bar of a 4-bar phrase it starts on.
    void commit (int numBars, int phaseBars, int phraseBars) noexcept
    {
        phase.store (phaseBars, std::memory_order_relaxed);
        phrase.store (phraseBars & 3, std::memory_order_relaxed);
        bars.store (std::clamp (numBars, 0, ghost::kMaxBars), std::memory_order_release);
        ++version;
    }

    void clear() noexcept { commit (0, 0, 0); }

private:
    std::array<std::atomic<float>, (std::size_t) (ghost::kMaxTicks * ghost::kNumLanes)> values {};
    std::array<std::atomic<uint8_t>, (std::size_t) ghost::kMaxTakeBeats> flags {};
    std::atomic<int> bars { 0 }, phase { 0 }, phrase { 0 }, version { 0 };
};

//==============================================================================
class GhostHands
{
public:
    enum RecState { idle = 0, armed = 1, recording = 2 };

    GhostTake take;

    GhostHands() : brains (std::make_unique<std::array<ghost::Brain, 3>>())
    {
        stopPerforming();
    }

    //==========================================================================
    // Message thread

    // REC button. Idle -> armed (starts on the next bar); armed or recording -> stop
    // (a recording keeps its complete bars).
    void toggleRecord() noexcept
    {
        command.store (recState.load() == idle ? cmdArm : cmdStop);
    }

    // True when the take changed or you taught the ghost something new by hand
    bool needsLearning() const noexcept
    {
        return take.getVersion() != learnedTakeVersion || learnedDirty || fifoWrite.load() != fifoRead.load();
    }

    // Rebuilds the brain from the take and the beats learned live, and hands it to the
    // audio thread. Takes well under a millisecond.
    void learn()
    {
        drainTouches();
        learnedDirty = false;
        learnedTakeVersion = take.getVersion();

        // Three brains: the one playing, the one the audio thread may be about to pick
        // up, and a free one to build into
        const int active = activeBrain.load(), inUse = brainInUse.load();
        int target = 0;
        while (target == active || target == inUse)
            ++target;
        auto& brain = (*brains)[(std::size_t) target];

        const int numTakeBeats = take.getBars() * ghost::kBeatsPerBar;
        const int64_t firstBeat = (int64_t) take.getPhrase() * ghost::kBeatsPerBar;
        int n = 0;
        for (int b = 0; b < numTakeBeats; ++b, ++n)
        {
            auto& s = brain.beats[(std::size_t) n];
            for (int t = 0; t < ghost::kTicksPerBeat; ++t)
                for (int lane = 0; lane < ghost::kNumLanes; ++lane)
                    s.set (t, lane, take.get (b * ghost::kTicksPerBeat + t, lane));
            s.context = (uint8_t) ghost::makeContext (firstBeat + b, take.getBeatFlags (b));
            s.followsPrevious = b > 0;
            brain.next[(std::size_t) n] = (int16_t) ((b + 1) % numTakeBeats);   // the take loops
        }
        if (numTakeBeats == 0)
            learned.clear();   // live beats only make sense next to a take

        for (const auto& s : learned)
        {
            brain.beats[(std::size_t) n] = s;
            brain.next[(std::size_t) n] = -1;
            if (s.followsPrevious && n > numTakeBeats)
                brain.next[(std::size_t) (n - 1)] = (int16_t) n;
            ++n;
        }

        brain.numBeats = n;
        brain.numTakeBeats = numTakeBeats;
        brain.learn();
        activeBrain.store (target);

        moveTypes.store (brain.numTypes, std::memory_order_relaxed);
        learnedCount.store ((int) learned.size(), std::memory_order_relaxed);
    }

    void clearLearned()
    {
        drainTouches();
        learned.clear();
        learnedDirty = true;
    }

    // For saving and loading with the project (message thread)
    const std::vector<ghost::BeatSample>& getLearnedBeats() const noexcept { return learned; }
    void setLearnedBeats (std::vector<ghost::BeatSample> beats)
    {
        drainTouches();
        learned = std::move (beats);
        if ((int) learned.size() > ghost::kMaxLearnedBeats)
            learned.erase (learned.begin(), learned.end() - ghost::kMaxLearnedBeats);
        learnedDirty = true;
    }

    // Status for the editor
    int getRecState() const noexcept           { return recState.load (std::memory_order_relaxed); }
    float getRecordProgress() const noexcept   { return recordProgress.load (std::memory_order_relaxed); } // 0..1
    float getPlayPosition() const noexcept     { return playPosition.load (std::memory_order_relaxed); }   // 0..1 of the take, -1 = none
    bool isPlayingLiveMove() const noexcept    { return playingLiveMove.load (std::memory_order_relaxed); } // a beat learned by hand
    int getCompletedTakes() const noexcept     { return completedTakes.load (std::memory_order_relaxed); }
    int getMoveTypes() const noexcept          { return moveTypes.load (std::memory_order_relaxed); }
    int getLearnedCount() const noexcept       { return learnedCount.load (std::memory_order_relaxed); }
    bool isLaneYours (int lane) const noexcept { return yours[(std::size_t) lane].load (std::memory_order_relaxed); }

    // What the ghost is doing to each knob (0..1), or -1 when it isn't playing that knob
    float getOutput (int lane) const noexcept  { return output[(std::size_t) lane].load (std::memory_order_relaxed); }

    //==========================================================================
    // Audio thread

    void reset() noexcept
    {
        if (recState.load() == recording)
            recState.store (armed);
        stopPerforming();
    }

    // At control rate. v holds the panel knobs; the ghost replaces the six performance
    // knobs with its own moves, blended by the amount. beatFlags = the pattern's accents
    // and slides on the current beat.
    void process (VoiceParams& v, bool clockRunning, double ppq, int beatFlags, const GhostParams& p) noexcept
    {
        handleCommand();

        if (! clockRunning)
        {
            if (recState.load() == recording)
                finishRecording();
            stopPerforming();
            return;
        }

        const double ticks = ppq * ghost::kTicksPerBeat;
        const auto tick = (int64_t) std::floor (ticks);

        if (recState.load() == armed)
            maybeStartRecording (tick, p.bars);
        if (recState.load() == recording)
        {
            record (v, tick, beatFlags);
            stopPerforming();
            return;
        }

        if (! p.perform || take.getBars() == 0)
        {
            stopPerforming();
            return;
        }

        std::array<double, ghost::kNumLanes> ghostValues {};
        const bool improvised = p.mode == GhostParams::learn && improvise (ticks, beatFlags, p.variation, ghostValues);
        if (! improvised)
            loop (ticks, p.variation, ghostValues);
        playingLiveMove.store (improvised && playPosition.load() < 0.0f, std::memory_order_relaxed);

        handOff (v, ticks, ghostValues, std::clamp (p.amount, 0.0, 1.0), beatFlags);
    }

private:
    static constexpr int64_t kNone = std::numeric_limits<int64_t>::min();
    static constexpr double kHandOffBeats = 8.0;   // 2 bars without touching = ghost takes the knob back
    static constexpr int kFifoSize = 16;
    enum Command { cmdNone = 0, cmdArm, cmdStop };

    struct BarChoice { int source = 0; double depth = 1.0; };

    void handleCommand() noexcept
    {
        switch (command.exchange (cmdNone))
        {
            case cmdArm:
                if (recState.load() == idle)
                {
                    recordProgress.store (0.0f);
                    recState.store (armed);
                }
                break;
            case cmdStop:
                if (recState.load() == recording)
                    finishRecording();
                else
                    recState.store (idle);
                break;
            default:
                break;
        }
    }

    //==========================================================================
    // Recording

    // Recording starts on a bar line, so the take lines up with your phrase. Arming
    // inside the first sixteenth of a bar still catches that bar.
    void maybeStartRecording (int64_t tick, int bars) noexcept
    {
        const int64_t bar = ghost::floorDiv (tick, ghost::kTicksPerBar);
        if (tick - bar * ghost::kTicksPerBar >= ghost::kTicksPerBeat / 4)
            return;

        recStartBar = bar;
        recTargetBars = std::clamp (bars, 1, ghost::kMaxBars);
        recLastTick = -1;
        recordProgress.store (0.0f);
        recState.store (recording);
    }

    void record (const VoiceParams& v, int64_t tick, int beatFlags) noexcept
    {
        const int64_t local = tick - recStartBar * ghost::kTicksPerBar;
        const int total = recTargetBars * ghost::kTicksPerBar;

        // The host jumped backwards (loop or relocate): keep what we have
        if (local < recLastTick)
        {
            finishRecording();
            return;
        }

        VoiceParams knobs = v;
        for (int64_t t = recLastTick + 1; t <= local && t < total; ++t)
        {
            for (int lane = 0; lane < ghost::kNumLanes; ++lane)
                pending[(std::size_t) (t * ghost::kNumLanes + lane)] = (float) ghost::laneValue (knobs, lane);
            if (t % ghost::kTicksPerBeat == 0)
                pendingFlags[(std::size_t) (t / ghost::kTicksPerBeat)] = (uint8_t) beatFlags;
        }

        recLastTick = std::max (recLastTick, std::min<int64_t> (local, total - 1));
        recordProgress.store ((float) (recLastTick + 1) / (float) total);

        if (local >= total)
            finishRecording();
    }

    void finishRecording() noexcept
    {
        const int barsDone = (int) ((recLastTick + 1) / ghost::kTicksPerBar);
        if (barsDone > 0)
        {
            for (int i = 0; i < barsDone * ghost::kTicksPerBar; ++i)
                for (int lane = 0; lane < ghost::kNumLanes; ++lane)
                    take.set (i, lane, pending[(std::size_t) (i * ghost::kNumLanes + lane)]);
            for (int b = 0; b < barsDone * ghost::kBeatsPerBar; ++b)
                take.setBeatFlags (b, pendingFlags[(std::size_t) b]);
            take.commit (barsDone, ghost::wrap (recStartBar, barsDone), ghost::wrap (recStartBar, 4));
            ++completedTakes;
        }
        recState.store (idle);
    }

    //==========================================================================
    // LOOP: the take, bar by bar, with variation

    void loop (double ticks, double variation, std::array<double, ghost::kNumLanes>& out) noexcept
    {
        const int bars = take.getBars();
        if (take.getVersion() != meanVersion)
            computeMeans (bars);

        const auto absTick = (int64_t) std::floor (ticks);
        const int64_t bar = ghost::floorDiv (absTick, ghost::kTicksPerBar);
        const int slot = ghost::wrap (bar - take.getPhase(), bars);

        if (bar != currentBar)
        {
            const bool contiguous = currentBar != kNone && bar == currentBar + 1;
            previous = contiguous ? current : BarChoice { ghost::wrap (slot - 1, bars), 1.0 };
            current = chooseBar (slot, bars, std::clamp (variation, 0.0, 1.0));
            currentBar = bar;
        }

        const double inBar = ticks - (double) bar * ghost::kTicksPerBar;   // 0..96
        // Ease from where the previous bar was heading, so a swapped bar doesn't jump
        const double fade = std::min (1.0, inBar / (ghost::kTicksPerBeat * 0.5));
        const BarChoice carryOn { (previous.source + 1) % bars, previous.depth };

        for (int lane = 0; lane < ghost::kNumLanes; ++lane)
        {
            double value = shaped (current, inBar, lane, bars);
            if (fade < 1.0)
            {
                const double from = shaped (carryOn, inBar, lane, bars);
                value = from + (value - from) * fade;
            }
            out[(std::size_t) lane] = detail::clamp01 (value);
        }
        genBeat = kNone;   // LEARN starts fresh if you switch over
        playPosition.store ((float) (((double) slot * ghost::kTicksPerBar + inBar) / (double) (bars * ghost::kTicksPerBar)));
    }

    // Take value with this bar's variation: depth scales the move around the lane's average
    double shaped (const BarChoice& c, double inBar, int lane, int bars) const noexcept
    {
        const auto i = (int) std::floor (inBar);
        const double f = inBar - i;
        const int t0 = c.source * ghost::kTicksPerBar + std::clamp (i, 0, ghost::kTicksPerBar - 1);
        const int t1 = (t0 + 1) % (bars * ghost::kTicksPerBar);
        const double raw = take.get (t0, lane) + (take.get (t1, lane) - take.get (t0, lane)) * f;
        const double mean = means[(std::size_t) lane];
        return mean + (raw - mean) * c.depth;
    }

    // Variation: sometimes play a different bar of the take here, and push the moves
    // a little bigger or smaller. In takes of 4+ bars the last bar (usually the
    // turnaround) stays put.
    BarChoice chooseBar (int slot, int bars, double variation) noexcept
    {
        BarChoice c { slot, 1.0 };
        if (variation <= 0.0)
            return c;

        const int swappable = bars >= 4 ? bars - 1 : bars;
        if (swappable > 1 && slot < swappable && uniform() < 0.5 * variation)
        {
            const int pick = (int) (uniform() * (swappable - 1)) % (swappable - 1);   // any other swappable bar
            c.source = pick >= slot ? pick + 1 : pick;
        }
        c.depth = 1.0 + variation * (uniform() * 0.8 - 0.45);   // 0.55x .. 1.35x at full variation
        return c;
    }

    void computeMeans (int bars) noexcept
    {
        const int n = bars * ghost::kTicksPerBar;
        for (int lane = 0; lane < ghost::kNumLanes; ++lane)
        {
            double sum = 0.0;
            for (int t = 0; t < n; ++t)
                sum += take.get (t, lane);
            means[(std::size_t) lane] = n > 0 ? sum / n : 0.0;
        }
        meanVersion = take.getVersion();
    }

    //==========================================================================
    // LEARN: one beat at a time, the brain picks a move type for this situation and
    // plays a real beat of yours of that type

    bool improvise (double ticks, int beatFlags, double variation, std::array<double, ghost::kNumLanes>& out) noexcept
    {
        const auto beat = ghost::floorDiv ((int64_t) std::floor (ticks), ghost::kTicksPerBeat);
        if (beat != genBeat)
        {
            const bool contiguous = genBeat != kNone && beat == genBeat + 1;

            int index;
            do
            {
                index = activeBrain.load();
                brainInUse.store (index);
            } while (activeBrain.load() != index);

            if (index != genBrain)   // move and beat numbers belong to the brain that made them
            {
                genType = genExemplar = -1;
                genBrain = index;
            }
            if (index < 0 || (*brains)[(std::size_t) index].numTypes == 0)
            {
                genBeat = kNone;
                return false;
            }

            const auto& brain = (*brains)[(std::size_t) index];
            const int context = ghost::makeContext (beat, beatFlags);
            const double temperature = 0.25 + 1.5 * std::clamp (variation, 0.0, 1.0);
            const int type = brain.chooseType (contiguous ? genType : -1, context, temperature, uniform());
            const int exemplar = brain.chooseExemplar (type, context, contiguous ? genExemplar : -1, uniform(), uniform());

            // Fade in over the first eighth of a beat unless this beat really followed the last one
            genFade = contiguous && ! (genExemplar >= 0 && brain.next[(std::size_t) genExemplar] == exemplar);
            genFrom = lastGhost;
            genType = type;
            genExemplar = exemplar;
            genBeat = beat;
        }

        const auto& brain = (*brains)[(std::size_t) genBrain];
        const auto& sample = brain.beats[(std::size_t) genExemplar];
        const double inBeat = ticks - (double) beat * ghost::kTicksPerBeat;   // 0..24
        const int i = std::clamp ((int) std::floor (inBeat), 0, ghost::kTicksPerBeat - 1);
        const int i1 = std::min (i + 1, ghost::kTicksPerBeat - 1);
        const double f = inBeat - i;
        constexpr double fadeTicks = 3.0;

        for (int lane = 0; lane < ghost::kNumLanes; ++lane)
        {
            double value = sample.get (i, lane) + (sample.get (i1, lane) - sample.get (i, lane)) * f;
            if (genFade && inBeat < fadeTicks)
                value = genFrom[(std::size_t) lane] + (value - genFrom[(std::size_t) lane]) * inBeat / fadeTicks;
            out[(std::size_t) lane] = detail::clamp01 (value);
        }

        currentBar = kNone;   // LOOP starts fresh if you switch over
        playPosition.store (genExemplar < brain.numTakeBeats
                                ? (float) ((genExemplar * ghost::kTicksPerBeat + inBeat) / (brain.numTakeBeats * ghost::kTicksPerBeat))
                                : -1.0f);
        return true;
    }

    //==========================================================================
    // Hand-off: a knob you move is yours for 2 bars after you let go, then the ghost
    // eases back in over a beat. Beats where you had a knob get learned.

    void handOff (VoiceParams& v, double ticks, const std::array<double, ghost::kNumLanes>& ghostValues,
                  double amount, int beatFlags) noexcept
    {
        const double beats = ticks / ghost::kTicksPerBeat;
        const double elapsed = primed ? std::clamp (beats - lastBeats, 0.0, 1.0) : 0.0;
        bool anyYours = false;

        for (int lane = 0; lane < ghost::kNumLanes; ++lane)
        {
            const auto l = (std::size_t) lane;
            double& knob = ghost::laneValue (v, lane);

            // Compared with where the knob was at the last touch, so slow turns count too
            if (! primed || std::abs (knob - lastHuman[l]) > 0.003)
            {
                if (primed)
                    handOffUntil[l] = beats + kHandOffBeats;
                lastHuman[l] = knob;
            }
            handOffUntil[l] = std::min (handOffUntil[l], beats + kHandOffBeats);   // after a jump back in time

            const bool mine = beats < handOffUntil[l];
            mix[l] = mine ? std::max (0.0, mix[l] - elapsed * 16.0)   // let go within 1/16 beat
                          : std::min (1.0, mix[l] + elapsed);        // take back over a beat
            anyYours = anyYours || mix[l] < 0.5;

            lastGhost[l] = ghostValues[l];
            knob += (ghostValues[l] - knob) * amount * mix[l];
            output[l].store (mix[l] > 0.05 ? (float) knob : -1.0f, std::memory_order_relaxed);
            yours[l].store (mine, std::memory_order_relaxed);
        }

        primed = true;
        lastBeats = beats;
        captureTouch (v, ticks, anyYours, beatFlags);
    }

    void captureTouch (const VoiceParams& v, double ticks, bool touched, int beatFlags) noexcept
    {
        const auto tick = (int64_t) std::floor (ticks);
        const auto beat = ghost::floorDiv (tick, ghost::kTicksPerBeat);

        if (beat != capBeat)
        {
            if (capBeat != kNone && capComplete && capTouched && capLastTick == ghost::kTicksPerBeat - 1)
                pushTouch();
            capComplete = capBeat != kNone && beat == capBeat + 1;   // we were here for the whole beat
            capBeat = beat;
            capLastTick = -1;
            capTouched = false;
            capSample.context = (uint8_t) ghost::makeContext (beat, beatFlags);
        }

        VoiceParams knobs = v;
        const auto local = (int) (tick - beat * ghost::kTicksPerBeat);
        for (int t = capLastTick + 1; t <= local; ++t)
            for (int lane = 0; lane < ghost::kNumLanes; ++lane)
                capSample.set (t, lane, (float) ghost::laneValue (knobs, lane));
        capLastTick = std::max (capLastTick, local);
        capTouched = capTouched || touched;
    }

    void pushTouch() noexcept
    {
        const int w = fifoWrite.load (std::memory_order_relaxed);
        if (w - fifoRead.load (std::memory_order_acquire) >= kFifoSize)
            return;   // the message thread is behind; drop this beat
        capSample.followsPrevious = lastPushedBeat != kNone && capBeat == lastPushedBeat + 1;
        fifo[(std::size_t) (w % kFifoSize)] = capSample;
        fifoWrite.store (w + 1, std::memory_order_release);
        lastPushedBeat = capBeat;
    }

    // Message thread: moves beats learned by hand from the FIFO to the learned list
    void drainTouches()
    {
        int r = fifoRead.load (std::memory_order_relaxed);
        const int w = fifoWrite.load (std::memory_order_acquire);
        for (; r != w; ++r)
        {
            learned.push_back (fifo[(std::size_t) (r % kFifoSize)]);
            if ((int) learned.size() > ghost::kMaxLearnedBeats)
                learned.erase (learned.begin());
            learnedDirty = true;
        }
        fifoRead.store (r, std::memory_order_release);
    }

    void stopPerforming() noexcept
    {
        currentBar = genBeat = capBeat = kNone;
        primed = false;
        mix.fill (1.0);
        handOffUntil.fill (-1.0e9);
        playPosition.store (-1.0f);
        playingLiveMove.store (false);
        setOutputsInactive();
    }

    void setOutputsInactive() noexcept
    {
        for (int lane = 0; lane < ghost::kNumLanes; ++lane)
        {
            output[(std::size_t) lane].store (-1.0f, std::memory_order_relaxed);
            yours[(std::size_t) lane].store (false, std::memory_order_relaxed);
        }
    }

    double uniform() noexcept   // xorshift32, 0..1
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (double) (rng >> 8) / 16777216.0;
    }

    // Shared between threads
    std::atomic<int> command { cmdNone }, recState { idle }, completedTakes { 0 };
    std::atomic<float> recordProgress { 0.0f }, playPosition { -1.0f };
    std::atomic<bool> playingLiveMove { false };
    std::array<std::atomic<float>, ghost::kNumLanes> output {};
    std::array<std::atomic<bool>, ghost::kNumLanes> yours {};
    std::unique_ptr<std::array<ghost::Brain, 3>> brains;
    std::atomic<int> activeBrain { -1 }, brainInUse { -1 }, moveTypes { 0 }, learnedCount { 0 };
    std::array<ghost::BeatSample, kFifoSize> fifo {};
    std::atomic<int> fifoWrite { 0 }, fifoRead { 0 };

    // Message thread only
    std::vector<ghost::BeatSample> learned;
    int learnedTakeVersion = -1;
    bool learnedDirty = true;

    // Audio thread only: recording
    std::array<float, (std::size_t) (ghost::kMaxTicks * ghost::kNumLanes)> pending {};
    std::array<uint8_t, (std::size_t) ghost::kMaxTakeBeats> pendingFlags {};
    int64_t recStartBar = 0, recLastTick = -1;
    int recTargetBars = 4;

    // LOOP
    int64_t currentBar = kNone;
    BarChoice current, previous;
    std::array<double, ghost::kNumLanes> means {};
    int meanVersion = -1;

    // LEARN
    int64_t genBeat = kNone;
    int genBrain = -1, genType = -1, genExemplar = -1;
    bool genFade = false;
    std::array<double, ghost::kNumLanes> genFrom {}, lastGhost {};

    // Hand-off and learning by touch
    bool primed = false;
    double lastBeats = 0.0;
    std::array<double, ghost::kNumLanes> lastHuman {}, mix {}, handOffUntil {};
    int64_t capBeat = kNone, lastPushedBeat = kNone;
    int capLastTick = -1;
    bool capComplete = false, capTouched = false;
    ghost::BeatSample capSample;

    uint32_t rng = 0x6705757u;
};
} // namespace acid
