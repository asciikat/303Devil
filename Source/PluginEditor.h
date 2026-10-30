#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

juce::Font uiFont (float height, bool bold);

//==============================================================================
// Silver 303 panel in classic mode, black and yellow in acid mode
class AcidLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AcidLookAndFeel();

    void setAcid (bool shouldBeAcid);
    bool isAcid() const noexcept { return acid; }

    juce::Colour panel, section, sectionText, title, cellOff, cellText, gateOn, accentOn, slideOn,
                 playHighlight, scopeTrace, ledOff, ghost;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

private:
    bool acid = false;
    juce::Colour tick, knobHi, knobLo, pointer;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title);

    void paint (juce::Graphics&) override;
    void resized() override;

    // Where Ghost Hands is holding this knob (parameter value), or < 0 for no ghost needle
    void setGhost (float value);

private:
    static constexpr int kTitleHeight = 16;

    juce::String title;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// 16 columns: step number, note, gate, accent, slide, roll (hits per step)
class StepGrid : public juce::Component
{
public:
    StepGrid (TB303AcidProcessor&, AcidLookAndFeel&);

    void refresh();   // repaints if the pattern, playing step or length changed
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum Row { numberRow, noteRow, gateRow, accentRow, slideRow, rollRow, numRows };
    enum class Drag { none, note, paint };

    juce::Rectangle<float> cellBounds (int column, int row) const;
    int columnAt (float x) const;
    int rowAt (float y) const;
    void setNote (int column, int note);
    void setFlag (int column, int row, bool value);
    void setRoll (int column, int ratchet);
    static bool getFlag (const acid::Step&, int row);

    TB303AcidProcessor& proc;
    AcidLookAndFeel& lnf;

    int shownVersion = -1, shownStep = -2, shownLength = -1, shownRoot = -1, shownPattern = -1;
    Drag drag = Drag::none;
    int dragColumn = -1, dragStartNote = 0, paintRow = -1, lastPaintColumn = -1, paintRoll = 0;
    float dragStartY = 0.0f;
    bool paintValue = false;
};

//==============================================================================
class Scope : public juce::Component
{
public:
    Scope (TB303AcidProcessor&, AcidLookAndFeel&);
    void paint (juce::Graphics&) override;

private:
    TB303AcidProcessor& proc;
    AcidLookAndFeel& lnf;
};

//==============================================================================
// The ghost take: one strip per knob, bar lines, and the play head
class GhostView : public juce::Component
{
public:
    GhostView (TB303AcidProcessor&, AcidLookAndFeel&);
    void paint (juce::Graphics&) override;

private:
    TB303AcidProcessor& proc;
    AcidLookAndFeel& lnf;
};

//==============================================================================
// Live energy: a level bar in manual, or the arc's curve with a play head
class EnergyMeter : public juce::Component
{
public:
    EnergyMeter (TB303AcidProcessor&, AcidLookAndFeel&);
    void paint (juce::Graphics&) override;

private:
    TB303AcidProcessor& proc;
    AcidLookAndFeel& lnf;
};

//==============================================================================
class StatusLeds : public juce::Component
{
public:
    StatusLeds (TB303AcidProcessor&, AcidLookAndFeel&);
    void paint (juce::Graphics&) override;

private:
    TB303AcidProcessor& proc;
    AcidLookAndFeel& lnf;
};

//==============================================================================
// Drag this onto a track in the host (or a folder) to drop the selected pattern as a MIDI clip
class MidiDragButton : public juce::TextButton
{
public:
    MidiDragButton (TB303AcidProcessor&, const juce::ComboBox& barsBox);

    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    TB303AcidProcessor& proc;
    const juce::ComboBox& bars;
    bool dragStarted = false;
};

//==============================================================================
class AcidPanel : public juce::Component, private juce::Timer
{
public:
    explicit AcidPanel (TB303AcidProcessor&);
    ~AcidPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme();
    void renderBackground (juce::Graphics&);
    void syncControls();   // button states that follow parameters / processor state
    void ghostMenuChosen (int id);
    Knob& addKnob (std::vector<std::unique_ptr<Knob>>& row, const char* paramId, const char* title);
    void setupToggle (juce::TextButton&, const char* paramId,
                      std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&);
    void setupCombo (juce::ComboBox&, const char* paramId,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);

    TB303AcidProcessor& proc;
    AcidLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> voiceKnobs, acidKnobs, fxKnobs, seqKnobs, cheatKnobs, ghostKnobs;
    juce::TextButton acidButton, waveButton, runButton, randomButton { "RANDOM" }, initButton { "INIT" },
                     copyButton { "COPY" }, pasteButton { "PASTE" }, mutateButton { "MUTATE" },
                     undoButton { "UNDO" }, redoButton { "REDO" },
                     lockRhythmButton { "LOCK RHYTHM" }, lockPitchButton { "LOCK PITCH" },
                     generateButton { "GENERATE" }, recButton { "REC" }, ghostButton, ghostModeButton,
                     clearGhostButton { "CLEAR" };
    std::array<juce::TextButton, acid::kNumPatterns> patternButtons;
    juce::ComboBox sourceBox, clockBox, presetBox, libraryBox, styleBox, scaleBox, ghostLengthBox, ghostMenu;
    juce::ComboBox midiBarsBox, roleBox, arpModeBox, arpOctavesBox;
    MidiDragButton midiButton;
    juce::Label followLabel;   // what FOLLOW hears: key and chord
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::array<Knob*, acid::ghost::kNumLanes> ghostTargets {};   // the knobs Ghost Hands plays
    StepGrid grid;
    GhostView ghostView;
    juce::Slider energyFader;
    juce::ComboBox arcBox;
    EnergyMeter energyMeter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> energyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arcAttachment;
    Scope scope;
    StatusLeds leds;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> acidAttachment, waveAttachment, runAttachment,
                                                                          ghostAttachment, ghostModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, clockAttachment,
                                                                            styleAttachment, scaleAttachment, ghostLengthAttachment,
                                                                            roleAttachment, arpModeAttachment, arpOctavesAttachment;

    juce::Rectangle<int> headerArea, voiceArea, acidArea, fxArea, seqArea, patternLabelArea,
                         cheatArea, styleLabelArea, scaleLabelArea, ghostArea, ghostLengthLabelArea,
                         energyArea, arcLabelArea, roleLabelArea;
    bool shownAcid = false;
    int shownTakes = 0;
    juce::Image background;
    bool backgroundIsAcid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidPanel)
};

// The plugin window: the panel is laid out at its design size and scaled to fit, so the
// window can be dragged smaller or bigger (same shape). The size is saved with the session.
class TB303AcidEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kDesignWidth = 1200, kDesignHeight = 996;

    explicit TB303AcidEditor (TB303AcidProcessor&);

    void resized() override;

private:
    TB303AcidProcessor& proc;
    AcidPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TB303AcidEditor)
};
