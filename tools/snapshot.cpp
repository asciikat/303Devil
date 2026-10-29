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
        editor->setSize (900, 717);
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
        const bool ok = editor->getWidth() == 900 && editor->getHeight() == 717;
        std::printf ("scaled window %dx%d after reload: %s\n", editor->getWidth(), editor->getHeight(), ok ? "ok" : "FAILED");
        if (! ok)
            return 1;
    }
    return 0;
}
