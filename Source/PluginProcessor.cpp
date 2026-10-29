#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "dsp/FactoryGhosts.h"

#include <cstring>
#include <iterator>

namespace
{
juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
float percentValue (const juce::String& s) { return s.getFloatValue() / 100.0f; }

juce::String msText (float v, int)
{
    return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
}

const char* const kDelayTimeNames[] = { "1/16", "1/8", "3/16", "1/4", "3/8" };
constexpr double kDelayTimeBeats[]  = { 0.25, 0.5, 0.75, 1.0, 1.5 };
const char* const kLfoRateNames[]   = { "1 beat", "2 beats", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" };
constexpr double kLfoRateBeats[]    = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
const char* const kTweakRateNames[] = { "1 bar", "2 bars", "4 bars", "8 bars" };
constexpr double kTweakRateBeats[]  = { 4.0, 8.0, 16.0, 32.0 };

juce::NormalisableRange<float> skewedRange (float min, float max, float centre)
{
    juce::NormalisableRange<float> r (min, max);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

TB303AcidProcessor::TB303AcidProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    raw.tune        = get (pid::tune);
    raw.cutoff      = get (pid::cutoff);
    raw.resonance   = get (pid::resonance);
    raw.envMod      = get (pid::envMod);
    raw.decay       = get (pid::decay);
    raw.accent      = get (pid::accent);
    raw.wave        = get (pid::wave);
    raw.volume      = get (pid::volume);
    raw.acid        = get (pid::acid);
    raw.overdrive   = get (pid::overdrive);
    raw.filterFM    = get (pid::filterFM);
    raw.tracking    = get (pid::tracking);
    raw.sweep       = get (pid::sweep);
    raw.slideTime   = get (pid::slideTime);
    raw.ampDecay    = get (pid::ampDecay);
    raw.accentDecay = get (pid::accentDecay);
    raw.softAttack  = get (pid::softAttack);
    raw.muffler     = get (pid::muffler);
    raw.distortion  = get (pid::distortion);
    raw.sub         = get (pid::sub);
    raw.delayMix      = get (pid::delayMix);
    raw.delayTime     = get (pid::delayTime);
    raw.delayFeedback = get (pid::delayFeedback);
    raw.reverbMix     = get (pid::reverbMix);
    raw.reverbSize    = get (pid::reverbSize);
    raw.pump          = get (pid::pump);
    raw.lfoDepth      = get (pid::lfoDepth);
    raw.lfoRate       = get (pid::lfoRate);
    raw.squelch       = get (pid::squelch);
    raw.tweak         = get (pid::tweak);
    raw.tweakRate     = get (pid::tweakRate);
    raw.style         = get (pid::style);
    raw.scale         = get (pid::scale);
    raw.ghostPerform   = get (pid::ghostPerform);
    raw.ghostAmount    = get (pid::ghostAmount);
    raw.ghostVariation = get (pid::ghostVariation);
    raw.ghostLength    = get (pid::ghostLength);
    raw.ghostMode      = get (pid::ghostMode);
    raw.energy         = get (pid::energy);
    raw.energyArc      = get (pid::energyArc);
    raw.source      = get (pid::source);
    raw.clock       = get (pid::clock);
    raw.run         = get (pid::run);
    raw.bpm         = get (pid::bpm);
    raw.root        = get (pid::root);
    raw.shuffle     = get (pid::shuffle);
    raw.length      = get (pid::length);
    raw.pattern     = get (pid::pattern);
    raw.accentVel   = get (pid::accentVel);

    startTimerHz (5);   // Ghost Hands learns on the message thread
}

juce::AudioProcessorValueTreeState::ParameterLayout TB303AcidProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addPercent = [&layout] (const char* id, const char* name, float defaultValue)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), defaultValue,
            AudioParameterFloatAttributes().withStringFromValueFunction (percentText)
                                           .withValueFromStringFunction (percentValue)));
    };

    auto addMs = [&layout] (const char* id, const char* name, NormalisableRange<float> range, float defaultValue)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, defaultValue,
            AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms")));
    };

    // Classic panel
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::tune, 1 }, "Tuning", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 2) + " st"; })
            .withLabel ("st")));
    addPercent (pid::cutoff,    "Cutoff",    0.35f);
    addPercent (pid::resonance, "Resonance", 0.70f);
    addPercent (pid::envMod,    "Env Mod",   0.55f);
    addPercent (pid::decay,     "Decay",     0.35f);
    addPercent (pid::accent,    "Accent",    0.70f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::wave, 1 }, "Waveform",
                                                        StringArray { "Saw", "Square" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::volume, 1 }, "Volume", NormalisableRange<float> (-36.0f, 6.0f), -6.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
            .withLabel ("dB")));

    // Acid mode
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::acid, 1 }, "Acid Mode", false));
    addPercent (pid::overdrive, "Overdrive",      0.30f);
    addPercent (pid::filterFM,  "Filter FM",      0.0f);
    addPercent (pid::tracking,  "Filter Tracking", 0.0f);
    addPercent (pid::sweep,     "Accent Sweep Speed", 0.5f);
    addMs (pid::slideTime,   "Slide Time",   skewedRange (10.0f, 400.0f, 80.0f),  60.0f);
    addMs (pid::ampDecay,    "Amp Decay",    skewedRange (16.0f, 3000.0f, 400.0f), 3000.0f);
    addMs (pid::accentDecay, "Accent Decay", skewedRange (30.0f, 1000.0f, 200.0f), 200.0f);
    addMs (pid::softAttack,  "Soft Attack",  skewedRange (0.3f, 30.0f, 3.0f),      3.0f);
    addPercent (pid::muffler,    "Muffler",    0.30f);
    addPercent (pid::distortion, "Distortion", 0.35f);
    addPercent (pid::sub,        "Sub",        0.30f);

    // Main-stage FX
    addPercent (pid::delayMix, "Delay Mix", 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::delayTime, 1 }, "Delay Time",
                                                        StringArray (kDelayTimeNames), 2));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::delayFeedback, 1 }, "Delay Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.45f,
        AudioParameterFloatAttributes().withStringFromValueFunction (percentText).withValueFromStringFunction (percentValue)));
    addPercent (pid::reverbMix,  "Reverb Mix",  0.0f);
    addPercent (pid::reverbSize, "Reverb Size", 0.6f);
    addPercent (pid::pump,       "Pump",        0.0f);
    addPercent (pid::lfoDepth,   "Filter LFO Depth", 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfoRate, 1 }, "Filter LFO Rate",
                                                        StringArray (kLfoRateNames), 4));

    // Cheat panel
    addPercent (pid::squelch, "Squelch", 0.0f);
    addPercent (pid::tweak,   "Auto Tweak", 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::tweakRate, 1 }, "Auto Tweak Rate",
                                                        StringArray (kTweakRateNames), 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::style, 1 }, "Generator Style",
                                                        StringArray { "Classic", "Hard", "Hypnotic", "Minimal", "Dark", "Rave" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::scale, 1 }, "Scale Lock",
                                                        StringArray { "Off", "Minor", "Phrygian", "Minor Pentatonic", "Dorian", "Harmonic Minor" }, 0));

    // Ghost Hands
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::ghostPerform, 1 }, "Ghost Perform", false));
    addPercent (pid::ghostAmount,    "Ghost Amount",    1.0f);
    addPercent (pid::ghostVariation, "Ghost Variation", 0.3f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::ghostLength, 1 }, "Ghost Take Length",
                                                        StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::ghostMode, 1 }, "Ghost Mode",
                                                        StringArray { "Loop", "Learn" }, 1));

    // Energy
    addPercent (pid::energy, "Energy", 0.5f);
    {
        StringArray arcs;
        for (const auto& arc : acid::kEnergyArcs)
            arcs.add (arc.name);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::energyArc, 1 }, "Energy Arc", arcs, 0));
    }

    // Sequencer
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::source, 1 }, "Note Source",
                                                        StringArray { "Internal Sequencer", "MIDI Input" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::clock, 1 }, "Clock",
                                                        StringArray { "Host Sync", "Free Run" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::run, 1 }, "Run", true));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::bpm, 1 }, "Free Run BPM", NormalisableRange<float> (40.0f, 250.0f, 0.1f), 130.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { pid::root, 1 }, "Root Note", 24, 72, 36,
        AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 4); })));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::shuffle, 1 }, "Shuffle", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (juce::roundToInt (50.0f + 25.0f * v)) + "%"; })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid::length, 1 }, "Pattern Length", 1, acid::kNumSteps, acid::kNumSteps));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid::pattern, 1 }, "Pattern", 1, acid::kNumPatterns, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid::accentVel, 1 }, "Accent Velocity", 1, 127, 100));

    return layout;
}

