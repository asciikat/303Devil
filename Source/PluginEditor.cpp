#include "PluginEditor.h"
#include "PunkArt.h"
#include "dsp/FactoryGhosts.h"
#include "dsp/Energy.h"

juce::Font uiFont (float height, bool bold)
{
    const int style = bold ? juce::Font::bold : juce::Font::plain;
#if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, style));
#else
    return juce::Font (height, style);
#endif
}

//==============================================================================
AcidLookAndFeel::AcidLookAndFeel()
{
    setAcid (false);
}

void AcidLookAndFeel::setAcid (bool shouldBeAcid)
{
    using juce::Colour;
    acid = shouldBeAcid;

    if (! acid)
    {
        panel       = Colour (0xffc6c9ce);
        section     = Colour (0xffb4b8be);
        sectionText = Colour (0xff3a3c42);
        title       = Colour (0xff1d1e22);
        tick        = Colour (0xff2a2b30);
        knobHi      = Colour (0xff55575d);
        knobLo      = Colour (0xff0c0c0e);
        pointer     = Colour (0xfff4f4f4);
        accentOn    = Colour (0xffe0402a);
        scopeTrace  = Colour (0xff4ce06a);
    }
    else
    {
        panel       = Colour (0xff141418);
        section     = Colour (0xff1d1d23);
        sectionText = Colour (0xfff2d10e);
        title       = Colour (0xfff2d10e);
        tick        = Colour (0xfff2d10e).withAlpha (0.6f);
        knobHi      = Colour (0xff3a3a42);
        knobLo      = Colour (0xff09090b);
        pointer     = Colour (0xfff2d10e);
        accentOn    = Colour (0xffff2e88);
        scopeTrace  = Colour (0xfff2d10e);
    }

    cellOff       = Colour (0xff26272c);
    cellText      = Colour (0xffe8e8ea);
    gateOn        = acid ? Colour (0xff39ff14) : Colour (0xff36b4e8);
    slideOn       = Colour (0xfff2c40e);
    playHighlight = Colour (0xfff2d10e).withAlpha (0.25f);
    ledOff        = Colour (0xff2a2a2e);
    ghost         = Colour (0xff00e5ff);

    const auto valueText = acid ? Colour (0xff7dff5a) : Colour (0xff2b2d33);

    setColour (juce::ResizableWindow::backgroundColourId, panel);
    setColour (juce::Label::textColourId, title);
    setColour (juce::Slider::textBoxTextColourId, valueText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accentOn.withAlpha (0.4f));
    setColour (juce::TextButton::buttonColourId, acid ? Colour (0xff26262c) : Colour (0xff2b2d33));
    setColour (juce::TextButton::buttonOnColourId, acid ? Colour (0xfff2d10e) : accentOn);
    setColour (juce::TextButton::textColourOffId, Colour (0xffdedee2));
    setColour (juce::TextButton::textColourOnId, acid ? juce::Colours::black : juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, Colour (0xff26262c));
    setColour (juce::ComboBox::textColourId, Colour (0xffe8e8ea));
    setColour (juce::ComboBox::outlineColourId, acid ? Colour (0xfff2d10e).withAlpha (0.5f) : Colour (0xff111114));
    setColour (juce::ComboBox::arrowColourId, acid ? Colour (0xfff2d10e) : Colour (0xffe8e8ea));
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff202024));
    setColour (juce::PopupMenu::textColourId, Colour (0xffe8e8ea));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, acid ? Colour (0xfff2d10e) : accentOn);
    setColour (juce::PopupMenu::highlightedTextColourId, acid ? juce::Colours::black : juce::Colours::white);
}

void AcidLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (tick);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = rotaryStartAngle + (float) i / 10.0f * (rotaryEndAngle - rotaryStartAngle);
        g.drawLine ({ centre.getPointOnCircumference (radius, a), centre.getPointOnCircumference (radius - 4.0f, a) }, 1.2f);
    }

    const float r = radius - 7.0f;
    if (r <= 2.0f)
        return;

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (centre.x - r + 1.5f, centre.y - r + 2.5f, 2.0f * r, 2.0f * r);

    g.setGradientFill (juce::ColourGradient (knobHi, centre.x - r * 0.4f, centre.y - r * 0.6f,
                                             knobLo, centre.x + r * 0.6f, centre.y + r * 0.9f, true));
    g.fillEllipse (centre.x - r, centre.y - r, 2.0f * r, 2.0f * r);
    g.setColour (juce::Colours::black);
    g.drawEllipse (centre.x - r, centre.y - r, 2.0f * r, 2.0f * r, 1.0f);

    juce::Path p;
    p.addRoundedRectangle (-1.5f, -r + 2.0f, 3.0f, r * 0.55f, 1.5f);
    g.setColour (pointer);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre));

    // Ghost needle: a see-through second pointer showing where Ghost Hands has the knob
    const auto& ghostValue = slider.getProperties()["ghost"];
    if (! ghostValue.isVoid() && (double) ghostValue >= 0.0)
    {
        const auto proportion = (float) juce::jlimit (0.0, 1.0, slider.valueToProportionOfLength ((double) ghostValue));
        const float ghostAngle = rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);
        const juce::Line<float> needle (centre.getPointOnCircumference (r * 0.25f, ghostAngle),
                                        centre.getPointOnCircumference (radius - 1.0f, ghostAngle));
        g.setColour (ghost.withAlpha (0.25f));
        g.drawLine (needle, 6.0f);
        g.setColour (ghost.withAlpha (0.85f));
        g.drawLine (needle, 2.0f);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (needle.getEnd()));
    }
}

void AcidLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isMouseOver, bool isButtonDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    auto base = button.findColour (button.getToggleState() ? juce::TextButton::buttonOnColourId
                                                           : juce::TextButton::buttonColourId);
    if (isButtonDown)
        base = base.darker (0.25f);
    else if (isMouseOver)
        base = base.brighter (0.1f);

    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font AcidLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (std::min (14.0f, (float) buttonHeight * 0.5f), true);
}

juce::Font AcidLookAndFeel::getLabelFont (juce::Label&)
{
    return uiFont (12.0f, false);
}

juce::Font AcidLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return uiFont (13.0f, true);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& knobTitle)
    : title (knobTitle)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 15);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (auto* param = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (findColour (juce::Label::textColourId));
    g.setFont (uiFont (11.5f, true));
    g.drawText (title, getLocalBounds().removeFromTop (kTitleHeight), juce::Justification::centred, false);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (kTitleHeight));
}

