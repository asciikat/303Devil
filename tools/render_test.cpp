// Offline test for the DSP engine: checks the diode ladder maths, then renders a few
// patches to WAV so they can be listened to without a DAW.
//
//   g++ -std=c++17 -O2 -I Source tools/render_test.cpp -o build/acid_render
//   ./build/acid_render renders

#include "dsp/AcidEngine.h"
#include "dsp/FactoryGhosts.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
 #include <direct.h>
#endif

namespace
{
constexpr int kSampleRate = 44100;

void writeWav (const std::string& path, const std::vector<float>& samples)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr)
    {
        std::printf ("  could not write %s\n", path.c_str());
        return;
    }

    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t dataBytes = (uint32_t) samples.size() * 2;

    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes);
    std::fwrite ("WAVEfmt ", 1, 8, f); u32 (16); u16 (1); u16 (1);
    u32 (kSampleRate); u32 (kSampleRate * 2); u16 (2); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);

    for (float s : samples)
    {
        const float c = std::clamp (s, -1.0f, 1.0f);
        u16 ((uint16_t) (int16_t) std::lround (c * 32767.0f));
    }
    std::fclose (f);
}

struct Stats { double peak = 0.0, rms = 0.0; bool finite = true; };

Stats measure (const std::vector<float>& x)
{
    Stats s;
    double sum = 0.0;
    for (float v : x)
    {
        if (! std::isfinite (v))
            s.finite = false;
        s.peak = std::max (s.peak, (double) std::abs (v));
        sum += (double) v * v;
    }
    s.rms = x.empty() ? 0.0 : std::sqrt (sum / (double) x.size());
    return s;
}

double toDb (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-9)); }

//==============================================================================
// Ring the ladder with an impulse just below self-oscillation and count zero
// crossings: the ringing frequency should match the requested resonance.
void checkLadder()
{
    std::printf ("Diode ladder checks (2x rate = %d Hz)\n", kSampleRate * 2);
    const double fsOS = kSampleRate * 2.0;

    for (double target : { 300.0, 1000.0, 3000.0, 8000.0 })
    {
        acid::DiodeLadder ladder;
        ladder.prepare (fsOS);
        const double k = 0.995; // fraction of the self-oscillation threshold
        const int n = (int) (fsOS * 0.25);
        int crossings = 0, first = -1, last = -1;
        double prev = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double y = ladder.process (i == 0 ? 0.05 : 0.0, target, k);
            if (i > n / 10 && prev <= 0.0 && y > 0.0)
            {
                if (first < 0) first = i;
                last = i;
                ++crossings;
            }
            prev = y;
        }

        const double measured = crossings > 1 ? (crossings - 1) * fsOS / (double) (last - first) : 0.0;
        std::printf ("  resonance %6.0f Hz -> rings at %8.1f Hz (%+.1f cents)\n", target, measured,
                     measured > 0.0 ? 1200.0 * std::log2 (measured / target) : 0.0);
    }

    // DC gain with no feedback should be 1
    acid::DiodeLadder ladder;
    ladder.prepare (fsOS);
    double y = 0.0;
    for (int i = 0; i < (int) fsOS; ++i)
        y = ladder.process (0.01, 1000.0, 0.0);
    std::printf ("  DC gain (k = 0): %.4f\n", y / 0.01);

    // Above the threshold it must self-oscillate but stay bounded
    ladder.reset();
    double peak = 0.0;
    for (int i = 0; i < (int) fsOS; ++i)
    {
        y = ladder.process (i == 0 ? 0.01 : 0.0, 1000.0, 1.08);
        if (i > (int) fsOS / 2)
            peak = std::max (peak, std::abs (y));
    }
    std::printf ("  self-oscillation peak (k = 1.08 x threshold): %.3f\n\n", peak);
}

//==============================================================================
struct Scenario
{
    const char* name;
    double seconds;
    std::function<void (acid::EngineParams&)> setup;
    std::function<void (acid::EngineParams&, double progress)> automate; // optional
};