acid::EngineParams TB303AcidProcessor::readParams() const noexcept
{
    acid::EngineParams e;
    auto& v = e.voice;

    v.tuneSemis     = raw.tune->load();
    v.waveform      = (int) raw.wave->load();
    v.cutoff        = raw.cutoff->load();
    v.resonance     = raw.resonance->load();
    v.envMod        = raw.envMod->load();
    v.decay         = raw.decay->load();
    v.accent        = raw.accent->load();
    v.volumeDb      = raw.volume->load();
    v.acidMode      = raw.acid->load() > 0.5f;
    v.overdrive     = raw.overdrive->load();
    v.filterFM      = raw.filterFM->load();
    v.tracking      = raw.tracking->load();
    v.sweepSpeed    = raw.sweep->load();
    v.slideMs       = raw.slideTime->load();
    v.ampDecayMs    = raw.ampDecay->load();
    v.accentDecayMs = raw.accentDecay->load();
    v.softAttackMs  = raw.softAttack->load();
    v.muffler       = raw.muffler->load();
    v.distortion    = raw.distortion->load();
    v.sub           = raw.sub->load();

    auto& fx = e.fx;
    fx.delayMix      = raw.delayMix->load();
    fx.delayBeats    = kDelayTimeBeats[juce::jlimit (0, (int) std::size (kDelayTimeBeats) - 1, (int) raw.delayTime->load())];
    fx.delayFeedback = raw.delayFeedback->load();
    fx.reverbMix     = raw.reverbMix->load();
    fx.reverbSize    = raw.reverbSize->load();
    fx.pump          = raw.pump->load();
    fx.lfoDepth      = raw.lfoDepth->load();
    fx.lfoBeats      = kLfoRateBeats[juce::jlimit (0, (int) std::size (kLfoRateBeats) - 1, (int) raw.lfoRate->load())];

    e.source         = (int) raw.source->load();
    e.clock          = (int) raw.clock->load();
    e.run            = raw.run->load() > 0.5f;
    e.bpm            = raw.bpm->load();
    e.root           = (int) raw.root->load();
    e.shuffle        = raw.shuffle->load();
    e.length         = (int) raw.length->load();
    e.pattern        = getSelectedPattern();
    e.accentVelocity = (int) raw.accentVel->load();
    e.squelch        = raw.squelch->load();
    e.tweak          = raw.tweak->load();
    e.tweakBeats     = kTweakRateBeats[juce::jlimit (0, (int) std::size (kTweakRateBeats) - 1, (int) raw.tweakRate->load())];
    e.scale          = (int) raw.scale->load();

    e.ghost.perform   = raw.ghostPerform->load() > 0.5f;
    e.ghost.mode      = (int) raw.ghostMode->load();
    e.energy          = raw.energy->load();
    e.energyArc       = (int) raw.energyArc->load();
    e.ghost.amount    = raw.ghostAmount->load();
    e.ghost.variation = raw.ghostVariation->load();
    e.ghost.bars      = acid::ghost::kLengthChoices[juce::jlimit (0, acid::ghost::kNumLengthChoices - 1, (int) raw.ghostLength->load())];
    return e;
}

