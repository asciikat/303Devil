#include "PluginEditor.h"
#include "PunkArt.h"
#include "PunkFishAssets.h"
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
    knobArt[0] = juce::ImageCache::getFromMemory (PunkFishAssets::knob_lime_png, PunkFishAssets::knob_lime_pngSize);
    knobArt[1] = juce::ImageCache::getFromMemory (PunkFishAssets::knob_pink_png, PunkFishAssets::knob_pink_pngSize);
    knobArt[2] = juce::ImageCache::getFromMemory (PunkFishAssets::knob_black_png, PunkFishAssets::knob_black_pngSize);
    setAcid (true);
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
        panel       = Colour (0xffdc8a28);
        section     = Colour (0xffe79c3a);
        sectionText = Colour (0xff312016);
        title       = Colour (0xff241a15);
        tick        = Colour (0xff3c281b).withAlpha (0.8f);
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

    const auto valueText = Colour (0xffc2ee74);

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

void AcidLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                        float start, float end, juce::Slider& slider)
{
    const float size = (float) juce::jmin (width, height) * 1.24f;
    const auto art = juce::Rectangle<float> (size, size).withCentre ({ (float) x + (float) width * 0.5f,
                                                                    (float) y + (float) height * 0.5f });
    const auto id = slider.getName();
    const int type = id == pid::resonance || id == pid::distortion ? 1
                   : id == pid::cutoff || id == pid::envMod || id == pid::accent || id == pid::squelch ? 0 : 2;
    const auto light = type == 1 ? punk::colours::hotPink : punk::colours::neonGreen;
    const float angle = start + pos * (end - start);
    const auto base = juce::Point<float> (art.getCentreX(), art.getY() + size * 0.58f);
    juce::Path ring, active;
    ring.addCentredArc (base.x, base.y, size * 0.37f, size * 0.25f, 0.0f, start, end, true);
    active.addCentredArc (base.x, base.y, size * 0.37f, size * 0.25f, 0.0f, start, angle, true);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (art.getX() + size * 0.15f, base.y - size * 0.21f, size * 0.76f, size * 0.48f);
    g.strokePath (ring, juce::PathStrokeType (2.0f));
    g.setColour (light.withAlpha (0.12f));
    g.strokePath (active, juce::PathStrokeType (7.0f));
    g.setColour (light.withAlpha (slider.isEnabled() ? 0.9f : 0.25f));
    g.strokePath (active, juce::PathStrokeType (2.0f));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (knobArt[(size_t) type], art);

    // Projection measured by the asset-rendering camera, independent of window size.
    const auto cap = juce::Point<float> (art.getCentreX(), art.getY() + size * 0.40572f);
    auto point = [cap, size] (float proportion, float a)
    {
        return juce::Point<float> (cap.x + std::sin (a) * size * 0.31228f * proportion,
                                   cap.y - std::cos (a) * size * 0.21469f * proportion);
    };
    g.setColour (type == 2 ? juce::Colour (0xffdbe3cf) : juce::Colour (0xff202725));
    g.drawLine ({ point (0.38f, angle), point (0.88f, angle) }, juce::jmax (1.3f, size * 0.022f));
    if (slider.isMouseOverOrDragging())
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (art.getX() + size * 0.18f, cap.y - size * 0.22f, size * 0.64f, size * 0.44f, 0.8f);
    }
    const auto& ghostValue = slider.getProperties()["ghost"];
    if (! ghostValue.isVoid() && (double) ghostValue >= 0.0)
    {
        const float ghostAngle = start + (float) slider.valueToProportionOfLength ((double) ghostValue) * (end - start);
        g.setColour (ghost.withAlpha (0.85f));
        g.drawLine ({ point (0.3f, ghostAngle), point (0.95f, ghostAngle) }, 1.8f);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (point (0.95f, ghostAngle)));
    }
}

void AcidLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isMouseOver, bool isButtonDown)
{
    auto base = button.findColour (button.getToggleState() ? juce::TextButton::buttonOnColourId
                                                           : juce::TextButton::buttonColourId);
    if (isMouseOver && button.isEnabled())
        base = base.brighter (0.12f);
    if (! button.isEnabled())
        base = base.withMultipliedAlpha (0.4f);
    punk::drawKeycap (g, button.getLocalBounds().toFloat().reduced (0.5f), base,
                      isButtonDown, button.getToggleState());
    if (button.hasKeyboardFocus (true))
    {
        g.setColour (ghost);
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (2.0f), 3.0f, 1.3f);
    }
}

void AcidLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down,
                                    int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (r, 3.0f);
    const auto face = r.reduced (1.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff16191d), face.getTopLeft(),
                                            juce::Colour (0xff30343a), face.getBottomRight(), false));
    g.fillRoundedRectangle (face, 2.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ghost : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    const float cx = (float) width - 15.0f, cy = (float) height * 0.5f;
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.3f));
    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy + (down ? 2.0f : -2.0f));
    arrow.lineTo (cx, cy + (down ? -2.0f : 2.0f));
    arrow.lineTo (cx + 4.0f, cy + (down ? 2.0f : -2.0f));
    g.strokePath (arrow, juce::PathStrokeType (1.7f));
}

void AcidLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float minPos, float maxPos,
                                        juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, pos, minPos, maxPos, style, slider);
        return;
    }
    const float cy = (float) y + (float) height * 0.5f;
    const auto rail = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) width, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (rail.expanded (1.0f), 3.0f);
    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (rail.withWidth (juce::jlimit (0.0f, (float) width, pos - (float) x)), 2.0f);
    for (int i = 0; i <= 10; ++i)
    {
        const float tx = (float) x + (float) i * (float) width / 10.0f;
        g.setColour (title.withAlpha (0.4f));
        g.drawVerticalLine (juce::roundToInt (tx), cy + 6.0f, cy + 8.0f);
    }
    auto cap = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ pos, cy });
    punk::drawKeycap (g, cap, juce::Colour (0xff666970), slider.isMouseButtonDown(), false);
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillRect (cap.reduced (7.5f, 3.0f));
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
    slider.setName (paramId);
    slider.setTooltip (knobTitle + ": drag to adjust, double-click to reset; click the value to type.");
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 15);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (auto* param = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto legend = juce::Rectangle<float> (juce::jmin (r.getWidth() - 4.0f, 116.0f), 16.0f)
                            .withCentre ({ r.getCentreX(), 8.0f });
    g.setColour (juce::Colour (0xff27251f));
    g.fillRoundedRectangle (legend, 2.0f);
    g.setColour (juce::Colour (0xffefe6d5));
    g.setFont (uiFont (11.5f, true));
    g.drawText (title, legend, juce::Justification::centred, false);
    const auto display = juce::Rectangle<float> (76.0f, 16.0f).withCentre ({ r.getCentreX(), r.getBottom() - 7.5f });
    g.setColour (juce::Colour (0xff171c16));
    g.fillRoundedRectangle (display, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (display, 2.0f, 1.0f);
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
        g.setColour (lnf.cellText.withAlpha (alpha * (i % 4 == 0 ? 1.0f : 0.65f)));
        g.setFont (uiFont (12.0f, i % 4 == 0));
        g.drawText (juce::String (i + 1), numberCell, juce::Justification::centred, false);

        // Note
        const auto noteCell = cellBounds (i, noteRow);
        punk::drawKeycap (g, noteCell, lnf.cellOff.withAlpha (alpha), false, false);
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
            punk::drawKeycap (g, cell, (on ? onColour : lnf.cellOff).withAlpha (alpha), false, on);
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
    const float peak = proc.uiOutputPeak.load (std::memory_order_relaxed);
    g.setColour (juce::Colour (0xee101610));
    const auto readout = juce::Rectangle<float> (112.0f, 15.0f).withRightX (r.getRight() - 3.0f).withY (3.0f);
    g.fillRect (readout);
    g.setColour (peak >= 1.0f ? punk::colours::hotPink : punk::colours::neonGreen);
    g.setFont (uiFont (10.0f, true));
    g.drawText (peak > 0.000001f ? juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS"
                               : juce::String ("-inf dBFS"), readout, juce::Justification::centredRight, false);
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

    auto area = getLocalBounds().toFloat().withTrimmedBottom (15.0f);
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
    metal = juce::ImageCache::getFromMemory (PunkFishAssets::orange_metal_png, PunkFishAssets::orange_metal_pngSize);
    serviceButton.setName ("service");
    serviceButton.setClickingTogglesState (true);
    serviceButton.onClick = [this] { serviceOpen = serviceButton.getToggleState(); updateServiceView(); resized(); repaint(); };
    addAndMakeVisible (serviceButton);
    throwButton.setName ("throw"); fillButton.setName ("fill");
    holdButton.setName ("hold"); returnButton.setName ("return");
    throwButton.onStateChange = [this] { proc.setThrowHeld (throwButton.isDown()); };
    fillButton.onStateChange = [this] { proc.setFillHeld (fillButton.isDown()); };
    holdButton.onClick = [this] { proc.holdPerformance(); syncControls(); };
    returnButton.onClick = [this] { proc.returnPerformance(); syncControls(); };
    throwButton.setTooltip ("Hold to feed a filtered tempo-synced delay. Release keeps the echo tail.");
    fillButton.setTooltip ("Hold to add accents and slides to played steps. Stored patterns remain intact.");
    holdButton.setTooltip ("Capture current sound, Energy and Ghost settings as a return point.");
    returnButton.setTooltip ("Restore the captured settings and release Throw/Fill. Host automation can override them.");
    serviceButton.setTooltip ("Show deeper voice, clock and effects settings without losing the sequence.");
    for (auto* button : { &throwButton, &fillButton, &holdButton, &returnButton })
    {
        button->setColour (juce::TextButton::buttonColourId, punk::colours::hotPink.darker (0.12f));
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        addAndMakeVisible (*button);
    }
    squelchFader.setSliderStyle (juce::Slider::LinearHorizontal);
    squelchFader.setName (pid::squelch);
    squelchFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    squelchFader.setDoubleClickReturnValue (true, 0.0);
    squelchFader.setTooltip ("Squelch: perform the filter, envelope, resonance and drive together.");
    addAndMakeVisible (squelchFader);
    squelchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, pid::squelch, squelchFader);

    addKnob (voiceKnobs, pid::tune,      "TUNING");
    addKnob (voiceKnobs, pid::cutoff,    "CUT");
    addKnob (voiceKnobs, pid::resonance, "RESONANCE");
    addKnob (voiceKnobs, pid::envMod,    "ENV");
    addKnob (voiceKnobs, pid::decay,     "DECAY");
    addKnob (voiceKnobs, pid::accent,    "ACCENT");
    addKnob (voiceKnobs, pid::volume,    "VOLUME");

    addKnob (acidKnobs, pid::overdrive,   "DRIVE");
    addKnob (acidKnobs, pid::filterFM,    "FILTER FM");
    addKnob (acidKnobs, pid::tracking,    "TRACKING");
    addKnob (acidKnobs, pid::sweep,       "SWEEP SPD");
    addKnob (acidKnobs, pid::slideTime,   "SLIDE");
    addKnob (acidKnobs, pid::ampDecay,    "AMP DECAY");
    addKnob (acidKnobs, pid::accentDecay, "ACC DECAY");
    addKnob (acidKnobs, pid::softAttack,  "ATTACK");
    addKnob (acidKnobs, pid::muffler,     "MUFFLER");
    addKnob (acidKnobs, pid::distortion,  "BITE");
    addKnob (acidKnobs, pid::sub,         "BODY");

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
    proc.setThrowHeld (false);
    proc.setFillHeld (false);
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
    lnf.setAcid (true); // orange physical skin is independent of the sound circuit mode
    squelchFader.setColour (juce::Slider::trackColourId, punk::colours::neonGreen.darker (0.3f));
    squelchFader.setColour (juce::Slider::thumbColourId, punk::colours::neonGreen);
    energyFader.setColour (juce::Slider::trackColourId, shownAcid ? juce::Colour (0xffff2e88) : lnf.accentOn);
    energyFader.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff26262c));
    energyFader.setColour (juce::Slider::thumbColourId, shownAcid ? juce::Colour (0xfff2d10e) : juce::Colour (0xfff4f4f4));
    acidButton.setButtonText (shownAcid ? "ACID MODE: ON" : "ACID MODE: OFF");
    for (auto& k : acidKnobs)
        k->setAlpha (shownAcid ? 1.0f : 0.5f);
    sendLookAndFeelChange();
    repaint();
    if (auto* chassis = getParentComponent())
        chassis->repaint();
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
    repaint (1044, 92, 126, 66);
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
    returnButton.setEnabled (proc.hasPerformanceHold());
    holdButton.setToggleState (proc.hasPerformanceHold(), juce::dontSendNotification);
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
    punk::drawFish (g, { 1044.0f, 94.0f, 126.0f, 63.0f }, punk::colours::hotPink,
                    proc.uiAccentSweep.load() * 2.0f);
}