void Knob::setGhost (float value)
{
    const auto& current = slider.getProperties()["ghost"];
    const float shown = current.isVoid() ? -1.0f : (float) current;
    if (std::abs (shown - value) < 0.002f || (shown < 0.0f && value < 0.0f))
        return;
    slider.getProperties().set ("ghost", value);
    slider.repaint();
}

//==============================================================================
namespace
{
constexpr float kRowHeights[] = { 18.0f, 44.0f, 24.0f, 24.0f, 24.0f };
constexpr float kRowGap = 4.0f;

float rowTop (int row)
{
    float y = 0.0f;
    for (int r = 0; r < row; ++r)
        y += kRowHeights[r] + kRowGap;
    return y;
}

int currentLength (TB303AcidProcessor& p)
{
    return (int) p.apvts.getRawParameterValue (pid::length)->load();
}

int currentRoot (TB303AcidProcessor& p)
{
    return (int) p.apvts.getRawParameterValue (pid::root)->load();
}
} // namespace

StepGrid::StepGrid (TB303AcidProcessor& p, AcidLookAndFeel& l) : proc (p), lnf (l) {}

void StepGrid::refresh()
{
    const int version = proc.getPatternVersion();
    const int step = proc.uiStep.load();
    const int length = currentLength (proc);
    const int root = currentRoot (proc);
    const int pattern = proc.getSelectedPattern();

    if (version != shownVersion || step != shownStep || length != shownLength || root != shownRoot
        || pattern != shownPattern)
    {
        shownVersion = version;
        shownStep = step;
        shownLength = length;
        shownRoot = root;
        shownPattern = pattern;
        repaint();
    }
}

juce::Rectangle<float> StepGrid::cellBounds (int column, int row) const
{
    const float columnWidth = (float) getWidth() / (float) acid::kNumSteps;
    return { columnWidth * (float) column + 2.0f, rowTop (row), columnWidth - 4.0f, kRowHeights[row] };
}

int StepGrid::columnAt (float x) const
{
    const int c = (int) std::floor (x / ((float) getWidth() / (float) acid::kNumSteps));
    return (c >= 0 && c < acid::kNumSteps) ? c : -1;
}

int StepGrid::rowAt (float y) const
{
    for (int r = 0; r < numRows; ++r)
        if (y >= rowTop (r) && y < rowTop (r) + kRowHeights[r])
            return r;
    return -1;
}

bool StepGrid::getFlag (const acid::Step& s, int row)
{
    return row == gateRow ? s.gate : (row == accentRow ? s.accent : s.slide);
}

void StepGrid::setFlag (int column, int row, bool value)
{
    auto s = proc.getPattern().get (column);
    if (row == gateRow)        s.gate = value;
    else if (row == accentRow) s.accent = value;
    else if (row == slideRow)  s.slide = value;
    proc.getPattern().set (column, s);
    repaint();
}

void StepGrid::setNote (int column, int note)
{
    auto s = proc.getPattern().get (column);
    s.note = juce::jlimit (acid::kMinNoteOffset, acid::kMaxNoteOffset, note);
    proc.getPattern().set (column, s);
    repaint();
}