void TB303AcidProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    tempRight.assign ((size_t) std::max (samplesPerBlock, 8192), 0.0f);
    engine.setParams (readParams());
    engine.prepare (sampleRate);
}

bool TB303AcidProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void TB303AcidProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    engine.setParams (readParams());

    acid::Transport transport;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            transport.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())
                transport.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
            {
                transport.ppq = *ppq;
                transport.hasPpq = true;
            }
        }
    }
    engine.beginBlock (transport);

    // Render up to each MIDI event so notes land on the exact sample
    float* out = buffer.getWritePointer (0);
    if ((int) tempRight.size() < numSamples)
        tempRight.resize ((size_t) numSamples);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : tempRight.data();
    int done = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPos > done)
        {
            engine.render (out + done, outR + done, eventPos - done);
            done = eventPos;
        }

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine.midiNoteOn (msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            engine.midiNoteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.midiAllNotesOff();
    }
    if (done < numSamples)
        engine.render (out + done, outR + done, numSamples - done);

    midiMessages.clear();

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, ch % 2, 0, numSamples);

    scope.push (out, numSamples);
    uiStep.store (engine.getCurrentStep());
    uiPlayingPattern.store (engine.getPlayingPattern());
    uiGate.store (engine.getVoice().isGateOn());
    uiSlide.store (engine.getVoice().isSliding());
    uiAccentSweep.store ((float) engine.getVoice().getAccentSweep());
    uiEnergy.store ((float) engine.getEnergy());
    uiArcPosition.store ((float) engine.getArcPosition());
}

