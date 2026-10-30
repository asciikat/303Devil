// FOLLOW mode and ROLE checks, run by acid_snapshot. Returns 0 when everything passes.

#include "PluginProcessor.h"
#include "dsp/AcidEngine.h"
#include "dsp/Follow.h"

#include <cstdio>
#include <vector>

namespace
{
int bits (std::initializer_list<int> pcs)
{
    int m = 0;
    for (int pc : pcs)
        m |= 1 << pc;
    return m;
}

bool report (const char* what, bool ok)
{
    std::printf ("%s -> %s\n", what, ok ? "ok" : "FAILED");
    return ok;
}

// Holds a chord for some beats, then lets go of it
void play (acid::FollowHarmony& h, std::initializer_list<int> notes, double beats)
{
    for (int n : notes) h.noteOn (n);
    h.advance (beats);
    for (int n : notes) h.noteOff (n);
}

// The test track's 8-bar loop: Cm Cm G Cm | Ab Ab Fm7 Fm7
void playProgression (acid::FollowHarmony& h)
{
    play (h, { 60, 63, 67 }, 8.0);
    play (h, { 59, 62, 67 }, 4.0);
    play (h, { 60, 63, 67 }, 4.0);
    play (h, { 60, 63, 68 }, 8.0);
    play (h, { 60, 63, 65, 68 }, 8.0);
}
} // namespace

