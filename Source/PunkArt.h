#pragma once

// Procedural punk / acid house graphics for the editor: acid smileys, hazard tape,
// spray splatter, ransom-note lettering and sticky tape labels. Everything is drawn
// with a seeded juce::Random so the artwork is identical on every repaint.

#include <juce_gui_basics/juce_gui_basics.h>

namespace punk
{
namespace colours
{
const juce::Colour acidYellow { 0xfff7e016 };
const juce::Colour neonGreen  { 0xff39ff14 };
const juce::Colour hotPink    { 0xffff2e88 };
const juce::Colour paper      { 0xfff1ece0 };
const juce::Colour ghostCyan  { 0xff00e5ff };
const juce::Colour ink        { 0xff0b0b0d };
} // namespace colours

inline juce::Font font (const juce::String& typeface, float height, int style)
{
#if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (typeface, height, style));
#else
    return juce::Font (typeface, height, style);
#endif
}

// Classic acid smiley. xEyes = punk version; drips = melting off the bottom.
inline void drawSmiley (juce::Graphics& g, juce::Rectangle<float> r, bool xEyes, bool drips)
{
    r = r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight()), std::min (r.getWidth(), r.getHeight()));
    const float d = r.getWidth();
    const auto c = r.getCentre();
    const float line = d * 0.06f;

    if (drips)
    {
        // Drips hang from the lower half of the face, drawn first so the face covers their tops
        const float xs[] = { -0.28f, -0.05f, 0.22f };
        const float lens[] = { 0.42f, 0.62f, 0.32f };
        for (int i = 0; i < 3; ++i)
        {
            const float w = d * 0.13f;
            const float x = c.x + xs[i] * d - w * 0.5f;
            const float top = c.y + d * 0.2f;
            const float len = d * lens[i];
            juce::Path drip;
            drip.addRoundedRectangle (x, top, w, len, w * 0.5f);
            drip.addEllipse (x - w * 0.15f, top + len - w * 0.9f, w * 1.3f, w * 1.3f);
            g.setColour (colours::ink);
            g.strokePath (drip, juce::PathStrokeType (line));
            g.setColour (colours::acidYellow);
            g.fillPath (drip);
        }
    }

    g.setColour (colours::acidYellow);
    g.fillEllipse (r);
    g.setColour (colours::ink);
    g.drawEllipse (r.reduced (line * 0.5f), line);

    const float eyeY = c.y - d * 0.12f;
    const float eyeDx = d * 0.16f;
    if (xEyes)
    {
        const float s = d * 0.08f;
        for (float ex : { c.x - eyeDx, c.x + eyeDx })
        {
            g.drawLine (ex - s, eyeY - s, ex + s, eyeY + s, line);
            g.drawLine (ex - s, eyeY + s, ex + s, eyeY - s, line);
        }
    }
    else
    {
        const float w = d * 0.09f, h = d * 0.2f;
        g.fillEllipse (c.x - eyeDx - w * 0.5f, eyeY - h * 0.5f, w, h);
        g.fillEllipse (c.x + eyeDx - w * 0.5f, eyeY - h * 0.5f, w, h);
    }

    juce::Path smile;
    smile.addCentredArc (c.x, c.y + d * 0.02f, d * 0.3f, d * 0.26f, 0.0f,
                         juce::MathConstants<float>::pi * 0.62f, juce::MathConstants<float>::pi * 1.38f, true);
    g.strokePath (smile, juce::PathStrokeType (line, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Yellow/black warning stripes
inline void drawHazard (juce::Graphics& g, juce::Rectangle<float> r, float stripe)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (colours::acidYellow);
    g.fillRect (r);
    g.setColour (colours::ink);
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += stripe * 2.0f)
    {
        juce::Path p;
        p.startNewSubPath (x, r.getBottom());
        p.lineTo (x + stripe, r.getBottom());
        p.lineTo (x + stripe + r.getHeight(), r.getY());
        p.lineTo (x + r.getHeight(), r.getY());
        p.closeSubPath();
        g.fillPath (p);
    }
}