//==============================================================================
void TB303AcidProcessor::patternEdited()
{
    ++patternVersion;
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

void TB303AcidProcessor::randomizePattern()
{
    // Minor-ish acid scale, weighted towards the root
    static constexpr int scale[] = { 0, 0, 0, 0, 3, 5, 7, 7, 10, 12, 12, 15, -2, -5 };
    constexpr int scaleSize = (int) std::size (scale);

    auto& pattern = getPattern();
    for (int i = 0; i < acid::kNumSteps; ++i)
    {
        acid::Step s;
        s.gate   = random.nextFloat() < 0.78f;
        s.note   = scale[random.nextInt (scaleSize)];
        s.accent = s.gate && random.nextFloat() < 0.3f;
        s.slide  = s.gate && random.nextFloat() < 0.28f;
        pattern.set (i, s);
    }
    patternEdited();
}

void TB303AcidProcessor::initPattern()
{
    getPattern().clear();
    patternEdited();
}

void TB303AcidProcessor::loadLibraryPattern (int libraryIndex)
{
    if (! juce::isPositiveAndBelow (libraryIndex, acid::kNumLibraryPatterns))
        return;
    getPattern().loadFromString (acid::kPatternLibrary[libraryIndex].steps);
    patternEdited();
}

void TB303AcidProcessor::generatePattern()
{
    acid::AcidGenerator generator ((uint32_t) random.nextInt());
    generator.generate (getPattern(), (acid::Style) (int) raw.style->load(), (acid::Scale) (int) raw.scale->load(),
                        uiEnergy.load());
    patternEdited();
}

void TB303AcidProcessor::mutatePattern()
{
    // Change 2-4 steps: a new note, or flip accent / slide / rest. Keeps the groove recognisable.
    static constexpr int scale[] = { 0, 0, 3, 5, 7, 10, 12, 12, -2, 15 };
    auto& pattern = getPattern();
    const int changes = 2 + random.nextInt (3);
    for (int c = 0; c < changes; ++c)
    {
        const int i = random.nextInt (acid::kNumSteps);
        auto s = pattern.get (i);
        switch (random.nextInt (4))
        {
            case 0:  s.note = scale[random.nextInt ((int) std::size (scale))]; s.gate = true; break;
            case 1:  s.accent = ! s.accent; break;
            case 2:  s.slide = ! s.slide; break;
            default: s.gate = ! s.gate; break;
        }
        pattern.set (i, s);
    }
    patternEdited();
}

void TB303AcidProcessor::selectPattern (int index)
{
    auto* param = apvts.getParameter (pid::pattern);
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 ((float) (index + 1)));
    param->endChangeGesture();
}