void StepGrid::paint (juce::Graphics& g)
{
    const int length = currentLength (proc);
    const int root = currentRoot (proc);
    // Only highlight the running step when the grid shows the pattern that is playing
    const int playing = proc.uiPlayingPattern.load() == proc.getSelectedPattern() ? proc.uiStep.load() : -1;

    for (int i = 0; i < acid::kNumSteps; ++i)
    {
        const auto s = proc.getPattern().get (i);
        const float alpha = i < length ? 1.0f : 0.3f;

        // Step number, brighter on each beat
        const auto numberCell = cellBounds (i, numberRow);
        if (i == playing)
        {
            g.setColour (lnf.accentOn);
            g.fillEllipse (numberCell.withSizeKeepingCentre (8.0f, 8.0f).translated (-12.0f, 0.0f));
        }
        g.setColour (lnf.sectionText.withAlpha (alpha * (i % 4 == 0 ? 1.0f : 0.65f)));
        g.setFont (uiFont (12.0f, i % 4 == 0));
        g.drawText (juce::String (i + 1), numberCell, juce::Justification::centred, false);

        // Note
        const auto noteCell = cellBounds (i, noteRow);
        g.setColour (lnf.cellOff.withAlpha (alpha));
        g.fillRoundedRectangle (noteCell, 3.0f);
        const int midiNote = juce::jlimit (0, 127, root + s.note);
        g.setColour (lnf.cellText.withAlpha (alpha * (s.gate ? 1.0f : 0.4f)));
        g.setFont (uiFont (14.0f, true));
        g.drawText (juce::MidiMessage::getMidiNoteName (midiNote, true, true, 4),
                    noteCell.withTrimmedBottom (16.0f), juce::Justification::centredBottom, false);
        g.setFont (uiFont (10.5f, false));
        g.setColour (lnf.cellText.withAlpha (alpha * 0.55f));
        g.drawText ((s.note > 0 ? "+" : "") + juce::String (s.note),
                    noteCell.withTrimmedTop (28.0f), juce::Justification::centredTop, false);

        // Gate / accent / slide
        auto drawFlag = [&] (int row, bool on, juce::Colour onColour, const char* label)
        {
            const auto cell = cellBounds (i, row);
            g.setColour ((on ? onColour : lnf.cellOff).withAlpha (alpha));
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour ((on ? juce::Colours::black : lnf.cellText.withAlpha (0.4f)).withMultipliedAlpha (alpha));
            g.setFont (uiFont (10.5f, true));
            g.drawText (label, cell, juce::Justification::centred, false);
        };
        drawFlag (gateRow, s.gate, lnf.gateOn, s.gate ? "ON" : "REST");
        drawFlag (accentRow, s.accent, lnf.accentOn, "ACC");
        drawFlag (slideRow, s.slide, lnf.slideOn, "SLD");

        if (i == playing)
        {
            g.setColour (lnf.playHighlight);
            const float top = rowTop (noteRow);
            g.fillRoundedRectangle (cellBounds (i, noteRow).withHeight ((float) getHeight() - top).expanded (1.0f, 1.0f), 4.0f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int column = columnAt (e.position.x);
    const int row = rowAt (e.position.y);
    if (column < 0 || row <= numberRow)
        return;

    const auto s = proc.getPattern().get (column);

    if (row == noteRow)
    {
        drag = Drag::note;
        dragColumn = column;
        dragStartNote = s.note;
        dragStartY = e.position.y;
        return;
    }

    // Toggle, then drag across other steps to paint the same value
    drag = Drag::paint;
    paintRow = row;
    paintValue = ! getFlag (s, row);
    lastPaintColumn = column;
    setFlag (column, row, paintValue);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::note)
    {
        const int delta = juce::roundToInt ((dragStartY - e.position.y) / 7.0f);
        setNote (dragColumn, dragStartNote + delta);
    }
    else if (drag == Drag::paint)
    {
        const int column = juce::jlimit (0, acid::kNumSteps - 1,
                                         (int) std::floor (e.position.x / ((float) getWidth() / (float) acid::kNumSteps)));
        const int step = column > lastPaintColumn ? 1 : -1;
        while (column != lastPaintColumn)
        {
            lastPaintColumn += step;
            setFlag (lastPaintColumn, paintRow, paintValue);
        }
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (drag != Drag::none)
        proc.patternEdited();
    drag = Drag::none;
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int column = columnAt (e.position.x);
    if (column >= 0 && rowAt (e.position.y) == noteRow)
    {
        setNote (column, 0);
        proc.patternEdited();
    }
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int column = columnAt (e.position.x);
    if (column < 0 || rowAt (e.position.y) != noteRow || std::abs (wheel.deltaY) < 1.0e-6f)
        return;

    setNote (column, proc.getPattern().get (column).note + (wheel.deltaY > 0.0f ? 1 : -1));
    proc.patternEdited();
}

//==============================================================================
Scope::Scope (TB303AcidProcessor& p, AcidLookAndFeel& l) : proc (p), lnf (l) {}

void Scope::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07090b));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    std::array<float, ScopeBuffer::kSize> buf;
    proc.scope.copyLatest (buf.data(), (int) buf.size());

    // Trigger on the latest rising zero crossing that still leaves a full window after it
    constexpr int window = ScopeBuffer::kSize / 2;
    int start = 0;
    for (int i = window - 1; i > 0; --i)
    {
        if (buf[(size_t) i - 1] <= 0.0f && buf[(size_t) i] > 0.0f)
        {
            start = i;
            break;
        }
    }

    juce::Path path;
    const float mid = r.getCentreY();
    const float scale = r.getHeight() * 0.45f;
    for (int i = 0; i < window; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (window - 1);
        const float y = mid - juce::jlimit (-1.0f, 1.0f, buf[(size_t) (start + i)]) * scale;
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (lnf.scopeTrace);
    g.strokePath (path, juce::PathStrokeType (1.4f));
}

//==============================================================================
GhostView::GhostView (TB303AcidProcessor& p, AcidLookAndFeel& l) : proc (p), lnf (l) {}

void GhostView::paint (juce::Graphics& g)
{
    using namespace acid::ghost;
    const auto& ghost = proc.getGhost();
    const auto& take = ghost.take;
    const int state = ghost.getRecState();
    const int bars = take.getBars();

    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07090b));
    g.fillRoundedRectangle (r, 4.0f);
    r.reduce (4.0f, 3.0f);

    static const char* const laneNames[] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV" };
    auto labels = r.removeFromLeft (30.0f);
    const float laneHeight = r.getHeight() / (float) kNumLanes;
    g.setFont (uiFont (9.5f, true));
    for (int lane = 0; lane < kNumLanes; ++lane)
    {
        const bool yours = ghost.isLaneYours (lane);
        g.setColour (yours ? juce::Colour (0xffff2e88) : lnf.ghost.withAlpha (0.55f));
        g.drawText (yours ? "YOU" : laneNames[lane], labels.withY (r.getY() + laneHeight * (float) lane).withHeight (laneHeight),
                    juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) (r.getY() + laneHeight * (float) (lane + 1)), r.getX(), r.getRight());
    }

    if (state == acid::GhostHands::recording)
    {
        const float progress = ghost.getRecordProgress();
        g.setColour (juce::Colour (0xffff2a2a).withAlpha (0.25f));
        g.fillRect (r.withWidth (r.getWidth() * progress));
        g.setColour (juce::Colour (0xffff4040));
        g.setFont (uiFont (13.0f, true));
        g.drawText ("RECORDING YOUR HANDS...  ride the knobs", r, juce::Justification::centred, false);
        return;
    }

    if (bars == 0)
    {
        g.setColour (lnf.ghost.withAlpha (0.7f));
        g.setFont (uiFont (12.0f, true));
        g.drawText (state == acid::GhostHands::armed ? "ARMED - recording starts on the next bar (press play)"
                                                     : "NO TAKE YET - press REC, play, and ride the knobs",
                    r, juce::Justification::centred, false);
        return;
    }

    // Bar lines
    for (int b = 0; b <= bars; ++b)
    {
        const float x = r.getX() + r.getWidth() * (float) b / (float) bars;
        g.setColour (juce::Colours::white.withAlpha (b % 4 == 0 ? 0.18f : 0.08f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }

    // Lanes, sampled down to the view width. Each lane zooms to its own range so small
    // moves still show their shape; a knob you didn't touch is a flat line in the middle.
    const int ticks = bars * kTicksPerBar;
    const int points = juce::jmax (2, (int) r.getWidth());
    for (int lane = 0; lane < kNumLanes; ++lane)
    {
        float lo = 1.0f, hi = 0.0f;
        for (int t = 0; t < ticks; ++t)
        {
            lo = juce::jmin (lo, take.get (t, lane));
            hi = juce::jmax (hi, take.get (t, lane));
        }
        const float span = juce::jmax (0.02f, hi - lo), mid = 0.5f * (lo + hi);

        const float top = r.getY() + laneHeight * (float) lane + 1.0f, h = laneHeight - 2.0f;
        juce::Path path;
        for (int i = 0; i < points; ++i)
        {
            const int t = juce::jmin (ticks - 1, i * ticks / points);
            const float x = r.getX() + r.getWidth() * (float) i / (float) (points - 1);
            const float y = top + h * (0.5f - (take.get (t, lane) - mid) / span);
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }
        g.setColour (lnf.ghost.withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    // Play head. In LEARN mode it jumps around: it marks which beat of yours the ghost
    // is borrowing right now.
    const bool learnMode = proc.apvts.getRawParameterValue (pid::ghostMode)->load() > 0.5f;
    const float position = ghost.getPlayPosition();
    if (position >= 0.0f)
    {
        const float x = r.getX() + r.getWidth() * position;
        if (learnMode)
        {
            const float beatWidth = r.getWidth() / (float) (bars * kBeatsPerBar);
            const float beatX = r.getX() + beatWidth * std::floor ((x - r.getX()) / beatWidth);
            g.setColour (lnf.ghost.withAlpha (0.15f));
            g.fillRect (beatX, r.getY(), beatWidth, r.getHeight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (x, r.getY(), x, r.getBottom(), 1.5f);
    }

    juce::String info = proc.getGhostName().toUpperCase();
    if (info.isNotEmpty())
        info << "  /  ";
    info << bars << (bars == 1 ? " BAR" : " BARS");
    if (learnMode && ghost.getMoveTypes() > 0)
    {
        info << "  /  " << ghost.getMoveTypes() << " MOVE TYPES";
        if (ghost.getLearnedCount() > 0)
            info << "  +" << ghost.getLearnedCount() << " BEATS BY HAND";
        if (ghost.isPlayingLiveMove())
            info << "  /  PLAYING A MOVE YOU TAUGHT IT";
    }
    g.setColour (lnf.ghost.withAlpha (0.75f));
    g.setFont (uiFont (10.0f, true));
    g.drawText (info, r.reduced (3.0f, 0.0f), juce::Justification::topRight, false);
}

//==============================================================================
EnergyMeter::EnergyMeter (TB303AcidProcessor& p, AcidLookAndFeel& l) : proc (p), lnf (l) {}

void EnergyMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07090b));
    g.fillRoundedRectangle (r, 4.0f);
    r.reduce (4.0f, 3.0f);

    const float energy = proc.uiEnergy.load();
    const float arcPosition = proc.uiArcPosition.load();
    const int arc = (int) proc.apvts.getRawParameterValue (pid::energyArc)->load();
    const auto hot = juce::Colour (0xff39ff14).interpolatedWith (juce::Colour (0xffff2e88), energy);

    auto text = r.removeFromRight (78.0f);
    g.setColour (hot);
    g.setFont (uiFont (13.0f, true));
    g.drawText (juce::String (juce::roundToInt (energy * 100.0f)) + "%", text, juce::Justification::centred, false);

    if (arc <= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (r.withTrimmedTop (r.getHeight() * 0.3f));
        g.setColour (hot);
        g.fillRect (r.withTrimmedTop (r.getHeight() * 0.3f).withWidth (r.getWidth() * energy));
        return;
    }

    // The arc's curve over one cycle, and where the song is on it
    juce::Path curve;
    const int points = juce::jmax (2, (int) r.getWidth());
    const double bars = acid::kEnergyArcs[arc].bars;
    for (int i = 0; i < points; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (points - 1);
        const float y = r.getBottom() - r.getHeight() * (float) acid::arcEnergy (arc, bars * i / (points - 1));
        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }
    g.setColour (lnf.sectionText.withAlpha (0.25f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    g.setColour (hot.withAlpha (0.9f));
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    if (arcPosition >= 0.0f)
    {
        const float x = r.getX() + r.getWidth() * arcPosition;
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (x, r.getY(), x, r.getBottom(), 1.5f);
        g.setColour (hot);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, r.getBottom() - r.getHeight() * energy }));
    }
}

//==============================================================================
StatusLeds::StatusLeds (TB303AcidProcessor& p, AcidLookAndFeel& l) : proc (p), lnf (l) {}

void StatusLeds::paint (juce::Graphics& g)
{
    struct Led { const char* label; float level; juce::Colour colour; };
    const Led leds[] = {
        { "GATE",   proc.uiGate.load() ? 1.0f : 0.0f,  lnf.gateOn },
        { "SLIDE",  proc.uiSlide.load() ? 1.0f : 0.0f, lnf.slideOn },
        { "ACCENT", juce::jlimit (0.0f, 1.0f, proc.uiAccentSweep.load() * 1.6f), lnf.accentOn },
    };

    auto area = getLocalBounds().toFloat();
    const float w = area.getWidth() / 3.0f;
    for (const auto& led : leds)
    {
        auto cell = area.removeFromLeft (w);
        const auto dot = cell.removeFromLeft (22.0f).withSizeKeepingCentre (14.0f, 14.0f);
        g.setColour (lnf.ledOff);
        g.fillEllipse (dot);
        g.setColour (led.colour.withAlpha (led.level));
        g.fillEllipse (dot.reduced (2.0f));
        g.setColour (lnf.sectionText);
        g.setFont (uiFont (12.0f, true));
        g.drawText (led.label, cell, juce::Justification::centredLeft, false);
    }

    const int step = proc.uiStep.load();
    g.setColour (lnf.sectionText.withAlpha (0.8f));
    g.setFont (uiFont (11.0f, false));
    g.drawText (step >= 0 ? "STEP " + juce::String (step + 1) : juce::String ("STOPPED"),
                getLocalBounds().toFloat().removeFromBottom (14.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
AcidPanel::AcidPanel (TB303AcidProcessor& p)
    : proc (p), grid (p, lnf), ghostView (p, lnf), energyMeter (p, lnf),
      scope (p, lnf), leds (p, lnf)
{
    setLookAndFeel (&lnf);

    addKnob (voiceKnobs, pid::tune,      "TUNING");
    addKnob (voiceKnobs, pid::cutoff,    "CUTOFF");
    addKnob (voiceKnobs, pid::resonance, "RESONANCE");
    addKnob (voiceKnobs, pid::envMod,    "ENV MOD");
    addKnob (voiceKnobs, pid::decay,     "DECAY");
    addKnob (voiceKnobs, pid::accent,    "ACCENT");
    addKnob (voiceKnobs, pid::volume,    "VOLUME");

    addKnob (acidKnobs, pid::overdrive,   "OVERDRIVE");
    addKnob (acidKnobs, pid::filterFM,    "FILTER FM");
    addKnob (acidKnobs, pid::tracking,    "TRACKING");
    addKnob (acidKnobs, pid::sweep,       "SWEEP SPD");
    addKnob (acidKnobs, pid::slideTime,   "SLIDE");
    addKnob (acidKnobs, pid::ampDecay,    "AMP DECAY");
    addKnob (acidKnobs, pid::accentDecay, "ACC DECAY");
    addKnob (acidKnobs, pid::softAttack,  "ATTACK");
    addKnob (acidKnobs, pid::muffler,     "MUFFLER");
    addKnob (acidKnobs, pid::distortion,  "DISTORT");
    addKnob (acidKnobs, pid::sub,         "SUB");

    addKnob (fxKnobs, pid::delayMix,      "DELAY");
    addKnob (fxKnobs, pid::delayTime,     "DLY TIME");
    addKnob (fxKnobs, pid::delayFeedback, "FEEDBACK");
    addKnob (fxKnobs, pid::reverbMix,     "REVERB");
    addKnob (fxKnobs, pid::reverbSize,    "ROOM");
    addKnob (fxKnobs, pid::pump,          "PUMP");
    addKnob (fxKnobs, pid::lfoDepth,      "LFO > CUT");
    addKnob (fxKnobs, pid::lfoRate,       "LFO RATE");

    addKnob (cheatKnobs, pid::squelch,   "SQUELCH");
    addKnob (cheatKnobs, pid::tweak,     "AUTO TWEAK");
    addKnob (cheatKnobs, pid::tweakRate, "TWEAK RATE");

    addKnob (ghostKnobs, pid::ghostAmount,    "AMOUNT");
    addKnob (ghostKnobs, pid::ghostVariation, "VARIATION");

    // Knobs the ghost plays, in acid::ghost::Lane order
    ghostTargets = { voiceKnobs[1].get(), voiceKnobs[2].get(), voiceKnobs[3].get(),
                     voiceKnobs[4].get(), voiceKnobs[5].get(), acidKnobs[0].get() };

    addKnob (seqKnobs, pid::bpm,       "FREE BPM");
    addKnob (seqKnobs, pid::root,      "ROOT");
    addKnob (seqKnobs, pid::shuffle,   "SHUFFLE");
    addKnob (seqKnobs, pid::length,    "LENGTH");
    addKnob (seqKnobs, pid::accentVel, "ACC VEL");

    setupToggle (acidButton, pid::acid, acidAttachment);
    setupToggle (waveButton, pid::wave, waveAttachment);
    setupToggle (runButton, pid::run, runAttachment);
    runButton.setButtonText ("RUN");
    setupCombo (sourceBox, pid::source, sourceAttachment);
    setupCombo (clockBox, pid::clock, clockAttachment);
    setupCombo (styleBox, pid::style, styleAttachment);
    setupCombo (scaleBox, pid::scale, scaleAttachment);
    setupToggle (ghostButton, pid::ghostPerform, ghostAttachment);
    setupToggle (ghostModeButton, pid::ghostMode, ghostModeAttachment);
    setupCombo (ghostLengthBox, pid::ghostLength, ghostLengthAttachment);

    // Factory ghosts, and .ghost files
    ghostMenu.setTextWhenNothingSelected ("GHOSTS...");
    for (int i = 0; i < acid::kNumFactoryGhosts; ++i)
        ghostMenu.addItem (acid::kFactoryGhosts[i].name, i + 1);
    ghostMenu.addSeparator();
    ghostMenu.addItem ("Save ghost to file...", 100);
    ghostMenu.addItem ("Load ghost file...", 101);
    ghostMenu.onChange = [this]
    {
        const int id = ghostMenu.getSelectedId();
        ghostMenu.setSelectedId (0, juce::dontSendNotification);
        ghostMenuChosen (id);
    };
    addAndMakeVisible (ghostMenu);

    // Energy: a long fader to ride, or an arc that rides it for you
    energyFader.setSliderStyle (juce::Slider::LinearHorizontal);
    energyFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    energyFader.setDoubleClickReturnValue (true, 0.5);
    addAndMakeVisible (energyFader);
    energyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, pid::energy, energyFader);
    setupCombo (arcBox, pid::energyArc, arcAttachment);
    addAndMakeVisible (energyMeter);

    recButton.onClick = [this] { proc.toggleGhostRecord(); };
    clearGhostButton.onClick = [this] { proc.clearGhost(); };
    addAndMakeVisible (recButton);
    addAndMakeVisible (clearGhostButton);
    addAndMakeVisible (ghostView);
    shownTakes = proc.getGhost().getCompletedTakes();

    generateButton.onClick = [this] { proc.generatePattern(); };
    generateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffff2e88));
    generateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    addAndMakeVisible (generateButton);

    randomButton.onClick = [this] { proc.randomizePattern(); };
    initButton.onClick = [this] { proc.initPattern(); };
    copyButton.onClick = [this] { proc.copyPattern(); };
    pasteButton.onClick = [this] { proc.pastePattern(); };
    mutateButton.onClick = [this] { proc.mutatePattern(); };
    for (auto* b : { &randomButton, &initButton, &copyButton, &pasteButton, &mutateButton })
        addAndMakeVisible (*b);

    for (int i = 0; i < acid::kNumPatterns; ++i)
    {
        auto& b = patternButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.onClick = [this, i] { proc.selectPattern (i); };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    libraryBox.setTextWhenNothingSelected ("LOAD ACID PATTERN...");
    for (int i = 0; i < acid::kNumLibraryPatterns; ++i)
        libraryBox.addItem (acid::kPatternLibrary[i].name, i + 1);
    libraryBox.onChange = [this]
    {
        proc.loadLibraryPattern (libraryBox.getSelectedItemIndex());
        libraryBox.setSelectedId (0, juce::dontSendNotification);
    };
    addAndMakeVisible (libraryBox);

    addAndMakeVisible (grid);
    addAndMakeVisible (scope);
    addAndMakeVisible (leds);

    shownAcid = proc.apvts.getRawParameterValue (pid::acid)->load() > 0.5f;
    applyTheme();
    syncControls();

    setSize (TB303AcidEditor::kDesignWidth, TB303AcidEditor::kDesignHeight);
    startTimerHz (30);
}

AcidPanel::~AcidPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob& AcidPanel::addKnob (std::vector<std::unique_ptr<Knob>>& row, const char* paramId, const char* title)
{
    row.push_back (std::make_unique<Knob> (proc.apvts, paramId, title));
    addAndMakeVisible (*row.back());
    return *row.back();
}

void AcidPanel::setupToggle (juce::TextButton& button, const char* paramId,
                                   std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment)
{
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, button);
}

void AcidPanel::setupCombo (juce::ComboBox& box, const char* paramId,
                                  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

void AcidPanel::applyTheme()
{
    lnf.setAcid (shownAcid);
    energyFader.setColour (juce::Slider::trackColourId, shownAcid ? juce::Colour (0xffff2e88) : lnf.accentOn);
    energyFader.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff26262c));
    energyFader.setColour (juce::Slider::thumbColourId, shownAcid ? juce::Colour (0xfff2d10e) : juce::Colour (0xfff4f4f4));
    acidButton.setButtonText (shownAcid ? "ACID MODE: ON" : "ACID MODE: OFF");
    for (auto& k : acidKnobs)
        k->setAlpha (shownAcid ? 1.0f : 0.4f);
    sendLookAndFeelChange();
    repaint();
}