void AcidPanel::renderBackground (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.drawImage (metal, bounds);
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillRect (bounds);
    auto plate = [&] (juce::Rectangle<int> box, const juce::String& name, juce::Colour tape, bool recessed)
    {
        const auto r = box.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f, 2.0f);
        if (recessed)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff111515), r.getTopLeft(),
                                                    juce::Colour (0xff292b28), r.getBottomRight(), false));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 0.8f);
        punk::drawTape (g, { r.getX() + 12.0f, r.getY() + 5.0f,
                            juce::jmin (190.0f, 30.0f + (float) name.length() * 11.0f), 19.0f }, name, tape, -0.015f);
        punk::drawScrew (g, { r.getRight() - 9.0f, r.getY() + 10.0f });
        punk::drawScrew (g, { r.getRight() - 9.0f, r.getBottom() - 9.0f });
    };
    g.setColour (juce::Colour (0xff1e211d));
    g.setFont (uiFont (39.0f, true).italicised());
    g.drawText ("PUNK FISH", juce::Rectangle<int> { 22, 9, 285, 43 }, juce::Justification::centredLeft, false);
    punk::drawTape (g, { 295.0f, 22.0f, 120.0f, 20.0f }, "ACID TECHNO", punk::colours::neonGreen, -0.035f);
    plate (voiceArea, serviceOpen ? "SERVICE" : "VOICE", punk::colours::acidYellow, false);
    plate (ghostArea, "GHOST HANDS", punk::colours::neonGreen, true);
    plate (acidArea, "CHEMISTRY", punk::colours::acidYellow, false);
    plate (energyArea, "RAVE", punk::colours::hotPink, false);
    plate (seqArea, "SEQUENCE", punk::colours::neonGreen, true);
    plate (fxArea, "SPACE", punk::colours::hotPink, false);
    if (! serviceOpen)
    {
        g.setColour (juce::Colour (0xff241e18));
        g.setFont (uiFont (10.0f, true));
        g.drawText ("PLAY THE FILTER. RIDE THE ACCENT.", juce::Rectangle<int> { 350, 74, 330, 18 }, juce::Justification::centredRight, false);
    }
    g.setColour (juce::Colour (0xff201d18));
    g.setFont (uiFont (12.0f, true));
    g.drawText ("SQUELCH", juce::Rectangle<int> { 42, 431, 220, 16 }, juce::Justification::centredLeft, false);
    g.drawText ("ENERGY", juce::Rectangle<int> { 318, 431, 220, 16 }, juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xffb0b8a2));
    g.setFont (uiFont (10.0f, true));
    g.drawText ("PATTERN", juce::Rectangle<int> { 158, 506, 200, 12 }, juce::Justification::centredLeft, false);
    g.drawText ("SOURCE / CLOCK", juce::Rectangle<int> { 490, 718, 280, 13 }, juce::Justification::centredLeft, false);
    g.drawText ("STYLE", juce::Rectangle<int> { 830, 723, 95, 13 }, juce::Justification::centredLeft, false);
    g.drawText ("SCALE", juce::Rectangle<int> { 938, 723, 95, 13 }, juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff25251e));
    g.drawText ("ENERGY ARC", juce::Rectangle<int> { 448, 823, 200, 14 }, juce::Justification::centredLeft, false);
    g.drawText ("LIVE OUTPUT", juce::Rectangle<int> { 675, 820, 200, 15 }, juce::Justification::centredLeft, false);
    // Original stencil mark: a fish spine, teeth and fins printed onto the casing.
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::translation (365.0f, 872.0f));
    g.setColour (juce::Colour (0xff24221d));
    g.drawLine (-24.0f, 0.0f, 24.0f, 0.0f, 3.0f);
    juce::Path jaw;
    jaw.startNewSubPath (10.0f, -16.0f); jaw.lineTo (30.0f, -6.0f);
    jaw.lineTo (18.0f, 0.0f); jaw.lineTo (30.0f, 8.0f); jaw.lineTo (10.0f, 17.0f);
    g.strokePath (jaw, juce::PathStrokeType (3.0f));
    for (int i = 0; i < 5; ++i)
    {
        const float x = -18.0f + (float) i * 6.0f;
        g.drawLine (x, -13.0f + (float) i, x + 4.0f, 0.0f, 2.0f);
        g.drawLine (x + 4.0f, 0.0f, x, 13.0f - (float) i, 2.0f);
    }
    g.drawLine (-24.0f, 0.0f, -33.0f, -12.0f, 3.0f);
    g.drawLine (-24.0f, 0.0f, -33.0f, 12.0f, 3.0f);
    g.drawLine (14.0f, -11.0f, 19.0f, -6.0f, 2.0f);
    g.drawLine (19.0f, -11.0f, 14.0f, -6.0f, 2.0f);
}