std::vector<float> renderSequence (const Scenario& sc)
{
    acid::AcidEngine engine;
    acid::EngineParams p;
    p.clock = acid::EngineParams::freeRun;
    p.bpm = 130.0;
    sc.setup (p);
    engine.setParams (p);
    engine.prepare (kSampleRate);

    const int total = (int) (sc.seconds * kSampleRate);
    std::vector<float> out ((size_t) total);
    constexpr int block = 256;

    for (int pos = 0; pos < total; pos += block)
    {
        if (sc.automate)
        {
            sc.automate (p, (double) pos / total);
            engine.setParams (p);
        }
        engine.beginBlock ({});
        engine.render (out.data() + pos, std::min (block, total - pos));
    }
    return out;
}

// MIDI input path: overlapping notes must glide, separate notes must retrigger
std::vector<float> renderMidiLegato()
{
    acid::AcidEngine engine;
    acid::EngineParams p;
    p.source = acid::EngineParams::midiInput;
    engine.setParams (p);
    engine.prepare (kSampleRate);

    struct Event { double time; int note; int velocity; }; // velocity 0 = note off
    const Event events[] = {
        { 0.00, 36, 90 },  { 0.20, 36, 0 },
        { 0.25, 48, 120 }, { 0.40, 43, 90 }, { 0.45, 48, 0 },   // legato: 48 -> 43 glides
        { 0.70, 43, 0 },
        { 0.75, 39, 90 },  { 0.90, 51, 90 }, { 0.95, 39, 0 }, { 1.20, 51, 0 },
    };

    const int total = (int) (1.6 * kSampleRate);
    std::vector<float> out ((size_t) total);
    engine.beginBlock ({});
    int done = 0;
    for (const auto& e : events)
    {
        const int at = (int) (e.time * kSampleRate);
        engine.render (out.data() + done, at - done);
        done = at;
        if (e.velocity > 0)
            engine.midiNoteOn (e.note, e.velocity);
        else
            engine.midiNoteOff (e.note);
    }
    engine.render (out.data() + done, total - done);
    return out;
}

//==============================================================================
// Ghost Hands: record a made-up performance (cutoff opens over each bar, resonance
// rides up across the take), then let the ghost play it with the knobs parked.
std::vector<float> renderGhost (bool& passed)
{
    constexpr double bpm = 130.0;
    const double samplesPerBar = 4.0 * 60.0 / bpm * kSampleRate;

    acid::AcidEngine engine;
    acid::EngineParams p;
    p.clock = acid::EngineParams::freeRun;
    p.bpm = bpm;
    p.voice.envMod = 0.5;
    p.ghost.bars = 2;
    p.ghost.mode = acid::GhostParams::loop;
    engine.setParams (p);
    engine.prepare (kSampleRate);

    auto hands = [] (double bars, acid::VoiceParams& v)
    {
        const double inBar = bars - std::floor (bars);
        v.cutoff = 0.1 + 0.6 * inBar;           // open through every bar
        v.resonance = 0.6 + 0.35 * bars / 2.0;  // more squelch in bar 2
    };

    std::vector<float> out;
    constexpr int block = 64;
    float buf[block];
    double maxError = 0.0, maxVariation = 0.0;
    int checked = 0;
    bool inRange = true;

    engine.ghost.toggleRecord();
    const int total = (int) (12.0 * samplesPerBar);
    for (int pos = 0; pos < total; pos += block)
    {
        const double bars = pos / samplesPerBar;
        if (bars < 2.0)
            hands (bars, p.voice);
        else
        {
            // Hands off: knobs parked, ghost plays. Exact for 2 bars, then with variation.
            p.voice.cutoff = 0.2;
            p.voice.resonance = 0.5;
            p.ghost.perform = true;
            p.ghost.variation = bars < 4.0 ? 0.0 : 0.6;
        }
        engine.setParams (p);
        engine.beginBlock ({});
        engine.render (buf, block);
        out.insert (out.end(), buf, buf + block);

        if (bars >= 2.05 && bars < 3.95)
        {
            acid::VoiceParams expected;
            hands (bars - 2.0, expected);
            const double inBar = bars - std::floor (bars);
            if (inBar > 0.02 && inBar < 0.98)   // the made-up hands jump at each bar line
            {
                maxError = std::max ({ maxError, std::abs (engine.ghost.getOutput (acid::ghost::cutoff) - expected.cutoff),
                                       std::abs (engine.ghost.getOutput (acid::ghost::resonance) - expected.resonance) });
                ++checked;
            }
        }
        else if (bars >= 4.0)
        {
            acid::VoiceParams exact;
            hands (std::fmod (bars, 2.0), exact);
            const double inBar = bars - std::floor (bars);
            const float cut = engine.ghost.getOutput (acid::ghost::cutoff);
            inRange = inRange && cut >= 0.0f && cut <= 1.0f;
            if (inBar > 0.02 && inBar < 0.98)
                maxVariation = std::max (maxVariation, std::abs (cut - exact.cutoff));
        }
    }

    const int bars = engine.ghost.take.getBars();
    passed = bars == 2 && checked > 0 && maxError < 0.03 && inRange && maxVariation > 0.05;
    std::printf ("Ghost Hands: take %d bars, exact replay max error %.4f over %d checks,\n"
                 "             variation 0.6 moves cutoff up to %.3f away from the take -> %s\n\n",
                 bars, maxError, checked, maxVariation, passed ? "ok" : "FAILED");
    return out;
}