void TB303AcidProcessor::copyPattern()
{
    const auto& pattern = getPattern();
    for (int i = 0; i < acid::kNumSteps; ++i)
        clipboard[(size_t) i] = pattern.get (i);
    hasClipboard = true;
}

void TB303AcidProcessor::pastePattern()
{
    if (! hasClipboard)
        return;

    auto& pattern = getPattern();
    for (int i = 0; i < acid::kNumSteps; ++i)
        pattern.set (i, clipboard[(size_t) i]);
    patternEdited();
}

void TB303AcidProcessor::clearGhost()
{
    engine.ghost.take.clear();
    engine.ghost.clearLearned();
    ghostName = {};
    updateGhostBrain();
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

void TB303AcidProcessor::updateGhostBrain()
{
    if (const int takes = engine.ghost.getCompletedTakes(); takes != namedTakes)
    {
        namedTakes = takes;
        ghostName = "Your take";
    }

    if (engine.ghost.needsLearning())
        engine.ghost.learn();
}

//==============================================================================
// Ghost Hands: one format for the project state and for .ghost files. The take is
// 16-bit knob positions, tick by tick, lanes interleaved, plus the pattern's flags per beat.
juce::ValueTree TB303AcidProcessor::createGhostTree() const
{
    const auto& take = engine.ghost.take;
    const int bars = take.getBars();
    if (bars == 0)
        return {};

    {
        const int count = bars * acid::ghost::kTicksPerBar * acid::ghost::kNumLanes;
        juce::MemoryBlock data ((size_t) count * 2);
        auto* out = static_cast<uint16_t*> (data.getData());
        for (int i = 0; i < count; ++i)
        {
            const float v = take.get (i / acid::ghost::kNumLanes, i % acid::ghost::kNumLanes);
            out[i] = juce::ByteOrder::swapIfBigEndian ((uint16_t) juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 65535.0f));
        }
        juce::ValueTree ghostTree ("GHOST");
        ghostTree.setProperty ("name", ghostName, nullptr);
        ghostTree.setProperty ("bars", bars, nullptr);
        ghostTree.setProperty ("phase", take.getPhase(), nullptr);
        ghostTree.setProperty ("phrase", take.getPhrase(), nullptr);
        juce::String flags;
        for (int b = 0; b < bars * acid::ghost::kBeatsPerBar; ++b)
            flags << take.getBeatFlags (b);
        ghostTree.setProperty ("flags", flags, nullptr);
        ghostTree.setProperty ("lanes", acid::ghost::kNumLanes, nullptr);
        ghostTree.setProperty ("data", data.toBase64Encoding(), nullptr);

        // Beats the ghost learned while you grabbed knobs: values, then one byte each of
        // context (low 7 bits) and "followed the beat before" (top bit)
        const auto& learned = engine.ghost.getLearnedBeats();
        if (! learned.empty())
        {
            juce::MemoryBlock beats (learned.size() * (size_t) acid::ghost::kBeatValues * 2);
            juce::MemoryBlock contexts (learned.size());
            auto* v = static_cast<uint16_t*> (beats.getData());
            for (const auto& s : learned)
                for (float x : s.values)
                    *v++ = juce::ByteOrder::swapIfBigEndian ((uint16_t) juce::roundToInt (juce::jlimit (0.0f, 1.0f, x) * 65535.0f));
            for (size_t i = 0; i < learned.size(); ++i)
                static_cast<uint8_t*> (contexts.getData())[i] = (uint8_t) ((learned[i].context & 0x7f) | (learned[i].followsPrevious ? 0x80 : 0));

            juce::ValueTree learnedTree ("LEARNED");
            learnedTree.setProperty ("count", (int) learned.size(), nullptr);
            learnedTree.setProperty ("data", beats.toBase64Encoding(), nullptr);
            learnedTree.setProperty ("contexts", contexts.toBase64Encoding(), nullptr);
            ghostTree.appendChild (learnedTree, nullptr);
        }
        return ghostTree;
    }
}

