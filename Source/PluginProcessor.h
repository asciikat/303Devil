#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParamIDs.h"
#include "dsp/AcidEngine.h"

#include <array>
#include <atomic>

// Lock-free ring of recent output samples for the editor's oscilloscope
class ScopeBuffer
{
public:
    static constexpr int kSize = 2048;

    void push (const float* data, int numSamples) noexcept
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            samples[(size_t) w].store (data[i], std::memory_order_relaxed);
            w = (w + 1) & (kSize - 1);
        }
        writePos.store (w, std::memory_order_release);
    }

    // Copies the newest numSamples (<= kSize) samples, oldest first
    void copyLatest (float* dest, int numSamples) const noexcept
    {
        const int w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < numSamples; ++i)
            dest[i] = samples[(size_t) ((w - numSamples + i) & (kSize - 1))].load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, kSize> samples {};
    std::atomic<int> writePos { 0 };
};

class TB303AcidProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TB303AcidProcessor();
    ~TB303AcidProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Factory sound presets (see Presets.h)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Pattern editing (message thread). Edits apply to the selected bank slot.
    int getSelectedPattern() const noexcept { return juce::jlimit (0, acid::kNumPatterns - 1, (int) raw.pattern->load() - 1); }
    void selectPattern (int index);
    acid::Pattern& getPattern() noexcept { return engine.patterns[(size_t) getSelectedPattern()]; }
    int getPatternVersion() const noexcept { return patternVersion.load(); }
    void patternEdited();
    void randomizePattern();
    void initPattern();
    void loadLibraryPattern (int libraryIndex);
    void mutatePattern();   // small random variation of the selected pattern
    void generatePattern(); // writes a new acid line in the chosen style and scale
    void copyPattern();
    void pastePattern();
    bool canPaste() const noexcept { return hasClipboard; }

    // Writes the selected pattern as a MIDI file in the temp folder, ready to drag into a host.
    // Returns an empty File if it could not be written.
    juce::File exportPatternMidi (int bars) const;

    // Ghost Hands (message thread)
    const acid::GhostHands& getGhost() const noexcept { return engine.ghost; }
    void toggleGhostRecord() noexcept { engine.ghost.toggleRecord(); }
    void clearGhost();
    void updateGhostBrain();   // re-learns if the take changed or you taught it by hand
    void loadFactoryGhost (int index);           // acid::kFactoryGhosts
    bool saveGhostFile (const juce::File&);      // .ghost = the take + beats learned by hand
    bool loadGhostFile (const juce::File&);
    const juce::String& getGhostName() const noexcept { return ghostName; }
    static juce::File getGhostFolder();

    juce::AudioProcessorValueTreeState apvts;

    // Telemetry for the editor
    ScopeBuffer scope;
    std::atomic<int> uiStep { -1 }, uiPlayingPattern { -1 };
    std::atomic<bool> uiGate { false }, uiSlide { false };
    std::atomic<float> uiAccentSweep { 0.0f };
    std::atomic<float> uiEnergy { 0.5f }, uiArcPosition { -1.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override { updateGhostBrain(); }
    juce::ValueTree createGhostTree() const;          // invalid when there is no take
    bool restoreGhostTree (const juce::ValueTree&);   // invalid tree = clear the ghost
    void ghostLoaded();
    acid::EngineParams readParams() const noexcept;

    struct RawParams
    {
        std::atomic<float> *tune, *cutoff, *resonance, *envMod, *decay, *accent, *wave, *volume,
                           *acid, *overdrive, *filterFM, *tracking, *sweep, *slideTime, *ampDecay,
                           *accentDecay, *softAttack, *muffler, *distortion, *sub,
                           *delayMix, *delayTime, *delayFeedback, *reverbMix, *reverbSize, *pump, *lfoDepth, *lfoRate,
                           *squelch, *tweak, *tweakRate, *style, *scale,
                           *ghostPerform, *ghostAmount, *ghostVariation, *ghostLength, *ghostMode,
                           *energy, *energyArc,
                           *source, *clock, *run, *bpm, *root, *shuffle, *length, *pattern, *accentVel;
    };

    RawParams raw {};
    acid::AcidEngine engine;
    std::atomic<int> patternVersion { 0 };
    juce::Random random;
    int currentProgram = 0;
    std::array<acid::Step, acid::kNumSteps> clipboard {};
    bool hasClipboard = false;
    juce::String ghostName;
    int namedTakes = 0;
    std::vector<float> tempRight;   // right channel when the host gives a mono bus

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TB303AcidProcessor)
};