// Ghost Hands, LEARN mode. The made-up player opens the cutoff on beats where the
// pattern has an accent and keeps it shut otherwise. They recorded with accents on
// beats 1 and 3; the ghost then plays a pattern with accents on beats 2 and 4, and has to
// follow the new accents. Then a hand grabs the cutoff knob, lets go, and the ghost must
// take it back 2 bars later and have learned the beats played by hand.
std::vector<float> renderGhostLearn (bool& passed)
{
    using namespace acid::ghost;
    constexpr double bpm = 130.0;
    const double samplesPerBeat = 60.0 / bpm * kSampleRate;

    acid::AcidEngine engine;
    auto setAccents = [&engine] (int firstAccentBeat)
    {
        for (int i = 0; i < acid::kNumSteps; ++i)
            engine.patterns[0].set (i, { i % 2 == 0 ? 0 : 12, true, (i / 4) % 2 == firstAccentBeat && i % 4 == 0, false });
    };

    // The take: 4 bars, accents on beats 1 and 3 (index 0 and 2)
    setAccents (0);
    auto& take = engine.ghost.take;
    uint32_t noise = 1;
    for (int beat = 0; beat < 16; ++beat)
    {
        const bool accented = beat % 2 == 0;
        for (int t = 0; t < kTicksPerBeat; ++t)
        {
            noise = noise * 1664525u + 1013904223u;
            const float jitter = (float) (noise >> 8) / 16777216.0f * 0.02f;
            const float cut = accented ? 0.3f + 0.5f * (float) t / kTicksPerBeat : 0.15f;
            for (int lane = 0; lane < kNumLanes; ++lane)
                take.set (beat * kTicksPerBeat + t, lane, lane == cutoff ? cut + jitter : 0.5f + jitter);
        }
        take.setBeatFlags (beat, accented ? accentFlag : 0);
    }
    take.commit (4, 0, 0);
    engine.ghost.learn();
    std::printf ("Ghost Hands LEARN: %d move types from 16 beats\n", engine.ghost.getMoveTypes());

    // Perform with accents moved to beats 2 and 4
    setAccents (1);
    acid::EngineParams p;
    p.clock = acid::EngineParams::freeRun;
    p.bpm = bpm;
    p.voice.cutoff = 0.3;
    p.ghost.perform = true;
    p.ghost.mode = acid::GhostParams::learn;
    p.ghost.variation = 0.3;
    engine.setParams (p);
    engine.prepare (kSampleRate);

    std::vector<float> out;
    constexpr int block = 64;
    float buf[block];
    double sumAccent = 0.0, sumPlain = 0.0;
    int nAccent = 0, nPlain = 0;
    bool letGo = true, otherLanesKept = true, tookBack = false, stillYoursAfterRelease = true;

    const int total = (int) (80.0 * samplesPerBeat);
    for (int pos = 0; pos < total; pos += block)
    {
        const double beats = pos / samplesPerBeat;
        const double inBeat = beats - std::floor (beats);

        // A hand rides the cutoff during beats 48-52, then lets go
        if (beats >= 48.0 && beats < 52.0)
            p.voice.cutoff = 0.4 + 0.3 * std::sin (beats * 2.0);
        engine.setParams (p);
        engine.beginBlock ({});
        engine.render (buf, block);
        out.insert (out.end(), buf, buf + block);

        const float cut = engine.ghost.getOutput (cutoff);
        if (beats >= 4.0 && beats < 48.0 && inBeat > 0.15 && inBeat < 0.95)
        {
            if (((int) beats) % 2 == 1) { sumAccent += cut; ++nAccent; }
            else                        { sumPlain += cut; ++nPlain; }
        }
        if (beats >= 48.2 && beats < 52.0)
        {
            letGo = letGo && cut < 0.0f && engine.ghost.isLaneYours (cutoff);
            otherLanesKept = otherLanesKept && engine.ghost.getOutput (resonance) >= 0.0f;
        }
        if (beats >= 53.0 && beats < 59.5)
            stillYoursAfterRelease = stillYoursAfterRelease && cut < 0.0f;
        if (beats >= 61.5 && beats < 62.0)
            tookBack = cut >= 0.0f && ! engine.ghost.isLaneYours (cutoff);
    }

    const bool wantsLearning = engine.ghost.needsLearning();
    engine.ghost.learn();
    const int learnedByHand = engine.ghost.getLearnedCount();

    const double accentMean = nAccent > 0 ? sumAccent / nAccent : 0.0;
    const double plainMean = nPlain > 0 ? sumPlain / nPlain : 1.0;
    const bool follows = accentMean > 0.45 && plainMean < 0.25;
    passed = follows && letGo && otherLanesKept && stillYoursAfterRelease && tookBack && wantsLearning && learnedByHand > 0;

    std::printf ("  follows new accents: cutoff %.2f on accent beats, %.2f on others -> %s\n", accentMean, plainMean, follows ? "ok" : "FAILED");
    std::printf ("  grab cutoff: ghost lets go %s, keeps other knobs %s, waits 2 bars %s, takes it back %s\n",
                 letGo ? "ok" : "FAILED", otherLanesKept ? "ok" : "FAILED", stillYoursAfterRelease ? "ok" : "FAILED", tookBack ? "ok" : "FAILED");
    std::printf ("  learned %d beats played by hand -> %s\n\n", learnedByHand, wantsLearning && learnedByHand > 0 ? "ok" : "FAILED");
    return out;
}

