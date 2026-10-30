#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/AcidGenerator.h"
#include "dsp/Pattern.h"

#include <algorithm>
#include <vector>

namespace acid
{
struct MidiExportSettings
{
    int    root = 36;            // MIDI note of pattern offset 0
    int    length = kNumSteps;   // steps before the pattern loops
    Scale  scale = Scale::off;
    double bpm = 130.0;
    double shuffle = 0.0;        // 0..1 -> swing 50%..75%, same as the engine
    int    accentVelocity = 100; // velocity the plugin treats as an accent on MIDI input
    int    bars = 4;             // 16 steps per bar
};

// Writes the pattern the way the engine plays it: 16th-note steps, shuffle on every second
// step, a gate that is open for half a step, and slides that hold the note into the next one.
// A slide into a different pitch overlaps the next note (legato, so a synth glides); a slide
// into the same pitch is merged into one long note. Accents get velocity 127.
inline juce::MidiFile buildPatternMidi (const Pattern& pattern, const MidiExportSettings& s)
{
    constexpr int ppq = 960;
    constexpr int stepsPerBar = 16;
    constexpr int overlapTicks = 24;
    constexpr double gateLength = 0.5;   // AcidEngine::kGateLength

    const int len = std::clamp (s.length, 1, kNumSteps);
    const int totalSteps = std::max (1, s.bars) * stepsPerBar;
    const double swing = 0.5 + 0.25 * std::clamp (s.shuffle, 0.0, 1.0);
    const int normalVelocity = std::clamp (s.accentVelocity - 1, 1, 90);

    struct Note { int pitch, start, end, velocity; };
    std::vector<Note> notes;
    bool prevSlid = false;   // the previous step held its gate into this one

    for (int i = 0; i < totalSteps; ++i)
    {
        const Step step = pattern.get (i % len);
        const bool odd = (i & 1) != 0;
        const int eighthStart = (i / 2) * (ppq / 2);
        const int start = eighthStart + (odd ? (int) std::lround (swing * (ppq / 2)) : 0);
        const int stepTicks = odd ? (ppq / 2) - (int) std::lround (swing * (ppq / 2))
                                  : (int) std::lround (swing * (ppq / 2));
        const int end = start + (step.slide ? stepTicks : std::max (1, (int) std::lround (stepTicks * gateLength)));

        if (! step.gate)
        {
            prevSlid = false;
            continue;
        }

        const int pitch = std::clamp (s.root + snapToScale (step.note, s.scale), 0, 127);

        if (prevSlid && ! notes.empty())
        {
            Note& prev = notes.back();
            if (prev.pitch == pitch)
            {
                prev.end = end;   // same pitch: one held note
                prevSlid = step.slide;
                continue;
            }
            prev.end += overlapTicks;   // different pitch: overlap so the synth glides
        }

        notes.push_back ({ pitch, start, end, step.accent ? 127 : normalVelocity });
        prevSlid = step.slide;
    }

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));
    track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60.0e6 / std::max (20.0, s.bpm))));

    for (const auto& n : notes)
    {
        track.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) n.velocity), n.start);
        track.addEvent (juce::MidiMessage::noteOff (1, n.pitch), n.end);
    }

    const int clipEnd = totalSteps / 2 * (ppq / 2);
    track.addEvent (juce::MidiMessage::endOfTrack(), clipEnd);
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (track);
    return file;
}
} // namespace acid