bool TB303AcidProcessor::restoreGhostTree (const juce::ValueTree& ghostTree)
{
    auto& take = engine.ghost.take;
    take.clear();
    engine.ghost.setLearnedBeats ({});
    bool loaded = false;
    ghostName = {};
    if (ghostTree.isValid())
    {
        ghostName = ghostTree.getProperty ("name").toString();
        const int bars = juce::jlimit (0, acid::ghost::kMaxBars, (int) ghostTree.getProperty ("bars", 0));
        const int count = bars * acid::ghost::kTicksPerBar * acid::ghost::kNumLanes;
        juce::MemoryBlock ghostData;
        if ((int) ghostTree.getProperty ("lanes", 0) == acid::ghost::kNumLanes
            && ghostData.fromBase64Encoding (ghostTree.getProperty ("data").toString())
            && (int) ghostData.getSize() >= count * 2)
        {
            const auto* in = static_cast<const uint16_t*> (ghostData.getData());
            for (int i = 0; i < count; ++i)
                take.set (i / acid::ghost::kNumLanes, i % acid::ghost::kNumLanes,
                          (float) juce::ByteOrder::swapIfBigEndian (in[i]) / 65535.0f);
            const juce::String flags = ghostTree.getProperty ("flags").toString();
            for (int b = 0; b < bars * acid::ghost::kBeatsPerBar; ++b)
                take.setBeatFlags (b, b < flags.length() ? juce::jlimit (0, 3, flags[b] - '0') : 0);
            const int phase = juce::jlimit (0, juce::jmax (0, bars - 1), (int) ghostTree.getProperty ("phase", 0));
            take.commit (bars, phase, (int) ghostTree.getProperty ("phrase", phase));
            loaded = bars > 0;
        }

        std::vector<acid::ghost::BeatSample> learned;
        if (auto learnedTree = ghostTree.getChildWithName ("LEARNED"); learnedTree.isValid())
        {
            const int n = juce::jlimit (0, acid::ghost::kMaxLearnedBeats, (int) learnedTree.getProperty ("count", 0));
            juce::MemoryBlock beats, contexts;
            if (beats.fromBase64Encoding (learnedTree.getProperty ("data").toString())
                && contexts.fromBase64Encoding (learnedTree.getProperty ("contexts").toString())
                && (int) beats.getSize() >= n * acid::ghost::kBeatValues * 2 && (int) contexts.getSize() >= n)
            {
                const auto* v = static_cast<const uint16_t*> (beats.getData());
                learned.resize ((size_t) n);
                for (int i = 0; i < n; ++i)
                {
                    auto& s = learned[(size_t) i];
                    for (auto& x : s.values)
                        x = (float) juce::ByteOrder::swapIfBigEndian (*v++) / 65535.0f;
                    const auto c = static_cast<const uint8_t*> (contexts.getData())[i];
                    s.context = (uint8_t) juce::jlimit (0, acid::ghost::kNumContexts - 1, (int) (c & 0x7f));
                    s.followsPrevious = (c & 0x80) != 0;
                }
            }
        }
        engine.ghost.setLearnedBeats (std::move (learned));
    }
    updateGhostBrain();
    return loaded;
}

juce::File TB303AcidProcessor::getGhostFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("TB-303 Devil Acid Rig").getChildFile ("Ghosts");
}

bool TB303AcidProcessor::saveGhostFile (const juce::File& file)
{
    auto tree = createGhostTree();
    if (! tree.isValid())
        return false;

    tree.setProperty ("name", file.getFileNameWithoutExtension(), nullptr);
    tree.setProperty ("format", 1, nullptr);
    auto xml = tree.createXml();
    if (xml == nullptr || ! file.getParentDirectory().createDirectory() || ! xml->writeTo (file))
        return false;

    ghostName = file.getFileNameWithoutExtension();
    return true;
}