//==============================================================================
// Phase 3: every factory ghost loads, stays in range, learns, and plays. Two of them
// are checked for the habit they're named after.
bool checkFactoryGhosts()
{
    using namespace acid::ghost;
    std::printf ("Factory ghosts\n");
    bool ok = true;
    for (int i = 0; i < acid::kNumFactoryGhosts; ++i)
    {
        acid::AcidEngine engine;
        acid::writeFactoryGhost (i, engine.ghost.take);
        engine.ghost.learn();

        const int bars = engine.ghost.take.getBars();
        bool inRange = true;
        for (int t = 0; t < bars * kTicksPerBar; ++t)
            for (int lane = 0; lane < kNumLanes; ++lane)
                inRange = inRange && engine.ghost.take.get (t, lane) >= 0.0f && engine.ghost.take.get (t, lane) <= 1.0f;

        // Average cutoff on accented vs plain beats, and in the 4th bar of each phrase vs the rest
        double acc = 0, plain = 0, last = 0, rest = 0;
        int nAcc = 0, nPlain = 0, nLast = 0, nRest = 0;
        for (int beat = 0; beat < bars * kBeatsPerBar; ++beat)
            for (int t = 0; t < kTicksPerBeat; ++t)
            {
                const double c = engine.ghost.take.get (beat * kTicksPerBeat + t, cutoff);
                if (engine.ghost.take.getBeatFlags (beat) & accentFlag) { acc += c; ++nAcc; } else { plain += c; ++nPlain; }
                if ((beat / kBeatsPerBar) % 4 == 3) { last += c; ++nLast; } else { rest += c; ++nRest; }
            }

        bool habit = true;
        if (i == 1) habit = acc / nAcc > plain / nPlain + 0.1;    // Accent Chaser
        if (i == 2) habit = last / nLast < rest / nRest - 0.1;    // Breakdown Closer

        // Play it in LEARN mode for 8 bars: the ghost must stay on and in range
        acid::EngineParams p;
        p.clock = acid::EngineParams::freeRun;
        p.ghost.perform = true;
        p.ghost.mode = acid::GhostParams::learn;
        engine.setParams (p);
        engine.prepare (kSampleRate);
        bool plays = true;
        float buf[256];
        engine.beginBlock ({});
        for (int pos = 256; pos < (int) (8 * 4 * 60.0 / 130.0 * kSampleRate); pos += 256)
        {
            engine.render (buf, 256);
            const float c = engine.ghost.getOutput (cutoff);
            plays = plays && c >= 0.0f && c <= 1.0f;
        }

        const bool good = bars == acid::kFactoryGhosts[i].bars && inRange && engine.ghost.getMoveTypes() >= 2 && habit && plays;
        std::printf ("  %-18s %2d bars, %2d move types, habit %s, plays %s -> %s\n", acid::kFactoryGhosts[i].name, bars,
                     engine.ghost.getMoveTypes(), habit ? "ok" : "MISSING", plays ? "ok" : "NO", good ? "ok" : "FAILED");
        ok = ok && good;
    }
    std::printf ("\n");
    return ok;
}