void AcidPanel::timerCallback()
{
    const bool acid = proc.apvts.getRawParameterValue (pid::acid)->load() > 0.5f;
    if (acid != shownAcid)
    {
        shownAcid = acid;
        applyTheme();
    }

    syncControls();
    grid.refresh();
    scope.repaint();
    leds.repaint();
    ghostView.repaint();
    energyMeter.repaint();
}

void AcidPanel::ghostMenuChosen (int id)
{
    if (id >= 1 && id <= acid::kNumFactoryGhosts)
    {
        proc.loadFactoryGhost (id - 1);
        return;
    }
    if (id != 100 && id != 101)
        return;

    const bool saving = id == 100;
    const auto folder = TB303AcidProcessor::getGhostFolder();
    folder.createDirectory();
    const auto start = saving ? folder.getChildFile ((proc.getGhostName().isEmpty() ? juce::String ("my ghost") : proc.getGhostName()) + ".ghost")
                              : folder;
    fileChooser = std::make_unique<juce::FileChooser> (saving ? "Save ghost" : "Load ghost", start, "*.ghost");

    const int chooserFlags = juce::FileBrowserComponent::canSelectFiles
                    | (saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                              : juce::FileBrowserComponent::openMode);
    fileChooser->launchAsync (chooserFlags, [this, saving] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
            return;
        if (saving)
            file = file.withFileExtension ("ghost");

        const bool ok = saving ? proc.saveGhostFile (file) : proc.loadGhostFile (file);
        if (! ok)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Ghost Hands",
                                                    saving ? "Nothing to save yet: record a take or load a ghost first."
                                                           : "That file isn't a ghost this plugin can read.");
    });
}