bool TB303AcidProcessor::loadGhostFile (const juce::File& file)
{
    auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("GHOST"))
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (tree.getProperty ("name").toString().isEmpty())
        tree.setProperty ("name", file.getFileNameWithoutExtension(), nullptr);
    if (! restoreGhostTree (tree))
        return false;

    ghostLoaded();
    return true;
}

void TB303AcidProcessor::loadFactoryGhost (int index)
{
    if (! juce::isPositiveAndBelow (index, acid::kNumFactoryGhosts))
        return;

    acid::writeFactoryGhost (index, engine.ghost.take);
    engine.ghost.clearLearned();
    ghostName = acid::kFactoryGhosts[index].name;
    updateGhostBrain();
    ghostLoaded();
}

// A loaded ghost starts playing straight away
void TB303AcidProcessor::ghostLoaded()
{
    auto* param = apvts.getParameter (pid::ghostPerform);
    param->beginChangeGesture();
    param->setValueNotifyingHost (1.0f);
    param->endChangeGesture();
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

//==============================================================================
int TB303AcidProcessor::getNumPrograms()
{
    return presets::kNumFactory;
}

const juce::String TB303AcidProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, presets::kNumFactory) ? presets::kFactory[index].name : "";
}

void TB303AcidProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, presets::kNumFactory))
        return;

    currentProgram = index;
    const auto& preset = presets::kFactory[index];

    for (const char* id : presets::kSoundParams)
    {
        auto* param = apvts.getParameter (id);
        float value = param->convertFrom0to1 (param->getDefaultValue());
        for (const auto& v : preset.values)
            if (v.id != nullptr && std::strcmp (v.id, id) == 0)
                value = v.value;

        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

//==============================================================================
void TB303AcidProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (const char* child : { "PATTERNS", "GHOST" })
        if (auto old = state.getChildWithName (child); old.isValid())
            state.removeChild (old, nullptr);

    juce::ValueTree bank ("PATTERNS");
    for (const auto& pattern : engine.patterns)
    {
        juce::ValueTree patternTree ("PATTERN");
        for (int i = 0; i < acid::kNumSteps; ++i)
        {
            const auto s = pattern.get (i);
            juce::ValueTree step ("STEP");
            step.setProperty ("note", s.note, nullptr);
            step.setProperty ("gate", s.gate, nullptr);
            step.setProperty ("accent", s.accent, nullptr);
            step.setProperty ("slide", s.slide, nullptr);
            patternTree.appendChild (step, nullptr);
        }
        bank.appendChild (patternTree, nullptr);
    }
    state.appendChild (bank, nullptr);

    if (auto ghostTree = createGhostTree(); ghostTree.isValid())
        state.appendChild (ghostTree, nullptr);

    state.setProperty ("program", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TB303AcidProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (auto bank = tree.getChildWithName ("PATTERNS"); bank.isValid())
    {
        const int numPatterns = std::min (acid::kNumPatterns, bank.getNumChildren());
        for (int p = 0; p < numPatterns; ++p)
        {
            const auto patternTree = bank.getChild (p);
            const int numSteps = std::min (acid::kNumSteps, patternTree.getNumChildren());
            for (int i = 0; i < numSteps; ++i)
            {
                const auto step = patternTree.getChild (i);
                engine.patterns[(size_t) p].set (i, { (int) step.getProperty ("note", 0),
                                                      (bool) step.getProperty ("gate", true),
                                                      (bool) step.getProperty ("accent", false),
                                                      (bool) step.getProperty ("slide", false) });
            }
        }
        tree.removeChild (bank, nullptr);
    }

    restoreGhostTree (tree.getChildWithName ("GHOST"));
    if (auto ghostTree = tree.getChildWithName ("GHOST"); ghostTree.isValid())
        tree.removeChild (ghostTree, nullptr);

    currentProgram = juce::jlimit (0, presets::kNumFactory - 1, (int) tree.getProperty ("program", 0));
    apvts.replaceState (tree);
    ++patternVersion;
    updateGhostBrain();
}

juce::AudioProcessorEditor* TB303AcidProcessor::createEditor()
{
    return new TB303AcidEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB303AcidProcessor();
}