// Phase 4: arcs, the pattern at different energies, the generator, and the sound
bool checkEnergy()
{
    std::printf ("Energy\n");
    bool ok = true;
    auto check = [&ok] (const char* what, bool good)
    {
        std::printf ("  %-58s %s\n", what, good ? "ok" : "FAILED");
        ok = ok && good;
    };

    bool inRange = true;
    for (int arc = 1; arc < acid::kNumEnergyArcs; ++arc)
        for (double b = -8.0; b < 130.0; b += 0.05)
            inRange = inRange && acid::arcEnergy (arc, b) >= 0.0 && acid::arcEnergy (arc, b) <= 1.0;
    check ("all arcs stay 0..1", inRange);
    check ("build-up rises from bar 0 to bar 15", acid::arcEnergy (1, 0.0) < 0.3 && acid::arcEnergy (1, 15.0) > 0.95);
    check ("build-peak-break-drop: break at bar 22, drop at bar 24",
           acid::arcEnergy (2, 22.0) < 0.2 && acid::arcEnergy (2, 24.01) > 0.95 && acid::arcEnergy (2, 17.0) > 0.9);

    // Pattern at different energies (the stored pattern stays as it is)
    acid::Pattern pattern;
    pattern.loadFromString (acid::kFactoryPatterns[0]);
    auto count = [&pattern] (double e, int64_t phrase, int& gates, int& accents, int& slides)
    {
        gates = accents = slides = 0;
        for (int i = 0; i < acid::kNumSteps; ++i)
        {
            const auto s = acid::energiseStep (pattern.get (i), i, phrase * 64 + i, e);
            gates += s.gate; accents += s.gate && s.accent; slides += s.gate && s.slide;
        }
    };
    int g0, a0, s0, gLow, aLow, sLow, gHigh, aHigh, sHigh;
    count (0.5, 0, g0, a0, s0);
    count (0.05, 0, gLow, aLow, sLow);
    count (1.0, 0, gHigh, aHigh, sHigh);
    std::printf ("  pattern notes/accents/slides: low %d/%d/%d  neutral %d/%d/%d  high %d/%d/%d\n",
                 gLow, aLow, sLow, g0, a0, s0, gHigh, aHigh, sHigh);
    check ("low energy thins the line, high energy busies it up", gLow < g0 && gHigh >= g0 && aHigh > a0 && sHigh >= s0);

    bool sameInPhrase = true, newEachPhrase = false;
    for (int i = 0; i < acid::kNumSteps; ++i)
    {
        const auto a = acid::energiseStep (pattern.get (i), i, i, 0.75), b = acid::energiseStep (pattern.get (i), i, 64 + i, 0.75);
        sameInPhrase = sameInPhrase && a.gate == b.gate && a.accent == b.accent && a.slide == b.slide;
        for (int64_t phrase = 1; phrase < 8; ++phrase)
        {
            const auto c = acid::energiseStep (pattern.get (i), i, i, 1.0), d = acid::energiseStep (pattern.get (i), i, phrase * 64 + i, 1.0);
            newEachPhrase = newEachPhrase || c.gate != d.gate || c.accent != d.accent || c.slide != d.slide;
        }
    }
    check ("high energy repeats each phrase; peak energy mutates per phrase", sameInPhrase && newEachPhrase);

    // Generator: same seed, more energy = busier
    int lowNotes = 0, highNotes = 0, lowAcc = 0, highAcc = 0;
    for (uint32_t seed = 1; seed <= 40; ++seed)
    {
        acid::Pattern lo, hi;
        acid::AcidGenerator (seed).generate (lo, acid::Style::classic, acid::Scale::off, 0.0);
        acid::AcidGenerator (seed).generate (hi, acid::Style::classic, acid::Scale::off, 1.0);
        for (int i = 0; i < acid::kNumSteps; ++i)
        {
            lowNotes += lo.get (i).gate; highNotes += hi.get (i).gate;
            lowAcc += lo.get (i).gate && lo.get (i).accent; highAcc += hi.get (i).gate && hi.get (i).accent;
        }
    }
    std::printf ("  generator over 40 seeds: notes %d -> %d, accents %d -> %d\n", lowNotes, highNotes, lowAcc, highAcc);
    check ("GENERATE writes busier lines at high energy", highNotes > lowNotes && highAcc > lowAcc);

    // Sound: louder and brighter at high energy (RMS of the same 4 bars)
    auto rmsAt = [] (double e)
    {
        acid::AcidEngine engine;
        acid::EngineParams p;
        p.clock = acid::EngineParams::freeRun;
        p.energy = e;
        engine.setParams (p);
        engine.prepare (kSampleRate);
        std::vector<float> out ((size_t) (4 * 4 * 60.0 / 130.0 * kSampleRate));
        engine.beginBlock ({});
        engine.render (out.data(), (int) out.size());
        return measure (out);
    };
    const auto low = rmsAt (0.1), mid = rmsAt (0.5), high = rmsAt (0.95);
    std::printf ("  rms dB: low %.1f  neutral %.1f  high %.1f\n", toDb (low.rms), toDb (mid.rms), toDb (high.rms));
    check ("energy changes the sound and never clips", low.rms < mid.rms && high.rms > mid.rms && high.peak <= 1.0 && low.finite && high.finite);
    std::printf ("\n");
    return ok;
}

