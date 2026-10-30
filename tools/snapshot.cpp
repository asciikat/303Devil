// Renders the plugin editor to PNG (classic and acid mode) without opening a window.
//   ./acid_snapshot out_dir

#include "PluginEditor.h"
#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshots");
    dir.createDirectory();

    TB303AcidProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    auto setParam = [&processor] (const char* id, float value)
    {
        auto* param = processor.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    // Record a 2-bar Ghost Hands take in free run (cutoff opens each bar, resonance
    // rides up), then let the ghost play a little so the needles have somewhere to be.
    setParam (pid::clock, 1.0f);
    setParam (pid::ghostLength, 1.0f);   // 2 bars
    processor.toggleGhostRecord();
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    const double samplesPerBar = 4.0 * 60.0 / 130.0 * 44100.0;
    for (int pos = 0; pos < (int) (2.6 * samplesPerBar); pos += 512)
    {
        const double bars = pos / samplesPerBar;
        if (bars < 2.0)
        {
            setParam (pid::cutoff, (float) (0.15 + 0.6 * (bars - std::floor (bars))));
            setParam (pid::resonance, (float) (0.55 + 0.2 * bars));
            setParam (pid::envMod, (float) (0.5 + 0.3 * std::sin (bars * 3.14159)));
        }
        else
        {
            if (bars - 512 / samplesPerBar < 2.0)
                processor.updateGhostBrain();   // what the processor's timer does in a host
            setParam (pid::cutoff, 0.3f);
            // A hand grabs RESONANCE half way through, so the snapshot shows the hand-off
            setParam (pid::resonance, bars < 2.3 ? 0.6f : (float) (0.6 + 0.3 * (bars - 2.3)));
            setParam (pid::ghostPerform, 1.0f);
        }
        processor.processBlock (buffer, midi);
    }

    // Run on until the hand-held beats are captured, as the timer would
    for (int i = 0; i < 40; ++i)
        processor.processBlock (buffer, midi);
    processor.updateGhostBrain();

    // The take and the beats learned by hand must survive a save and reload
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    TB303AcidProcessor reloaded;
    reloaded.setStateInformation (state.getData(), (int) state.getSize());
    const auto& a = processor.getGhost().take;
    const auto& b = reloaded.getGhost().take;
    float worst = 0.0f;
    for (int t = 0; t < a.getBars() * acid::ghost::kTicksPerBar; ++t)
        for (int lane = 0; lane < acid::ghost::kNumLanes; ++lane)
            worst = std::max (worst, std::abs (a.get (t, lane) - b.get (t, lane)));
    std::printf ("ghost take: %d bars, reloaded %d bars, max save/load error %.6f -> %s\n", a.getBars(), b.getBars(),
                 worst, a.getBars() == 2 && b.getBars() == 2 && worst < 1.0e-4f ? "ok" : "FAILED");
    const int learned = processor.getGhost().getLearnedCount(), reloadedLearned = reloaded.getGhost().getLearnedCount();
    std::printf ("ghost beats learned by hand: %d, reloaded %d -> %s\n", learned, reloadedLearned,
                 learned > 0 && learned == reloadedLearned ? "ok" : "FAILED");

    // .ghost files: save, load into a fresh plugin, compare
    {
        const auto file = dir.getChildFile ("roundtrip_test.ghost");
        const bool saved = processor.saveGhostFile (file);
        TB303AcidProcessor other;
        const bool loaded = other.loadGhostFile (file);
        float fileWorst = 0.0f;
        const auto& x = processor.getGhost().take;
        const auto& y = other.getGhost().take;
        for (int t = 0; t < x.getBars() * acid::ghost::kTicksPerBar; ++t)
            for (int lane = 0; lane < acid::ghost::kNumLanes; ++lane)
                fileWorst = std::max (fileWorst, std::abs (x.get (t, lane) - y.get (t, lane)));
        const bool good = saved && loaded && y.getBars() == x.getBars() && fileWorst < 1.0e-4f
                          && other.getGhostName() == "roundtrip_test"
                          && other.getGhost().getLearnedCount() == processor.getGhost().getLearnedCount()
                          && other.apvts.getRawParameterValue (pid::ghostPerform)->load() > 0.5f;
        std::printf (".ghost file: saved %s, loaded %s, max error %.6f, name '%s' -> %s\n", saved ? "yes" : "no",
                     loaded ? "yes" : "no", fileWorst, other.getGhostName().toRawUTF8(), good ? "ok" : "FAILED");
        file.deleteFile();

        TB303AcidProcessor empty;
        std::printf ("saving with no take is refused -> %s\n", empty.saveGhostFile (file) ? "FAILED" : "ok");

        TB303AcidProcessor factory;
        factory.loadFactoryGhost (2);
        std::printf ("factory ghost via plugin: '%s', %d bars, %d move types -> %s\n", factory.getGhostName().toRawUTF8(),
                     factory.getGhost().take.getBars(), factory.getGhost().getMoveTypes(),
                     factory.getGhostName() == "Breakdown Closer" && factory.getGhost().take.getBars() == 4
                         && factory.getGhost().getMoveTypes() >= 2 ? "ok" : "FAILED");
    }

    // MIDI export: factory pattern 1 ("0a 12s 3 - -2a 5s 7 - 0a 10s 3 - 12a 7s 3 -") at the default
    // root, no swing, no scale lock, read back from the file the drag button would hand to a host
    {
        TB303AcidProcessor exporter;
        auto setExporterParam = [&exporter] (const char* id, float value)
        {
            auto* param = exporter.apvts.getParameter (id);
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        };
        setExporterParam (pid::shuffle, 0.0f);
        setExporterParam (pid::scale, 0.0f);
        exporter.selectPattern (0);

        const int root = (int) exporter.apvts.getRawParameterValue (pid::root)->load();
        const auto file = exporter.exportPatternMidi (4);
        juce::FileInputStream in (file);
        juce::MidiFile midi;
        const bool read = in.openedOk() && midi.readFrom (in);

        struct Held { int pitch, start, end, velocity; };
        std::vector<Held> notes;
        int lastTick = 0;
        if (read && midi.getNumTracks() == 1)
        {
            const auto* track = midi.getTrack (0);
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto* e = track->getEventPointer (i);
                lastTick = (int) e->message.getTimeStamp();
                if (e->message.isNoteOn() && e->noteOffObject != nullptr)
                    notes.push_back ({ e->message.getNoteNumber(), (int) e->message.getTimeStamp(),
                                       (int) e->noteOffObject->message.getTimeStamp(), e->message.getVelocity() });
            }
        }

        // 12 gated steps per bar. The slide on step 2 (12s) runs into a different pitch, so it
        // overlaps the next note by 24 ticks; the plain steps close after half a step.
        const bool ok = read && midi.getTimeFormat() == 960 && notes.size() == 48 && lastTick == 4 * 3840
                        && notes[0].pitch == root && notes[0].start == 0 && notes[0].end == 120 && notes[0].velocity == 127
                        && notes[1].pitch == root + 12 && notes[1].start == 240 && notes[1].end == 504 && notes[1].velocity < 100
                        && notes[2].pitch == root + 3 && notes[2].start == 480
                        && notes[12].pitch == root && notes[12].start == 3840;
        std::printf ("midi export: %zu notes over %d ticks -> %s\n", notes.size(), lastTick, ok ? "ok" : "FAILED");
        if (! ok)
            return 1;
    }

    // Undo / redo and the rhythm / pitch locks
    {
        TB303AcidProcessor p;
        p.selectPattern (0);
        using Steps = std::array<acid::Step, acid::kNumSteps>;
        auto read = [&p] { Steps s; for (int i = 0; i < acid::kNumSteps; ++i) s[(size_t) i] = p.getPattern().get (i); return s; };
        auto same = [] (const Steps& a, const Steps& b)
        {
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i].note != b[i].note || a[i].gate != b[i].gate || a[i].accent != b[i].accent || a[i].slide != b[i].slide)
                    return false;
            return true;
        };

        const Steps original = read();
        p.generatePattern();
        const Steps generated = read();
        bool ok = ! same (original, generated) && p.canUndo() && ! p.canRedo();
        p.undoPattern();
        ok = ok && same (read(), original) && ! p.canUndo() && p.canRedo();
        p.redoPattern();
        ok = ok && same (read(), generated) && p.canUndo() && ! p.canRedo();

        // A grid edit (begin ... edited) is one undo step, and a new edit clears redo
        p.undoPattern();
        p.beginPatternEdit();
        auto step = p.getPattern().get (3);
        step.note += 5;
        p.getPattern().set (3, step);
        p.patternEdited();
        ok = ok && p.canUndo() && ! p.canRedo();
        p.beginPatternEdit();   // an edit that changes nothing must not add a step
        p.patternEdited();
        p.undoPattern();
        ok = ok && same (read(), original) && ! p.canUndo();
        std::printf ("undo / redo -> %s\n", ok ? "ok" : "FAILED");
        if (! ok)
            return 1;

        // Locks: 40 generate / random / mutate rounds each, from a fresh pattern
        auto lockRun = [&] (bool rhythm, bool pitch)
        {
            TB303AcidProcessor q;
            q.selectPattern (0);
            q.setLockRhythm (rhythm);
            q.setLockPitch (pitch);
            q.apvts.getParameter (pid::roll)->setValueNotifyingHost (1.0f);   // rolls must be held too
            Steps base; for (int i = 0; i < acid::kNumSteps; ++i) base[(size_t) i] = q.getPattern().get (i);
            bool kept = true, changedFree = false;
            for (int round = 0; round < 40; ++round)
            {
                if (round % 3 == 0)      q.generatePattern();
                else if (round % 3 == 1) q.randomizePattern();
                else                     q.mutatePattern();
                for (int i = 0; i < acid::kNumSteps; ++i)
                {
                    const auto s = q.getPattern().get (i), b = base[(size_t) i];
                    if (rhythm && (s.gate != b.gate || s.accent != b.accent || s.slide != b.slide || s.ratchet != b.ratchet)) kept = false;
                    if (pitch && s.note != b.note) kept = false;
                    if ((! rhythm && (s.gate != b.gate || s.accent != b.accent || s.slide != b.slide || s.ratchet != b.ratchet))
                        || (! pitch && s.note != b.note))
                        changedFree = true;
                }
            }
            return kept && (changedFree || (rhythm && pitch));
        };
        const bool rhythmOk = lockRun (true, false), pitchOk = lockRun (false, true), bothOk = lockRun (true, true);
        std::printf ("lock rhythm -> %s, lock pitch -> %s, both -> %s\n", rhythmOk ? "ok" : "FAILED",
                     pitchOk ? "ok" : "FAILED", bothOk ? "ok" : "FAILED");

        // The locks are saved with the session
        p.setLockRhythm (true);
        juce::MemoryBlock saved;
        p.getStateInformation (saved);
        TB303AcidProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        const bool persisted = restored.getLockRhythm() && ! restored.getLockPitch();
        std::printf ("locks saved with session -> %s\n", persisted ? "ok" : "FAILED");
        if (! (rhythmOk && pitchOk && bothOk && persisted))
            return 1;
    }

    // Rolls (ratchets): a step with x4 fires four hits, and the text format, saving and MIDI export keep them
    {
        acid::Pattern text;
        text.loadFromString ("7r4 0a - 3r2s 5as");
        const auto s0 = text.get (0), s3 = text.get (3);
        const bool parsed = s0.note == 7 && s0.ratchet == 3 && s0.hits() == 4
                            && s3.ratchet == 1 && s3.slide && s3.hits() == 1 && text.get (1).accent && ! text.get (2).gate;

        // Engine: 120 bpm free run = 5512 samples per step. Step 0 rolls x4, step 1 is plain, step 2 rolls but slides.
        auto countHits = [] (acid::Step a, acid::Step b, acid::Step c)
        {
            acid::AcidEngine engine;
            engine.prepare (44100.0);
            acid::EngineParams p;
            p.clock = acid::EngineParams::freeRun;
            p.bpm = 120.0;
            p.length = 3;
            engine.patterns[0].set (0, a);
            engine.patterns[0].set (1, b);
            engine.patterns[0].set (2, c);
            engine.setParams (p);
            std::vector<float> out (64), outR (64);
            int edges = 0;
            bool was = false;
            for (int done = 0; done < 3 * 5512 - 200; done += 64)
            {
                engine.render (out.data(), outR.data(), 64);
                const bool on = engine.getVoice().isGateOn();
                edges += on && ! was ? 1 : 0;
                was = on;
            }
            return edges;
        };
        const int rolled = countHits ({ 0, true, false, false, 3 }, { 0, true, false, false, 0 }, { 0, false, false, false, 0 });
        const int slid = countHits ({ 0, true, false, true, 3 }, { 0, true, false, false, 0 }, { 0, false, false, false, 0 });
        const int rest = countHits ({ 0, false, false, false, 3 }, { 0, true, false, false, 0 }, { 0, false, false, false, 0 });

        // Saved with the session, and exported as that many short notes
        TB303AcidProcessor r;
        r.selectPattern (0);
        r.apvts.getParameter (pid::shuffle)->setValueNotifyingHost (0.0f);
        r.apvts.getParameter (pid::scale)->setValueNotifyingHost (0.0f);
        for (int i = 0; i < acid::kNumSteps; ++i)
            r.getPattern().set (i, { 0, i == 0 || i == 4, false, false, i == 0 ? 3 : (i == 4 ? 1 : 0) });
        juce::MemoryBlock saved;
        r.getStateInformation (saved);
        TB303AcidProcessor back;
        back.setStateInformation (saved.getData(), (int) saved.getSize());
        const bool persisted = back.getPattern().get (0).ratchet == 3 && back.getPattern().get (4).ratchet == 1;

        juce::FileInputStream in (r.exportPatternMidi (1));
        juce::MidiFile midi;
        std::vector<int> starts, ends;
        if (in.openedOk() && midi.readFrom (in) && midi.getNumTracks() == 1)
            for (int i = 0; i < midi.getTrack (0)->getNumEvents(); ++i)
            {
                const auto* e = midi.getTrack (0)->getEventPointer (i);
                if (e->message.isNoteOn() && e->noteOffObject != nullptr)
                {
                    starts.push_back ((int) e->message.getTimeStamp());
                    ends.push_back ((int) e->noteOffObject->message.getTimeStamp());
                }
            }
        const bool exported = starts == std::vector<int> { 0, 60, 120, 180, 960, 1080 }
                              && ends == std::vector<int> { 30, 90, 150, 210, 1020, 1140 };

        std::printf ("roll: parse %s, engine hits x4=%d slide=%d rest=%d, saved %s, midi %zu notes %s\n",
                     parsed ? "ok" : "FAILED", rolled, slid, rest, persisted ? "ok" : "FAILED", starts.size(),
                     exported ? "ok" : "FAILED");
        // x4 + 1 plain = 5. A slide can't roll, and it holds the gate into step 1 (a glide, no retrigger), so 1.
        // A rest can't roll, so only step 1's hit is left: 1.
        if (! (parsed && rolled == 5 && slid == 1 && rest == 1 && persisted && exported))
            return 1;
    }

    // Show an energy arc on the meter
    setParam (pid::energyArc, 2.0f);
    for (int i = 0; i < 4; ++i)
        processor.processBlock (buffer, midi);

    for (bool acid : { false, true })
    {
        processor.apvts.getParameter (pid::acid)->setValueNotifyingHost (acid ? 1.0f : 0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto file = dir.getChildFile (acid ? "gui_acid.png" : "gui_classic.png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
    }

    // Resizable window: scaled down to 75%, the panel must still fill it, and the size
    // must come back with the session
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (900, 747);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto file = dir.getChildFile ("gui_scaled_75.png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        editor.reset();
        processor.setStateInformation (saved.getData(), (int) saved.getSize());
        editor.reset (processor.createEditor());
        const bool ok = editor->getWidth() == 900 && editor->getHeight() == 747;
        std::printf ("scaled window %dx%d after reload: %s\n", editor->getWidth(), editor->getHeight(), ok ? "ok" : "FAILED");
        if (! ok)
            return 1;
    }
    return 0;
}