void AcidPanel::syncControls()
{
    const juce::String waveText = proc.apvts.getRawParameterValue (pid::wave)->load() > 0.5f ? "WAVE: SQUARE" : "WAVE: SAW";
    if (waveButton.getButtonText() != waveText)
        waveButton.setButtonText (waveText);

    // Selected slot is lit; the slot actually playing (if different) is marked with dots
    const int selected = proc.getSelectedPattern();
    const int playingPattern = proc.uiPlayingPattern.load();
    for (int i = 0; i < acid::kNumPatterns; ++i)
    {
        auto& b = patternButtons[(size_t) i];
        b.setToggleState (i == selected, juce::dontSendNotification);
        const juce::String text = i == playingPattern && i != selected ? "." + juce::String (i + 1) + "."
                                                                       : juce::String (i + 1);
        if (b.getButtonText() != text)
            b.setButtonText (text);
    }
    pasteButton.setEnabled (proc.canPaste());

    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);

    // Ghost Hands
    const auto& ghost = proc.getGhost();
    const int recState = ghost.getRecState();
    const bool blink = (juce::Time::getMillisecondCounter() / 300) % 2 == 0;
    juce::String recText ("REC");
    auto recColour = findColour (juce::TextButton::buttonColourId);
    if (recState == acid::GhostHands::armed)
    {
        recText = "ARMED";
        recColour = blink ? juce::Colour (0xffff8a00) : juce::Colour (0xff5a3200);
    }
    else if (recState == acid::GhostHands::recording)
    {
        recText = "STOP REC";
        recColour = juce::Colour (0xffe01818);
    }
    if (recButton.getButtonText() != recText)
        recButton.setButtonText (recText);
    recButton.setColour (juce::TextButton::buttonColourId, recColour);

    const bool performing = proc.apvts.getRawParameterValue (pid::ghostPerform)->load() > 0.5f;
    const juce::String ghostText = performing ? "GHOST: ON" : "GHOST: OFF";
    if (ghostButton.getButtonText() != ghostText)
        ghostButton.setButtonText (ghostText);
    const juce::String modeText = proc.apvts.getRawParameterValue (pid::ghostMode)->load() > 0.5f ? "MODE: LEARN" : "MODE: LOOP";
    if (ghostModeButton.getButtonText() != modeText)
        ghostModeButton.setButtonText (modeText);
    clearGhostButton.setEnabled (ghost.take.getBars() > 0);

    // A fresh take goes straight into PERFORM, so you hear the ghost play it back
    if (ghost.getCompletedTakes() != shownTakes)
    {
        shownTakes = ghost.getCompletedTakes();
        if (! performing)
        {
            auto* param = proc.apvts.getParameter (pid::ghostPerform);
            param->beginChangeGesture();
            param->setValueNotifyingHost (1.0f);
            param->endChangeGesture();
        }
    }

    for (int lane = 0; lane < acid::ghost::kNumLanes; ++lane)
        ghostTargets[(size_t) lane]->setGhost (ghost.getOutput (lane));

    // The fader does nothing while an arc is driving the energy
    const bool arcOn = proc.apvts.getRawParameterValue (pid::energyArc)->load() > 0.5f;
    energyFader.setAlpha (arcOn ? 0.35f : 1.0f);
}