void AcidPanel::updateServiceView()
{
    for (auto& knob : voiceKnobs) knob->setVisible (false);
    for (auto& knob : acidKnobs) knob->setVisible (false);
    for (auto& knob : fxKnobs) knob->setVisible (false);
    for (auto& knob : cheatKnobs) knob->setVisible (false);
    for (int i : { 0, 8, 9, 10 }) acidKnobs[(size_t) i]->setVisible (true);
    for (int i : { 0, 3, 5 }) fxKnobs[(size_t) i]->setVisible (true);
    voiceKnobs[6]->setVisible (true);
    if (! serviceOpen)
    {
        for (int i = 1; i <= 5; ++i) voiceKnobs[(size_t) i]->setVisible (true);
        acidKnobs[4]->setVisible (true);
    }
    else
    {
        voiceKnobs[0]->setVisible (true);
        for (int i : { 1, 2, 3, 5, 6, 7 }) acidKnobs[(size_t) i]->setVisible (true);
        for (int i : { 1, 2, 4, 6, 7 }) fxKnobs[(size_t) i]->setVisible (true);
        cheatKnobs[1]->setVisible (true);
        cheatKnobs[2]->setVisible (true);
    }
    background = {};
}

void AcidPanel::resized()
{
    headerArea = { 16, 8, 1168, 46 };
    voiceArea = { 16, 68, 690, 330 };
    ghostArea = { 720, 68, 464, 210 };
    acidArea = { 720, 290, 464, 108 };
    energyArea = { 16, 410, 1168, 84 };
    seqArea = { 16, 506, 1168, 292 };
    fxArea = { 16, 810, 1168, 122 };
    presetBox.setBounds (430, 17, 238, 31);
    waveButton.setBounds (680, 15, 110, 35);
    acidButton.setBounds (800, 15, 140, 35);
    serviceButton.setBounds (950, 15, 108, 35);
    runButton.setBounds (1070, 15, 104, 35);
    updateServiceView();
    if (! serviceOpen)
    {
        std::array<Knob*, 6> primary { voiceKnobs[1].get(), voiceKnobs[2].get(), voiceKnobs[3].get(),
                                      voiceKnobs[4].get(), voiceKnobs[5].get(), acidKnobs[4].get() };
        for (int i = 0; i < 6; ++i)
            primary[(size_t) i]->setBounds (30 + (i % 3) * 220, 104 + (i / 3) * 144, 214, 139);
    }
    else
    {
        std::array<Knob*, 14> secondary { voiceKnobs[0].get(), acidKnobs[1].get(), acidKnobs[2].get(),
            acidKnobs[3].get(), acidKnobs[5].get(), acidKnobs[6].get(), acidKnobs[7].get(),
            fxKnobs[1].get(), fxKnobs[2].get(), fxKnobs[4].get(), fxKnobs[6].get(), fxKnobs[7].get(),
            cheatKnobs[1].get(), cheatKnobs[2].get() };
        for (int i = 0; i < 14; ++i)
            secondary[(size_t) i]->setBounds (28 + (i % 7) * 96, 112 + (i / 7) * 136, 92, 120);
    }
    ghostView.setBounds (734, 94, 305, 63);
    recButton.setBounds (734, 166, 94, 29);
    ghostButton.setBounds (836, 166, 105, 29);
    ghostModeButton.setBounds (949, 166, 122, 29);
    clearGhostButton.setBounds (1079, 166, 90, 29);
    ghostLengthLabelArea = { 734, 207, 37, 25 };
    ghostLengthBox.setBounds (774, 207, 91, 27);
    ghostMenu.setBounds (875, 207, 123, 27);
    ghostKnobs[0]->setBounds (1007, 202, 80, 69);
    ghostKnobs[1]->setBounds (1091, 202, 80, 69);
    const int chem[] = { 0, 10, 9, 8 };
    for (int i = 0; i < 4; ++i)
        acidKnobs[(size_t) chem[i]]->setBounds (734 + i * 109, 316, 103, 76);
    squelchFader.setBounds (40, 449, 246, 31);
    energyFader.setBounds (316, 449, 246, 31);
    int bx = 596;
    for (auto* button : { &throwButton, &fillButton, &holdButton, &returnButton })
    {
        button->setBounds (bx, 437, 132, 42);
        bx += 143;
    }
    for (int i = 0; i < acid::kNumPatterns; ++i)
        patternButtons[(size_t) i].setBounds (150 + i * 40, 523, 36, 27);
    copyButton.setBounds (486, 523, 65, 27);
    pasteButton.setBounds (557, 523, 65, 27);
    mutateButton.setBounds (628, 523, 80, 27);
    randomButton.setBounds (714, 523, 89, 27);
    initButton.setBounds (809, 523, 65, 27);
    libraryBox.setBounds (884, 523, 286, 27);
    grid.setBounds (34, 558, 1134, 157);
    for (int i = 0; i < 5; ++i)
        seqKnobs[(size_t) i]->setBounds (32 + i * 87, 724, 82, 67);
    sourceBox.setBounds (490, 738, 174, 28);
    clockBox.setBounds (674, 738, 141, 28);
    styleLabelArea = { 830, 722, 95, 14 };
    scaleLabelArea = { 938, 722, 95, 14 };
    styleBox.setBounds (830, 740, 98, 26);
    scaleBox.setBounds (938, 740, 98, 26);
    generateButton.setBounds (1048, 735, 120, 38);
    fxKnobs[0]->setBounds (32, 839, 92, 86);
    fxKnobs[3]->setBounds (137, 839, 92, 86);
    fxKnobs[5]->setBounds (242, 839, 92, 86);
    voiceKnobs[6]->setBounds (1070, 839, 96, 86);
    arcBox.setBounds (448, 844, 201, 27);
    energyMeter.setBounds (448, 881, 201, 27);
    scope.setBounds (675, 841, 375, 54);
    leds.setBounds (681, 899, 366, 31);
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

void TB303AcidEditor::paint (juce::Graphics& g)
{
    const float scale = (float) getWidth() / (float) kDesignWidth;
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto shell = juce::Colour (0xffa95620);
    g.fillAll (juce::Colour (0xff171719));
    // Visible side cheeks and front fascia sit beneath the inset playing surface.
    const auto chassis = juce::Rectangle<float> (4.0f, 4.0f, 1192.0f, 948.0f);
    g.setGradientFill (juce::ColourGradient (shell.brighter (0.2f), chassis.getTopLeft(),
                                            shell.darker (0.45f), chassis.getBottomRight(), false));
    g.fillRoundedRectangle (chassis, 9.0f);
    const auto fascia = juce::Rectangle<float> (22.0f, 925.0f, 1156.0f, 25.0f);
    g.setGradientFill (juce::ColourGradient (shell, fascia.getTopLeft(), shell.darker (0.45f),
                                            fascia.getBottomLeft(), false));
    g.fillRect (fascia);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawHorizontalLine (925, 22.0f, 1178.0f);
    g.setColour (juce::Colour (0xffe9d7b8));
    g.setFont (punk::font (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold));
    g.drawText ("PUNK FISH / ACID HOUSE & TECHNO", fascia, juce::Justification::centred, false);
    punk::drawScrew (g, { 13.0f, 35.0f });
    punk::drawScrew (g, { 1187.0f, 35.0f });
    punk::drawScrew (g, { 13.0f, 928.0f });
    punk::drawScrew (g, { 1187.0f, 928.0f });
}

void TB303AcidEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignWidth;
    // Component transforms also transform mouse coordinates, so the inset surface
    // and every control keep their real hit areas at all supported window sizes.
    panel.setTransform (juce::AffineTransform::scale (scale * 0.96f)
                            .translated (24.0f * scale, 6.0f * scale));
    panel.setBounds (0, 0, kDesignWidth, kDesignHeight);
    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}