// 32 bars of the build-peak-break-drop arc with a factory ghost playing
std::vector<float> renderEnergyArc()
{
    acid::AcidEngine engine;
    acid::writeFactoryGhost (3, engine.ghost.take);   // Peak-Time Maniac
    engine.ghost.learn();
    acid::EngineParams p;
    p.clock = acid::EngineParams::freeRun;
    p.voice.acidMode = true;
    p.energyArc = 2;
    p.ghost.perform = true;
    p.ghost.amount = 0.7;
    p.fx.delayMix = 0.12;
    p.fx.reverbMix = 0.1;
    engine.setParams (p);
    engine.prepare (kSampleRate);
    std::vector<float> out ((size_t) (32 * 4 * 60.0 / 130.0 * kSampleRate));
    engine.beginBlock ({});
    engine.render (out.data(), (int) out.size());
    return out;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "renders";
   #ifdef _WIN32
    _mkdir (dir.c_str());
   #else
    mkdir (dir.c_str(), 0755);
   #endif

    checkLadder();

    const Scenario scenarios[] = {
        { "01_classic_default", 7.4, [] (acid::EngineParams&) {}, nullptr },
        { "02_classic_square", 7.4, [] (acid::EngineParams& p) { p.voice.waveform = 1; }, nullptr },
        { "03_classic_max_reso", 7.4,
          [] (acid::EngineParams& p) { p.voice.resonance = 1.0; p.voice.cutoff = 0.2; p.voice.envMod = 0.8; p.voice.decay = 0.5; },
          nullptr },
        { "04_classic_cutoff_sweep", 14.8,
          [] (acid::EngineParams& p) { p.voice.resonance = 0.85; },
          [] (acid::EngineParams& p, double t) { p.voice.cutoff = t < 0.5 ? 2.0 * t : 2.0 - 2.0 * t; } },
        { "05_classic_no_accent_vs_accent", 7.4,
          [] (acid::EngineParams& p) { p.voice.accent = 1.0; p.voice.resonance = 0.95; },
          nullptr },
        { "06_acid_default", 7.4, [] (acid::EngineParams& p) { p.voice.acidMode = true; }, nullptr },
        { "07_acid_screaming", 7.4,
          [] (acid::EngineParams& p)
          {
              auto& v = p.voice;
              v.acidMode = true; v.resonance = 1.0; v.cutoff = 0.3; v.envMod = 0.85; v.overdrive = 0.7;
              v.distortion = 0.7; v.sub = 0.5; v.slideMs = 120.0; v.filterFM = 0.15;
          },
          nullptr },
        { "08_classic_shuffle", 7.4, [] (acid::EngineParams& p) { p.shuffle = 0.6; }, nullptr },
        { "10_main_stage_fx", 14.8,
          [] (acid::EngineParams& p)
          {
              auto& v = p.voice;
              v.acidMode = true; v.resonance = 0.93; v.cutoff = 0.32; v.envMod = 0.78; v.overdrive = 0.55;
              v.distortion = 0.55; v.sub = 0.4;
              p.fx.delayMix = 0.25; p.fx.reverbMix = 0.2; p.fx.pump = 0.4; p.fx.lfoDepth = 0.4; p.fx.lfoBeats = 16.0;
          },
          nullptr },
    };

    std::printf ("%-32s %9s %9s\n", "render", "peak dB", "rms dB");
    bool ok = true;

    auto report = [&] (const std::string& name, const std::vector<float>& audio)
    {
        const auto s = measure (audio);
        ok = ok && s.finite && s.peak > 1.0e-4;
        std::printf ("%-32s %9.1f %9.1f%s\n", name.c_str(), toDb (s.peak), toDb (s.rms),
                     ! s.finite ? "  NON-FINITE!" : (s.peak > 1.0 ? "  (clips)" : ""));
        writeWav (dir + "/" + name + ".wav", audio);
    };

    for (const auto& sc : scenarios)
        report (sc.name, renderSequence (sc));
    report ("09_midi_legato", renderMidiLegato());

    // Cheat panel: one generated line per style, auto tweak on, 4 bars each
    const char* styleNames[] = { "classic", "hard", "hypnotic", "minimal", "dark", "rave" };
    for (int st = 0; st < 6; ++st)
    {
        acid::AcidEngine engine;
        acid::AcidGenerator (1234u + (uint32_t) st).generate (engine.patterns[0], (acid::Style) st, acid::Scale::off);

        std::printf ("  %-9s ", styleNames[st]);
        for (int i = 0; i < acid::kNumSteps; ++i)
        {
            const auto s = engine.patterns[0].get (i);
            if (! s.gate) std::printf ("  -   ");
            else std::printf ("%+3d%s%s ", s.note, s.accent ? "a" : " ", s.slide ? "s" : " ");
        }
        std::printf ("\n");

        acid::EngineParams p;
        p.clock = acid::EngineParams::freeRun;
        p.voice.acidMode = st == 1 || st == 5;
        p.squelch = 0.3;
        p.tweak = 0.6;
        p.tweakBeats = 4.0;
        engine.setParams (p);
        engine.prepare (kSampleRate);
        std::vector<float> out ((size_t) (7.4 * kSampleRate));
        engine.beginBlock ({});
        engine.render (out.data(), (int) out.size());
        report ("11_cheat_" + std::string (styleNames[st]), out);
    }

    bool ghostOk = false;
    report ("12_ghost_hands", renderGhost (ghostOk));
    ok = ok && ghostOk;
    bool learnOk = false;
    report ("13_ghost_learn", renderGhostLearn (learnOk));
    ok = ok && learnOk;

    ok = checkFactoryGhosts() && ok;
    ok = checkEnergy() && ok;
    report ("14_energy_arc_32_bars", renderEnergyArc());

    std::printf ("\nWAV files written to %s/\n", dir.c_str());
    return ok ? 0 : 1;
}