void AcidPanel::paint (juce::Graphics& g)
{
    // The panel artwork is static, so it's rendered once into an image and reused
    const auto scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    const int w = juce::roundToInt ((float) getWidth() * scale), h = juce::roundToInt ((float) getHeight() * scale);
    if (background.isNull() || background.getWidth() != w || background.getHeight() != h || backgroundIsAcid != shownAcid)
    {
        background = juce::Image (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (scale));
        renderBackground (bg);
        backgroundIsAcid = shownAcid;
    }
    g.drawImage (background, getLocalBounds().toFloat());
}

void AcidPanel::renderBackground (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const juce::String seqHint ("drag or scroll notes, click to toggle, drag across steps to paint, double-click note = root");

    if (! shownAcid)
    {
        // Stock silver panel, with one acid sticker slapped on it
        g.fillAll (lnf.panel);
        punk::drawSmiley (g, headerArea.toFloat().removeFromLeft (34.0f).reduced (2.0f), false, false);
        g.setColour (lnf.title);
        g.setFont (uiFont (22.0f, true));
        g.drawText ("TB-303  DEVIL ACID RIG", headerArea.withTrimmedLeft (44), juce::Justification::centredLeft, false);

        auto drawSection = [&] (juce::Rectangle<int> area, const juce::String& name)
        {
            g.setColour (lnf.section);
            g.fillRoundedRectangle (area.toFloat(), 6.0f);
            g.setColour (lnf.sectionText);
            g.setFont (uiFont (11.0f, true));
            g.drawText (name, area.reduced (10, 4).removeFromTop (14), juce::Justification::centredLeft, false);
        };

        drawSection (voiceArea, "VOICE");
        drawSection (cheatArea, "CHEAT  -  MAKES ACID EASY");
        g.setColour (lnf.sectionText);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("STYLE", styleLabelArea, juce::Justification::centredLeft, false);
        g.drawText ("SCALE", scaleLabelArea, juce::Justification::centredLeft, false);
        drawSection (acidArea, "ACID MODE  -  OFF (STOCK 303 VALUES IN USE)");
        drawSection (fxArea, "MAIN STAGE FX  -  TEMPO-SYNCED DELAY, REVERB, PUMP, FILTER LFO");
        drawSection (ghostArea, "GHOST HANDS  -  REC YOUR KNOB MOVES. LEARN MODE IMPROVISES IN YOUR STYLE. GRAB A KNOB TO TAKE IT BACK");
        g.setColour (lnf.sectionText);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("TAKE", ghostLengthLabelArea, juce::Justification::centredLeft, false);
        drawSection (energyArea, "ENERGY");
        g.setColour (lnf.sectionText);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("ARC", arcLabelArea, juce::Justification::centredLeft, false);
        drawSection (seqArea, "SEQUENCER   " + seqHint);

        g.setColour (lnf.sectionText);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("PATTERN", patternLabelArea, juce::Justification::centredLeft, false);
        return;
    }

    // Acid mode: photocopied punk flyer
    juce::Random rng (303);
    g.fillAll (lnf.panel);
    punk::drawSplatter (g, bounds, rng, 70, { punk::colours::hotPink, punk::colours::neonGreen, punk::colours::acidYellow }, 0.22f);
    punk::drawGrain (g, bounds, rng, 9000);
    punk::drawHazard (g, bounds.withHeight (7.0f), 10.0f);
    punk::drawHazard (g, bounds.withTop (bounds.getBottom() - 7.0f), 10.0f);

    const auto header = headerArea.toFloat();
    punk::drawSmiley (g, { header.getX(), header.getY() - 2.0f, 40.0f, 40.0f }, true, true);
    punk::drawRansom (g, "DEVIL ACID 303", { header.getX() + 50.0f, header.getY() + 6.0f }, 26.0f, rng);

    auto drawSection = [&] (juce::Rectangle<int> area, const juce::String& name, juce::Colour neon,
                            const juce::String& note, float tilt)
    {
        const auto r = area.toFloat();
        g.setColour (juce::Colour (0xd0141418));
        g.fillRect (r);

        // Hand-sprayed outline: a dashed neon border, slightly wobbly
        juce::Path outline;
        outline.addRectangle (r.reduced (1.5f));
        juce::Path dashed;
        const float dashes[] = { 14.0f, 5.0f, 3.0f, 5.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, outline, dashes, 4);
        g.setColour (neon.withAlpha (0.85f));
        g.fillPath (dashed);

        const float tapeWidth = 26.0f + (float) name.length() * 9.0f;
        punk::drawTape (g, { r.getX() + 10.0f, r.getY() - 5.0f, tapeWidth, 18.0f }, name, neon, tilt);

        if (note.isNotEmpty())
        {
            g.setColour (neon.withAlpha (0.75f));
            g.setFont (uiFont (11.0f, false));
            g.drawText (note, juce::Rectangle<float> (r.getX() + tapeWidth + 22.0f, r.getY() - 2.0f, r.getWidth() - tapeWidth - 30.0f, 14.0f),
                        juce::Justification::centredLeft, true);
        }
    };

    drawSection (voiceArea, "VOICE", punk::colours::acidYellow, {}, -0.03f);
    drawSection (cheatArea, "CHEAT", punk::colours::hotPink, "acid made easy", 0.03f);
    g.setColour (punk::colours::hotPink);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("STYLE", styleLabelArea, juce::Justification::centredLeft, false);
    g.drawText ("SCALE", scaleLabelArea, juce::Justification::centredLeft, false);
    drawSection (acidArea, "ACID MODE", punk::colours::neonGreen, "devil fish mods + drive", 0.025f);
    drawSection (fxArea, "MAIN STAGE FX", punk::colours::paper, "synced to the beat", -0.015f);
    drawSection (ghostArea, "GHOST HANDS", punk::colours::ghostCyan, "it learns your hands. grab a knob to take it back", 0.02f);
    g.setColour (punk::colours::ghostCyan);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("TAKE", ghostLengthLabelArea, juce::Justification::centredLeft, false);
    drawSection (energyArea, "ENERGY", punk::colours::hotPink, {}, -0.025f);
    g.setColour (punk::colours::hotPink);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("ARC", arcLabelArea, juce::Justification::centredLeft, false);
    drawSection (seqArea, "SEQUENCER", punk::colours::acidYellow, seqHint, -0.02f);

    punk::drawTape (g, patternLabelArea.toFloat().withSizeKeepingCentre (60.0f, 18.0f), "PATT", punk::colours::paper, 0.04f);
}

