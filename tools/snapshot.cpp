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
    // The artwork must remain attached to real host parameters. Exercise both
    // directions through the editor's actual controls, including at a scaled size.
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        juce::Slider* cutoff = nullptr;
        juce::TextButton* acidButton = nullptr;
        auto findControls = [&] (auto&& self, juce::Component& component) -> void
        {
            if (auto* slider = dynamic_cast<juce::Slider*> (&component))
                if (slider->getName() == pid::cutoff)
                    cutoff = slider;
            if (auto* button = dynamic_cast<juce::TextButton*> (&component))
                if (button->getButtonText().startsWith ("ACID MODE:"))
                    acidButton = button;
            for (auto* child : component.getChildren())
                self (self, *child);
        };
        findControls (findControls, *editor);
        if (cutoff == nullptr || acidButton == nullptr)
            return 1;

        // A host shows the editor before routing mouse events. Headless snapshot
        // editors start hidden, and JUCE deliberately returns no hit for them.
        editor->setVisible (true);
        bool hitAreas = true;
        for (int width : { 600, 900, 1200, 2400 })
        {
            editor->setSize (width, juce::roundToInt ((double) width * 956.0 / 1200.0));
            for (auto* control : { static_cast<juce::Component*> (cutoff),
                                   static_cast<juce::Component*> (acidButton) })
            {
                const auto point = editor->getLocalPoint (control, control->getLocalBounds().getCentre());
                auto* hit = editor->getComponentAt (point);
                const bool found = hit != nullptr && (hit == control || control->isParentOf (hit));
                if (! found)
                    std::printf ("hit diagnostic: width %d, control %s, point %d,%d, hit %s, visible %d\n",
                                 width, control->getName().toRawUTF8(), point.x, point.y,
                                 hit != nullptr ? hit->getName().toRawUTF8() : "null", (int) control->isVisible());
                hitAreas = hitAreas && found;
            }
        }
        std::printf ("inset control hit areas at 50/75/100/200 percent -> %s\n", hitAreas ? "ok" : "FAILED");
        if (! hitAreas)
            return 1;

        auto* parameter = processor.apvts.getParameter (pid::cutoff);
        const float requested = parameter->convertFrom0to1 (0.73f);
        cutoff->setValue (requested, juce::sendNotificationSync);
        const bool knobToHost = std::abs (parameter->getValue() - 0.73f) < 0.001f;
        parameter->setValueNotifyingHost (0.24f);
        const bool hostToKnob = std::abs ((float) cutoff->getValue() - parameter->convertFrom0to1 (0.24f)) < 0.001f;
        acidButton->setToggleState (false, juce::sendNotificationSync);
        const bool switchedOff = processor.apvts.getRawParameterValue (pid::acid)->load() < 0.5f;
        acidButton->setToggleState (true, juce::sendNotificationSync);
        const bool switchedOn = processor.apvts.getRawParameterValue (pid::acid)->load() > 0.5f;
        const bool ok = knobToHost && hostToKnob && switchedOff && switchedOn;
        std::printf ("editor controls: knob -> host %s, host -> knob %s, acid switch %s -> %s\n",
                     knobToHost ? "ok" : "FAILED", hostToKnob ? "ok" : "FAILED",
                     switchedOff && switchedOn ? "ok" : "FAILED", ok ? "ok" : "FAILED");
        if (! ok)
            return 1;
    }
    // Check the live Rave controls through the actual editor callbacks.
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setVisible (true);
        std::vector<juce::Component*> controls;
        juce::TextButton *service = nullptr, *throwKey = nullptr, *fillKey = nullptr,
                         *holdKey = nullptr, *returnKey = nullptr;
        auto collect = [&] (auto&& self, juce::Component& c) -> void
        {
            if (auto* button = dynamic_cast<juce::TextButton*> (&c))
            {
                if (c.getName() == "service") service = button;
                if (c.getName() == "throw") throwKey = button;
                if (c.getName() == "fill") fillKey = button;
                if (c.getName() == "hold") holdKey = button;
                if (c.getName() == "return") returnKey = button;
            }
            if (dynamic_cast<juce::Slider*> (&c) || dynamic_cast<juce::Button*> (&c)
                || dynamic_cast<juce::ComboBox*> (&c)) controls.push_back (&c);
            for (auto* child : c.getChildren()) self (self, *child);
        };
        collect (collect, *editor);
        if (! service || ! throwKey || ! fillKey || ! holdKey || ! returnKey) return 1;
        const float originalCut = processor.apvts.getParameter (pid::cutoff)->getValue();
        const int originalPattern = processor.getSelectedPattern();
        holdKey->onClick();
        processor.apvts.getParameter (pid::cutoff)->setValueNotifyingHost (0.92f);
        throwKey->setState (juce::Button::buttonDown);
        fillKey->setState (juce::Button::buttonDown);
        const bool held = processor.isThrowHeld() && processor.isFillHeld();
        returnKey->onClick();
        const bool restored = std::abs (processor.apvts.getParameter (pid::cutoff)->getValue() - originalCut) < 0.001f
                           && processor.getSelectedPattern() == originalPattern
                           && ! processor.isThrowHeld() && ! processor.isFillHeld();
        throwKey->setState (juce::Button::buttonNormal);
        fillKey->setState (juce::Button::buttonNormal);
        bool targets = true;
        int checked = 0;
        for (bool open : { false, true })
        {
            service->setToggleState (open, juce::dontSendNotification);
            service->onClick();
            for (int width : { 600, 900, 1200, 2400 })
            {
                editor->setSize (width, juce::roundToInt ((double) width * 956.0 / 1200.0));
                for (auto* c : controls)
                {
                    // Snapshot editors have no native peer; isShowing() is false
                    // even though their visible component tree can be hit-tested.
                    bool visible = true;
                    for (auto* ancestor = c; ancestor != nullptr; ancestor = ancestor->getParentComponent())
                        visible = visible && ancestor->isVisible();
                    if (! visible || ! c->isEnabled()) continue;
                    auto* hit = editor->getComponentAt (editor->getLocalPoint (c, c->getLocalBounds().getCentre()));
                    const bool good = hit && (hit == c || c->isParentOf (hit));
                    if (! good) std::printf ("target failed: service %d width %d name '%s'\n", (int) open, width, c->getName().toRawUTF8());
                    targets = targets && good;
                    ++checked;
                }
            }
        }
        editor->setSize (1200, 956);
        const auto file = dir.getChildFile ("gui_service.png");
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds()), stream);
        const bool ok = held && restored && targets && checked > 100;
        std::printf ("Rave buttons and Hold/Return: %s; %d main/service scaled hit targets: %s\n",
                     held && restored ? "ok" : "FAILED", checked, targets ? "ok" : "FAILED");
        if (! ok) return 1;
    }
    // Audible effects must differ while held, keep echo tails after release, and
    // leave every stored step and saved delay setting alone.
    {
        TB303AcidProcessor dry, thrown, filled;
        for (auto* p : { &dry, &thrown, &filled })
        {
            p->apvts.getParameter (pid::clock)->setValueNotifyingHost (1.0f);
            p->prepareToPlay (44100.0, 128);
        }
        std::array<acid::Step, acid::kNumSteps> original;
        for (int i = 0; i < acid::kNumSteps; ++i) original[(size_t) i] = filled.getPattern().get (i);
        juce::AudioBuffer<float> a (2, 128), b (2, 128), c (2, 128);
        juce::MidiBuffer midi;
        double echo = 0.0, fillDifference = 0.0;
        bool finite = true;
        for (int block = 0; block < 1100; ++block)
        {
            thrown.setThrowHeld (block < 100);
            filled.setFillHeld (block < 300);
            dry.processBlock (a, midi); thrown.processBlock (b, midi); filled.processBlock (c, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 128; ++i)
                {
                    const float x = a.getSample (ch, i), y = b.getSample (ch, i), z = c.getSample (ch, i);
                    finite = finite && std::isfinite (x) && std::isfinite (y) && std::isfinite (z);
                    if (block > 100) echo += std::abs (x - y);
                    if (block < 300) fillDifference += std::abs (x - z);
                }
        }
        bool unchanged = thrown.apvts.getRawParameterValue (pid::delayMix)->load() == 0.0f;
        for (int i = 0; i < acid::kNumSteps; ++i)
        {
            const auto x = original[(size_t) i], y = filled.getPattern().get (i);
            unchanged = unchanged && x.note == y.note && x.gate == y.gate && x.accent == y.accent && x.slide == y.slide;
        }
        const bool ok = echo > 1.0 && fillDifference > 1.0 && unchanged && finite
                     && thrown.getTailLengthSeconds() > 1.0;
        std::printf ("Rave audio: echo tail %.3f, fill difference %.3f, stored values %s, finite %s -> %s\n",
                     echo, fillDifference, unchanged ? "ok" : "FAILED", finite ? "ok" : "FAILED", ok ? "ok" : "FAILED");
        if (! ok) return 1;
    }
    return 0;
}