// Spray-can splatter: big blobs with satellite droplets
inline void drawSplatter (juce::Graphics& g, juce::Rectangle<float> area, juce::Random& rng, int blobs,
                          std::initializer_list<juce::Colour> palette, float alpha)
{
    const std::vector<juce::Colour> cols (palette);
    for (int i = 0; i < blobs; ++i)
    {
        const auto col = cols[(size_t) rng.nextInt ((int) cols.size())];
        const float x = area.getX() + rng.nextFloat() * area.getWidth();
        const float y = area.getY() + rng.nextFloat() * area.getHeight();
        const float r = 2.0f + std::pow (rng.nextFloat(), 3.0f) * 22.0f;

        g.setColour (col.withAlpha (alpha * (0.5f + 0.5f * rng.nextFloat())));
        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);

        const int satellites = 3 + rng.nextInt (8);
        for (int s = 0; s < satellites; ++s)
        {
            const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const float dist = r * (1.3f + rng.nextFloat() * 2.2f);
            const float sr = 0.6f + rng.nextFloat() * r * 0.18f;
            g.fillEllipse (x + std::cos (a) * dist - sr, y + std::sin (a) * dist - sr, 2.0f * sr, 2.0f * sr);
        }
    }
}

// Film grain / photocopier noise
inline void drawGrain (juce::Graphics& g, juce::Rectangle<float> area, juce::Random& rng, int dots)
{
    for (int i = 0; i < dots; ++i)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f + 0.05f * rng.nextFloat()));
        g.fillRect (area.getX() + rng.nextFloat() * area.getWidth(), area.getY() + rng.nextFloat() * area.getHeight(), 1.0f, 1.0f);
    }
}

// Ransom-note lettering: every letter cut from a different scrap, font and angle.
// Returns the width used.
inline float drawRansom (juce::Graphics& g, const juce::String& text, juce::Point<float> origin, float height,
                         juce::Random& rng)
{
    const juce::String faces[] = { juce::Font::getDefaultSansSerifFontName(),
                                   juce::Font::getDefaultSerifFontName(),
                                   juce::Font::getDefaultMonospacedFontName() };
    struct Scrap { juce::Colour back, fore; };
    const Scrap scraps[] = { { colours::paper, colours::ink },      { colours::acidYellow, colours::ink },
                             { colours::hotPink, colours::paper },  { colours::neonGreen, colours::ink },
                             { colours::ink, colours::acidYellow }, { colours::paper, colours::hotPink } };

    float x = origin.x;
    for (auto ch : text)
    {
        if (ch == ' ')
        {
            x += height * 0.35f;
            continue;
        }

        const float w = height * (0.72f + rng.nextFloat() * 0.18f);
        const float h = height * (0.88f + rng.nextFloat() * 0.18f);
        const float y = origin.y + (height - h) * rng.nextFloat();
        const auto scrap = scraps[rng.nextInt ((int) std::size (scraps))];
        const auto box = juce::Rectangle<float> (x, y, w, h);

        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation ((rng.nextFloat() - 0.5f) * 0.3f, box.getCentreX(), box.getCentreY()));

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (box.translated (1.5f, 2.0f));
        g.setColour (scrap.back);
        g.fillRect (box);

        const int style = (rng.nextBool() ? juce::Font::bold : juce::Font::plain) | (rng.nextInt (4) == 0 ? juce::Font::italic : 0);
        g.setFont (font (faces[rng.nextInt (3)], h * 0.82f, style));
        g.setColour (scrap.fore);
        g.drawText (juce::String::charToString (ch), box, juce::Justification::centred, false);

        x += w + height * 0.06f;
    }
    return x - origin.x;
}

// Strip of sticky tape with a handwritten-ish label, slightly crooked, torn ends
inline void drawTape (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, juce::Colour tape,
                      float angle)
{
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (angle, r.getCentreX(), r.getCentreY()));

    juce::Path p;
    const float tooth = r.getHeight() / 4.0f;
    p.startNewSubPath (r.getX(), r.getY());
    p.lineTo (r.getRight(), r.getY());
    for (int i = 1; i <= 4; ++i)
        p.lineTo (r.getRight() + ((i % 2) ? -tooth * 0.6f : 0.0f), r.getY() + tooth * (float) i);
    p.lineTo (r.getX(), r.getBottom());
    for (int i = 3; i >= 0; --i)
        p.lineTo (r.getX() + ((i % 2) ? tooth * 0.6f : 0.0f), r.getY() + tooth * (float) i);
    p.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (p, juce::AffineTransform::translation (1.5f, 2.0f));
    g.setColour (tape);
    g.fillPath (p);

    g.setColour (colours::ink);
    g.setFont (font (juce::Font::getDefaultMonospacedFontName(), r.getHeight() * 0.72f, juce::Font::bold));
    g.drawText (label, r.reduced (tooth, 0.0f), juce::Justification::centredLeft, true);
}
} // namespace punk