void AcidPanel::resized()
{
    auto r = getLocalBounds().reduced (12);

    headerArea = r.removeFromTop (40);
    {
        auto h = headerArea;
        acidButton.setBounds (h.removeFromRight (190).reduced (0, 4));
        h.removeFromRight (8);
        waveButton.setBounds (h.removeFromRight (140).reduced (0, 4));
        h.removeFromRight (8);
        presetBox.setBounds (h.removeFromRight (220).reduced (0, 6));
    }
    r.removeFromTop (8);

    auto layoutRow = [] (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> area)
    {
        const int w = area.getWidth() / (int) knobs.size();
        for (auto& k : knobs)
            k->setBounds (area.removeFromLeft (w).reduced (3, 0));
    };

    // CHEAT panel on the right, next to the three knob rows
    {
        auto upper = r.withHeight (116 + 8 + 104 + 8 + 104);
        cheatArea = upper.removeFromRight (220);
        r.removeFromRight (228);

        auto c = cheatArea.reduced (10, 6).withTrimmedTop (16);
        cheatKnobs[0]->setBounds (c.removeFromTop (110));
        auto tweakRow = c.removeFromTop (84);
        cheatKnobs[1]->setBounds (tweakRow.removeFromLeft (tweakRow.getWidth() / 2));
        cheatKnobs[2]->setBounds (tweakRow);
        c.removeFromTop (4);
        auto styleRow = c.removeFromTop (26);
        styleLabelArea = styleRow.removeFromLeft (48);
        styleBox.setBounds (styleRow);
        c.removeFromTop (6);
        auto scaleRow = c.removeFromTop (26);
        scaleLabelArea = scaleRow.removeFromLeft (48);
        scaleBox.setBounds (scaleRow);
        c.removeFromTop (8);
        generateButton.setBounds (c);
    }

    voiceArea = r.removeFromTop (116);
    layoutRow (voiceKnobs, voiceArea.reduced (10, 6).withTrimmedTop (14));
    r.removeFromTop (8);

    acidArea = r.removeFromTop (104);
    layoutRow (acidKnobs, acidArea.reduced (8, 6).withTrimmedTop (14));
    r.removeFromTop (8);

    fxArea = r.removeFromTop (104);
    layoutRow (fxKnobs, fxArea.reduced (8, 6).withTrimmedTop (14));
    r.removeFromTop (8);

    ghostArea = r.removeFromTop (100).withRight (getWidth() - 12);
    {
        auto gr = ghostArea.reduced (10, 6).withTrimmedTop (14);
        auto centred = [] (juce::Rectangle<int> area) { return area.withSizeKeepingCentre (area.getWidth(), 30); };
        recButton.setBounds (centred (gr.removeFromLeft (100)));
        gr.removeFromLeft (8);
        ghostButton.setBounds (centred (gr.removeFromLeft (112)));
        gr.removeFromLeft (8);
        ghostModeButton.setBounds (centred (gr.removeFromLeft (118)));
        gr.removeFromLeft (10);
        ghostLengthLabelArea = gr.removeFromLeft (38);
        ghostLengthBox.setBounds (centred (gr.removeFromLeft (96)).withSizeKeepingCentre (96, 26));
        gr.removeFromLeft (6);
        ghostKnobs[0]->setBounds (gr.removeFromLeft (80));
        ghostKnobs[1]->setBounds (gr.removeFromLeft (80));
        gr.removeFromLeft (6);
        {
            auto column = gr.removeFromLeft (124).withSizeKeepingCentre (124, 60);
            ghostMenu.setBounds (column.removeFromTop (27));
            column.removeFromTop (6);
            clearGhostButton.setBounds (column);
        }
        gr.removeFromLeft (12);
        ghostView.setBounds (gr.reduced (0, 2));
    }
    r.removeFromTop (8);

    energyArea = r.removeFromTop (40).withRight (getWidth() - 12);
    {
        auto er = energyArea.reduced (10, 6);
        er.removeFromLeft (104);   // section name
        energyMeter.setBounds (er.removeFromRight (300));
        er.removeFromRight (10);
        arcBox.setBounds (er.removeFromRight (230));
        arcLabelArea = er.removeFromRight (34);
        er.removeFromRight (10);
        energyFader.setBounds (er);
    }
    r.removeFromTop (8);

    seqArea = r.withRight (getWidth() - 12);
    auto s = seqArea.reduced (10, 6).withTrimmedTop (16);

    auto controls = s.removeFromTop (86);
    auto centred = [] (juce::Rectangle<int> area) { return area.withSizeKeepingCentre (area.getWidth(), 30); };
    runButton.setBounds (centred (controls.removeFromLeft (64)));
    controls.removeFromLeft (8);
    sourceBox.setBounds (centred (controls.removeFromLeft (160)));
    controls.removeFromLeft (8);
    clockBox.setBounds (centred (controls.removeFromLeft (118)));
    controls.removeFromLeft (8);
    initButton.setBounds (centred (controls.removeFromRight (70)));
    controls.removeFromRight (8);
    randomButton.setBounds (centred (controls.removeFromRight (96)));
    controls.removeFromRight (8);
    layoutRow (seqKnobs, controls);

    s.removeFromTop (4);
    auto strip = s.removeFromTop (26);
    patternLabelArea = strip.removeFromLeft (64);
    for (auto& b : patternButtons)
    {
        b.setBounds (strip.removeFromLeft (40).reduced (2, 0));
    }
    strip.removeFromLeft (12);
    copyButton.setBounds (strip.removeFromLeft (70).reduced (2, 0));
    pasteButton.setBounds (strip.removeFromLeft (70).reduced (2, 0));
    mutateButton.setBounds (strip.removeFromLeft (84).reduced (2, 0));
    strip.removeFromLeft (12);
    libraryBox.setBounds (strip.removeFromLeft (240));

    s.removeFromTop (6);
    grid.setBounds (s.removeFromTop ((int) (rowTop (5) + 1.0f)));
    s.removeFromTop (8);

    auto bottom = s;
    leds.setBounds (bottom.removeFromLeft (330).reduced (4, 2));
    bottom.removeFromLeft (8);
    scope.setBounds (bottom);
}

TB303AcidEditor::TB303AcidEditor (TB303AcidProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    addAndMakeVisible (panel);

    // Read before the limits are set: setting them resizes the empty window, which saves its width
    const int savedWidth = juce::jlimit (kDesignWidth / 2, kDesignWidth * 2,
                                         (int) proc.apvts.state.getProperty ("uiWidth", kDesignWidth));

    setResizable (true, true);
    setResizeLimits (kDesignWidth / 2, kDesignHeight / 2, kDesignWidth * 2, kDesignHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kDesignWidth / kDesignHeight);
    setSize (savedWidth, juce::roundToInt (savedWidth * (double) kDesignHeight / kDesignWidth));
}

void TB303AcidEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, kDesignWidth, kDesignHeight);
    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}