int runFollowTests()
{
    using acid::Role;
    using acid::Scale;
    bool ok = true;

    // Chord: held notes, latched for a bar after release, then gone. A note-on after a
    // release starts a new chord; notes added while others are held join it.
    {
        acid::FollowHarmony h;
        const bool empty = ! h.hasChord() && ! h.hasKey();
        h.noteOn (60); h.noteOn (63); h.noteOn (67);
        const bool cm = h.hasChord() && h.chordMask() == bits ({ 0, 3, 7 }) && h.chordRoot() == 0;
        h.noteOff (60); h.noteOff (63); h.noteOff (67);
        h.advance (3.0);
        const bool latched = h.hasChord() && h.chordMask() == bits ({ 0, 3, 7 });
        h.advance (1.5);
        const bool expired = ! h.hasChord();
        h.noteOn (55); h.noteOn (59);
        h.advance (0.5);
        h.noteOn (62);
        const bool g = h.hasChord() && h.chordMask() == bits ({ 7, 11, 2 }) && h.chordRoot() == 7;
        // Legato: the next chord comes in before the old one lets go
        acid::FollowHarmony l;
        l.noteOn (60); l.noteOn (63); l.noteOn (67);
        l.advance (1.0);
        l.noteOn (55); l.noteOn (59); l.noteOn (62);
        l.noteOff (60); l.noteOff (63); l.noteOff (67);
        const bool legato = l.chordMask() == bits ({ 7, 11, 2 }) && l.chordRoot() == 7;
        std::printf ("follow chord: empty %d cm %d latched %d expired %d g %d legato %d\n", empty, cm, latched, expired, g, legato);
        ok &= report ("follow chord: held, latched 1 bar, expires, new chord, legato", empty && cm && latched && expired && g && legato);
    }

    // Key: the track's progression is C minor
    {
        acid::FollowHarmony h;
        playProgression (h);
        std::printf ("follow key: %s (root %d minor %d)\n", h.keyName().c_str(), h.keyRoot(), h.keyMinor() ? 1 : 0);
        ok &= report ("follow key from Cm G Ab Fm7", h.hasKey() && h.keyRoot() == 0 && h.keyMinor());
    }

    // Key stays put on thin evidence: one bar of Cm then one of G is still C minor, not G major
    {
        acid::FollowHarmony h;
        play (h, { 60, 63, 67 }, 4.0);
        play (h, { 59, 62, 67 }, 4.0);
        const bool early = h.keyName() == "C minor";
        std::printf ("follow key after Cm G: %s\n", h.keyName().c_str());
        play (h, { 60, 63, 67 }, 4.0);
        play (h, { 60, 63, 68 }, 4.0);
        play (h, { 59, 62, 67 }, 4.0);
        const bool later = h.keyName() == "C minor";
        std::printf ("follow key early: %s\n", h.keyName().c_str());
        ok &= report ("follow key holds through the first bars", early && later);

        // ...but a track that really is in G major gets there
        acid::FollowHarmony g;
        for (int i = 0; i < 4; ++i)
        {
            play (g, { 55, 59, 62 }, 4.0);
            play (g, { 60, 64, 67 }, 4.0);
            play (g, { 62, 66, 69 }, 4.0);
            play (g, { 55, 59, 62 }, 4.0);
        }
        std::printf ("follow key G C D G: %s\n", g.keyName().c_str());
        ok &= report ("follow key moves when the music does", g.keyName() == "G major");
    }

    // Where a pattern note lands, per role, over each chord of the loop
    {
        acid::FollowHarmony h;
        playProgression (h);   // learns C minor
        auto under = [&h] (std::initializer_list<int> chord) { for (int n = 0; n < 128; ++n) h.noteOff (n); for (int n : chord) h.noteOn (n); };

        std::vector<int> got;
        under ({ 60, 63, 67 });   // Cm
        got.push_back (h.noteFor (0, 36, Role::bass, Scale::off));      // C2 36
        got.push_back (h.noteFor (2, 36, Role::bass, Scale::off));      // D -> Eb 39 (chord tones only)
        got.push_back (h.noteFor (0, 48, Role::overlay, Scale::off));   // C3 48
        under ({ 59, 62, 67 });   // G
        got.push_back (h.noteFor (0, 36, Role::bass, Scale::off));      // G1 31
        got.push_back (h.noteFor (3, 36, Role::bass, Scale::off));      // Bb -> B1 35
        got.push_back (h.noteFor (12, 36, Role::bass, Scale::off));     // G2 43
        got.push_back (h.noteFor (0, 48, Role::overlay, Scale::off));   // C3 -> B2 47 (stays in key, winds to the chord)
        got.push_back (h.noteFor (3, 48, Role::overlay, Scale::off));   // Eb3 -> D3 50
        got.push_back (h.noteFor (10, 36, Role::driver, Scale::off));   // G1+10 = F2 41 (G7 colour allowed)
        under ({ 60, 63, 68 });   // Ab
        got.push_back (h.noteFor (0, 36, Role::bass, Scale::off));      // Ab1 32
        under ({ 60, 63, 65, 68 });   // Fm7
        got.push_back (h.noteFor (0, 36, Role::bass, Scale::off));      // F2 41 (nearest F to C2)
        got.push_back (h.noteFor (7, 36, Role::driver, Scale::off));    // F2+7 = C3 48
        const std::vector<int> want { 36, 39, 48, 31, 35, 43, 47, 50, 41, 32, 41, 48 };
        std::printf ("follow notes:");
        for (int n : got) std::printf (" %d", n);
        std::printf ("\n");
        ok &= report ("follow note placement per role", got == want);

        // Silence for over a bar: back to the key's tonic chord
        under ({});
        h.advance (5.0);
        ok &= report ("follow falls back to the tonic", ! h.hasChord() && h.noteFor (0, 36, Role::bass, Scale::off) == 36
                                                         && h.noteFor (2, 36, Role::bass, Scale::off) == 39);

        // Nothing heard yet: exactly the internal sequencer's note
        acid::FollowHarmony fresh;
        ok &= report ("follow before any MIDI = internal note", fresh.noteFor (4, 36, Role::bass, Scale::minor) == 36 + acid::snapToScale (4, Scale::minor)
                                                                && fresh.noteFor (4, 36, Role::driver, Scale::off) == 40);
    }

    // Engine: the sequencer plays, the MIDI input steers it and is not played itself
    {
        acid::AcidEngine engine;
        engine.prepare (44100.0);
        acid::EngineParams p;
        p.source = acid::EngineParams::followMidi;
        p.role = (int) Role::bass;
        p.clock = acid::EngineParams::freeRun;
        p.bpm = 120.0;   // 5512.5 samples per step
        p.root = 36;
        p.length = 4;
        for (int i = 0; i < 4; ++i)
            engine.patterns[0].set (i, { 0, true, false, false });
        engine.setParams (p);
        std::vector<float> l (64), r (64);
        auto renderSteps = [&] (double steps) { for (int done = 0; done < (int) (steps * 5512.5); done += 64) engine.render (l.data(), r.data(), 64); };

        engine.midiNoteOn (55, 100); engine.midiNoteOn (59, 100); engine.midiNoteOn (62, 100);   // G
        renderSteps (0.5);
        const int underG = engine.getCurrentNote();
        engine.midiNoteOff (55); engine.midiNoteOff (59); engine.midiNoteOff (62);
        engine.midiNoteOn (56, 100); engine.midiNoteOn (60, 100); engine.midiNoteOn (63, 100);   // Ab
        renderSteps (1.0);
        const int underAb = engine.getCurrentNote();

        // Stopped sequencer: input notes make no sound
        p.run = false;
        engine.setParams (p);
        renderSteps (1.0);
        engine.midiNoteOn (60, 100);
        renderSteps (0.2);
        const bool silent = ! engine.getVoice().isGateOn();
        std::printf ("follow engine: under G %d, under Ab %d, input alone gate %s\n", underG, underAb, silent ? "off" : "ON");
        ok &= report ("follow engine plays the followed notes", underG == 31 && underAb == 32 && silent);
    }

    // GENERATE per role
    {
        int bassDownbeats = 0, bassGated = 0, bassAccentOffbeat = 0, bassAccents = 0, bassSlides = 0;
        int overlayGated = 0, overlayNoteSum = 0, overlayMin = 99, overlayMax = 0;
        bool driverSame = true;
        for (uint32_t seed = 1; seed <= 20; ++seed)
        {
            acid::Pattern bass, overlay, driver, plain;
            acid::AcidGenerator (seed).generate (bass, acid::Style::hypnotic, Scale::minor, 0.5, 0.0, Role::bass);
            acid::AcidGenerator (seed).generate (overlay, acid::Style::hypnotic, Scale::minor, 0.5, 0.0, Role::overlay);
            acid::AcidGenerator (seed).generate (driver, acid::Style::hypnotic, Scale::minor, 0.5, 0.0, Role::driver);
            acid::AcidGenerator (seed).generate (plain, acid::Style::hypnotic, Scale::minor, 0.5, 0.0);
            int gatedHere = 0;
            for (int i = 0; i < acid::kNumSteps; ++i)
            {
                const auto b = bass.get (i), o = overlay.get (i), d = driver.get (i), q = plain.get (i);
                bassDownbeats += (i % 4 == 0 && b.gate) ? 1 : 0;
                bassGated += b.gate ? 1 : 0;
                bassAccents += (b.gate && b.accent) ? 1 : 0;
                bassAccentOffbeat += (b.gate && b.accent && i % 4 == 2) ? 1 : 0;
                bassSlides += (b.gate && b.slide) ? 1 : 0;
                if (o.gate)
                {
                    ++gatedHere;
                    overlayNoteSum += o.note;
                }
                driverSame &= d.note == q.note && d.gate == q.gate && d.accent == q.accent && d.slide == q.slide;
            }
            overlayGated += gatedHere;
            overlayMin = std::min (overlayMin, gatedHere);
            overlayMax = std::max (overlayMax, gatedHere);
        }
        std::printf ("generate bass: %d gated, %d on downbeats, accents %d (%d on the off-beat 8th), slides %d\n",
                     bassGated, bassDownbeats, bassAccents, bassAccentOffbeat, bassSlides);
        std::printf ("generate overlay: %d-%d gated steps per pattern, mean note %.1f\n", overlayMin, overlayMax,
                     overlayGated > 0 ? (double) overlayNoteSum / overlayGated : 0.0);
        ok &= report ("generate bass rests on beats, rolls the 16ths",
                      bassDownbeats == 0 && bassGated >= 20 * 8 && bassAccents > 0 && bassAccentOffbeat * 2 >= bassAccents && bassSlides <= 20);
        ok &= report ("generate overlay is sparse and an octave up",
                      overlayMin >= 4 && overlayMax <= 11 && overlayGated > 0 && overlayNoteSum >= 9 * overlayGated);
        ok &= report ("generate driver unchanged", driverSame);
    }

    // The readout the editor shows, from the processor's ui values
    {
        const auto cm = bits ({ 0, 3, 7 }), ab = bits ({ 8, 0, 3 });
        const bool waiting = acid::follow::describe (-1, -1) == "waiting for MIDI";
        const bool keyOnly = acid::follow::describe (12, -1) == "C minor";
        const bool withChord = acid::follow::describe (12, ab | 8 << 12) == "C minor, chord Ab";
        const bool major = acid::follow::describe (7, bits ({ 7, 11, 2 }) | 7 << 12) == "G major, chord G";
        const bool tonic = acid::follow::describe (12, cm) == "C minor, chord Cm";
        std::printf ("follow readout: %s / %s\n", acid::follow::describe (12, ab | 8 << 12).c_str(), acid::follow::describe (-1, -1).c_str());
        ok &= report ("follow readout text", waiting && keyOnly && withChord && major && tonic);
    }

    // Plugin: role and Follow source are parameters and survive a save / reload
    {
        TB303AcidProcessor p;
        auto set = [&p] (const char* id, float v) { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set (pid::source, 2.0f);
        set (pid::role, 2.0f);
        juce::MemoryBlock saved;
        p.getStateInformation (saved);
        TB303AcidProcessor back;
        back.setStateInformation (saved.getData(), (int) saved.getSize());
        auto* source = dynamic_cast<juce::AudioParameterChoice*> (back.apvts.getParameter (pid::source));
        auto* role = dynamic_cast<juce::AudioParameterChoice*> (back.apvts.getParameter (pid::role));
        const bool kept = source != nullptr && role != nullptr && source->getIndex() == 2 && role->getIndex() == 2
                          && source->getCurrentChoiceName() == "Follow MIDI" && role->getCurrentChoiceName() == "Overlay";
        ok &= report ("follow source and role saved", kept);
    }

    return ok ? 0 : 1;
}
