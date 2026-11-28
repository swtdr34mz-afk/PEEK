/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "Look.h"
#include "BinaryData.h"
#include <map>

namespace pkui
{
/* the fonts and the tag: plain pointers, made when first drawn and freed when the last editor closes. Never function
 * statics: those are freed while the host unloads the DLL, under Windows' loader lock (PROPHET 67's lesson) */
struct Store
{
    juce::Typeface::Ptr t[4];
    juce::Image tag;
    std::map<int, juce::Image> coat;     /* the powder coat's tile, per pixel density */
};
static Store *store = nullptr;
static Store &get()
{
    if (store == nullptr)
    {
        store = new Store();
        store->t[0] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansSemiBold_ttf, BinaryData::FiraSansSemiBold_ttfSize);
        store->t[1] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansBold_ttf, BinaryData::FiraSansBold_ttfSize);
        store->t[2] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansExtraBold_ttf, BinaryData::FiraSansExtraBold_ttfSize);
        store->t[3] = juce::Typeface::createSystemTypefaceFor(BinaryData::AnonymousProBold_ttf, BinaryData::AnonymousProBold_ttfSize);
        store->tag = juce::ImageFileFormat::loadFrom(BinaryData::swtdr34mztag_png, BinaryData::swtdr34mztag_pngSize);
    }
    return *store;
}
UiResources::~UiResources()
{
    /* (the last editor closed: the fonts, the coat and the tag let go here, on the message thread, never when the
     * host unloads the plugin: on Windows that runs under the loader's lock, and freeing fonts there hung FL Studio) */
    delete store;
    store = nullptr;
    juce::Typeface::clearTypefaceCache();
    juce::ImageCache::releaseUnusedImages();
}

juce::Font font(Face f, float height) { return juce::Font(juce::FontOptions(get().t[(int)f]).withHeight(height)); }

float textWidth(const juce::Font &f, const juce::String &t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText(f, t, 0.0f, 0.0f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}

Look::Look()
{
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, col::panel);
    setColour(PopupMenu::backgroundColourId, Colour(0xff1b1d20));
    setColour(PopupMenu::textColourId, col::legend);
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff1f7fb8));
    setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(PopupMenu::headerTextColourId, col::dim);
    setColour(AlertWindow::backgroundColourId, Colour(0xff1b1d20));
    setColour(AlertWindow::textColourId, col::legend);
    setColour(TextEditor::backgroundColourId, Colour(0xff0b0b0c));
    setColour(TextEditor::textColourId, col::legend);
    setColour(TextButton::buttonColourId, Colour(0xff2a2b2e));
}

juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font &) { return get().t[0]; }

void Look::drawPopupMenuBackground(juce::Graphics &g, int w, int h)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(col::blue.withAlpha(0.7f));
    g.drawRect(0, 0, w, h, 1);
}

/* ------------------------------------------------------------------------------- the knob --- */
/* The Peak's knob seen from above: white dots round it, a black rubber skirt with fine ribs, and on top a brushed
 * aluminium cap (spun: its sheen in two opposite arcs), the black line cut into it */
void drawKnob(juce::Graphics &g, juce::Rectangle<float> r, bool bipolar, bool endless, bool big)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f;
    const Point<float> c = r.getCentre();
    const float a0 = -2.45f, a1 = 2.45f;
    if (!endless) {                      /* the dots */
        g.setColour(col::legend.withAlpha(0.92f));
        int n = big ? 15 : 11;
        for (int i = 0; i < n; i++) {
            float a = a0 + (a1 - a0) * (float)i / (float)(n - 1);
            float s = (bipolar && i == n / 2) ? d * 0.05f : d * 0.034f;
            g.fillEllipse(Rectangle<float>(s, s).withCentre(c.getPointOnCircumference(R * 0.95f, a)));
        }
    }
    const float rs = R * 0.80f;          /* the skirt */
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.7f), c.x, c.y + rs * 0.25f, Colours::transparentBlack, c.x,
                                     c.y + rs * 1.45f, true));
    g.fillEllipse(Rectangle<float>(rs * 2.6f, rs * 2.6f).withCentre(c.translated(rs * 0.06f, rs * 0.2f)));
    g.setColour(Colour(0xff0a0a0b));
    g.fillEllipse(Rectangle<float>(rs * 2.0f, rs * 2.0f).withCentre(c));
    const int ribs = big ? 48 : 36;
    for (int i = 0; i < ribs; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)ribs;
        float lit = 0.5f + 0.5f * std::cos(a + 2.2f);
        g.setColour(Colours::white.withAlpha(0.03f + 0.09f * lit));
        g.drawLine(Line<float>(c.getPointOnCircumference(rs * 0.84f, a), c.getPointOnCircumference(rs * 0.99f, a)),
                   jmax(0.8f, d * 0.016f));
    }
    g.setColour(Colours::white.withAlpha(0.10f));
    g.drawEllipse(Rectangle<float>(rs * 2.0f, rs * 2.0f).withCentre(c).reduced(0.5f), 0.8f);
    /* the cap */
    const float rc = rs * 0.70f;
    auto cap = Rectangle<float>(rc * 2.0f, rc * 2.0f).withCentre(c);
    g.setColour(Colour(0xff9da1a5));
    g.fillEllipse(cap);
    const int slices = 64;
    for (int i = 0; i < slices; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)slices, a2 = a + MathConstants<float>::twoPi / (float)slices + 0.01f;
        float k = 0.5f + 0.5f * std::cos(2.0f * (a + 0.7f));
        float v = 0.55f + 0.42f * std::pow(k, 2.2f);
        Path p;
        p.addPieSegment(cap, a, a2, 0.0f);
        g.setColour(Colour::fromFloatRGBA(v, v, v * 1.015f, 1.0f));
        g.fillPath(p);
    }
    Random rnd(big ? 77 : 11);
    for (int i = 0; i < 26; i++) {       /* the spun lines */
        float rr = rc * (0.15f + 0.83f * rnd.nextFloat());
        g.setColour((rnd.nextBool() ? Colours::white : Colours::black).withAlpha(0.05f + 0.05f * rnd.nextFloat()));
        g.drawEllipse(Rectangle<float>(rr * 2.0f, rr * 2.0f).withCentre(c), 0.5f);
    }
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.25f), c.x - rc * 0.6f, c.y - rc * 0.7f, Colours::transparentWhite,
                                     c.x + rc * 0.2f, c.y + rc * 0.2f, false));
    g.fillEllipse(cap);
    g.setColour(Colours::black.withAlpha(0.55f));
    g.drawEllipse(cap, 1.0f);
}

void drawKnobPointer(juce::Graphics &g, juce::Rectangle<float> r, float pos, bool endless)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), rc = d * 0.5f * 0.80f * 0.70f;
    const Point<float> c = r.getCentre();
    const float ang = endless ? pos * MathConstants<float>::twoPi : -2.45f + pos * 4.9f;
    auto p0 = c.getPointOnCircumference(rc * 0.35f, ang), p1 = c.getPointOnCircumference(rc * 1.0f, ang);
    g.setColour(Colour(0xff111111));
    g.drawLine({p0, p1}, jmax(1.6f, d * 0.05f));
}

/* ---------------------------------------------------------------------------- the button --- */
void drawButton(juce::Graphics &g, juce::Rectangle<float> r, BtnStyle s, bool on, bool down, bool over)
{
    using namespace juce;
    g.setColour(Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(r.translated(0.5f, 1.8f).expanded(0.6f), 3.0f);
    auto face = r.translated(0.0f, down ? 0.8f : 0.0f);
    if (s == BtnStyle::pale) {
        if (on) {
            g.setGradientFill(ColourGradient(Colour(0xffff4630).withAlpha(0.45f), face.getCentreX(), face.getCentreY(), Colours::transparentBlack,
                                             face.getRight() + face.getWidth() * 0.4f, face.getCentreY(), true));
            g.fillEllipse(face.expanded(face.getWidth() * 0.45f, face.getHeight() * 1.0f));
            ColourGradient lit(Colour(0xffffc0b0), face.getCentreX(), face.getCentreY(), Colour(0xffff4a36), face.getRight(), face.getBottom(), true);
            g.setGradientFill(lit);
        } else
            g.setGradientFill(ColourGradient(Colour(0xffd9dde0), face.getX(), face.getY(), Colour(0xffaab0b6), face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle(face, 3.0f);
        g.setColour(Colours::white.withAlpha(0.5f));
        g.drawLine(face.getX() + 3.0f, face.getY() + 1.0f, face.getRight() - 3.0f, face.getY() + 1.0f, 1.0f);
    } else {
        g.setGradientFill(ColourGradient(Colour(0xff2a2c2f), face.getX(), face.getY(), Colour(0xff111214), face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle(face, 3.0f);
        g.setColour(Colours::white.withAlpha(0.10f));
        g.drawLine(face.getX() + 3.0f, face.getY() + 1.0f, face.getRight() - 3.0f, face.getY() + 1.0f, 1.0f);
        g.setColour(Colours::black);
        g.drawRoundedRectangle(face, 3.0f, 1.0f);
    }
    if (over && !down) {
        g.setColour(Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(face, 3.0f);
    }
}

void drawLed(juce::Graphics &g, juce::Point<float> c, float w, float level)
{
    using namespace juce;
    level = jlimit(0.0f, 1.0f, level);
    auto body = Rectangle<float>(w, w * 0.42f).withCentre(c);
    if (level > 0.02f) {
        g.setGradientFill(ColourGradient(Colour(0xffff6a10).withAlpha(0.55f * level), c.x, c.y, Colours::transparentBlack, c.x + w * 1.1f, c.y, true));
        g.fillEllipse(Rectangle<float>(w * 2.4f, w * 1.6f).withCentre(c));
    }
    g.setColour(Colours::black.withAlpha(0.85f));
    g.fillRoundedRectangle(body.expanded(1.0f), body.getHeight() * 0.5f + 1.0f);
    g.setGradientFill(ColourGradient(Colour(0xff4a5058).interpolatedWith(Colour(0xffffd8a8), level), c.x, c.y - body.getHeight() * 0.3f,
                                     Colour(0xff262a2f).interpolatedWith(Colour(0xffff5a10), level), c.x, body.getBottom(), false));
    g.fillRoundedRectangle(body, body.getHeight() * 0.5f);
    g.setColour(Colours::white.withAlpha(0.18f + 0.3f * level));
    g.drawLine(body.getX() + 2.0f, body.getY() + 1.0f, body.getRight() - 2.0f, body.getY() + 1.0f, 0.8f);
}

/* ---------------------------------------------------------------------------- the slider --- */
void drawSliderBack(juce::Graphics &g, juce::Rectangle<float> r)
{
    using namespace juce;
    float cx = r.getCentreX();
    g.setColour(col::legend.withAlpha(0.9f));        /* the scale's lines either side */
    const int n = 11;
    float top = r.getY() + 8.0f, bot = r.getBottom() - 8.0f;
    for (int i = 0; i < n; i++) {
        float y = top + (bot - top) * (float)i / (float)(n - 1);
        float w = (i == 0 || i == n - 1) ? 15.0f : 11.0f;
        g.fillRect(Rectangle<float>(cx - 10.0f - w, y - 0.8f, w, 1.6f));
        g.fillRect(Rectangle<float>(cx + 10.0f, y - 0.8f, w, 1.6f));
    }
    auto slot = Rectangle<float>(7.0f, r.getHeight() - 4.0f).withCentre(r.getCentre());
    g.setColour(Colour(0xff050506));
    g.fillRoundedRectangle(slot.expanded(1.5f), 3.0f);
    g.setColour(Colour(0xff26292c));
    g.fillRoundedRectangle(slot.withWidth(1.2f).translated(5.0f, 0.0f), 0.5f);
}
void drawSliderCap(juce::Graphics &g, juce::Rectangle<float> r, float pos)
{
    using namespace juce;
    float top = r.getY() + 8.0f, bot = r.getBottom() - 8.0f;
    float y = bot - (bot - top) * jlimit(0.0f, 1.0f, pos);
    auto cap = Rectangle<float>(34.0f, 22.0f).withCentre({r.getCentreX(), y});
    g.setColour(Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(cap.translated(1.0f, 4.0f).expanded(1.0f), 3.0f);
    g.setGradientFill(ColourGradient(Colour(0xff3a3c40), cap.getX(), cap.getY(), Colour(0xff0b0b0c), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 3.0f);
    g.setColour(Colour(0xff050505));
    g.fillRect(cap.withHeight(3.0f).withY(cap.getCentreY() - 1.5f));
    g.setColour(Colours::white.withAlpha(0.85f));
    g.fillRect(cap.reduced(4.0f, 0.0f).withHeight(1.4f).withY(cap.getCentreY() - 0.7f));
    g.setColour(Colours::white.withAlpha(0.16f));
    g.drawLine(cap.getX() + 3.0f, cap.getY() + 1.0f, cap.getRight() - 3.0f, cap.getY() + 1.0f, 1.0f);
}

/* ------------------------------------------------------------------- sections and the face --- */
void drawLegend(juce::Graphics &g, const juce::String &t, juce::Rectangle<float> r, float size, juce::Justification j)
{
    g.setColour(col::legend);
    g.setFont(font(Face::label, size));
    g.drawFittedText(t, r.toNearestInt(), j, 2, 0.8f);
}

void drawSection(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title, bool arrow, bool zoom)
{
    using namespace juce;
    /* a slightly lighter field, a dark hairline round it */
    g.setColour(Colours::white.withAlpha(0.018f));
    g.fillRect(r);
    g.setColour(Colours::black.withAlpha(0.65f));
    g.drawRect(r, 1.4f);
    g.setColour(col::blue);
    g.fillRect(r.withHeight(4.0f));
    g.setColour(col::legend);
    g.setFont(font(Face::title, 17.0f));
    g.drawText(title, r.withTrimmedTop(6.0f).withTrimmedLeft(7.0f).withHeight(20.0f), Justification::centredLeft, false);
    if (arrow) {
        Path p;
        float x = r.getRight() - 16.0f, y = r.getY() + 9.0f;
        p.addTriangle(x, y, x, y + 12.0f, x + 10.0f, y + 6.0f);
        g.setColour(col::blue);
        g.fillPath(p);
    }
    if (zoom && title.isNotEmpty()) {   /* a small magnifier after the name: a click on either shows the section large */
        auto t = sectionTab(r, title);
        const Point<float> c(t.getRight() - 8.0f, t.getCentreY() - 1.0f);
        g.setColour(col::legend.withAlpha(0.55f));
        g.drawEllipse(Rectangle<float>(8.0f, 8.0f).withCentre(c), 1.5f);
        g.drawLine(c.x + 2.9f, c.y + 2.9f, c.x + 5.6f, c.y + 5.6f, 2.0f);
    }
}

juce::Rectangle<float> sectionTab(juce::Rectangle<float> r, const juce::String &title)
{
    using namespace juce;
    GlyphArrangement ga;
    ga.addLineOfText(font(Face::title, 17.0f), title, 0.0f, 0.0f);
    float w = ga.getBoundingBox(0, -1, true).getWidth();
    auto t = Rectangle<float>(r.getX() + 3.0f, r.getY() + 5.0f, w + 30.0f, 22.0f);
    if (t.getRight() > r.getRight() - 2.0f)   /* (a narrow section: the magnifier in its corner, under the name) */
        t = Rectangle<float>(r.getX() + 3.0f, r.getY() + 5.0f, r.getWidth() - 5.0f, 42.0f);
    return t;
}

/* ---------------------------------------------------------------------------------- the coat --- */
/* The panel's finish (SOLARIS-23's): a black textured powder coat on steel, a dense, even field of tiny round bumps,
 * each about a panel unit across and lit from the upper left (a bright crown, a dark foot), the pigment's own grain
 * between them: at arm's length a soft matte black metal, close up a fine pebbling. Made at the pixel density it is
 * shown at (an 8K screen gets its own), kept per density, tiling seamlessly. */
static juce::Image makeCoat(float sc)
{
    using namespace juce;
    const int T = 512;
    std::vector<float> h((size_t)(T * T), 0.0f);
    Random rnd(7719);
    const float spacing = jmax(1.6f, 1.05f * sc);
    const int n = (int)((float)T / spacing);
    const float cell = (float)T / (float)n;
    for (int gy = 0; gy < n; gy++)
        for (int gx = 0; gx < n; gx++) {
            float cx = ((float)gx + 0.2f + 0.6f * rnd.nextFloat()) * cell, cy = ((float)gy + 0.2f + 0.6f * rnd.nextFloat()) * cell;
            float rad = cell * (0.42f + 0.18f * rnd.nextFloat()), amp = 0.7f + 0.5f * rnd.nextFloat();
            int x0 = (int)std::floor(cx - rad * 1.8f), x1 = (int)std::ceil(cx + rad * 1.8f);
            int y0 = (int)std::floor(cy - rad * 1.8f), y1 = (int)std::ceil(cy + rad * 1.8f);
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    float dx = ((float)x - cx) / rad, dy = ((float)y - cy) / rad, e = dx * dx + dy * dy;
                    if (e < 3.2f)
                        h[(size_t)(((y % T + T) % T) * T + ((x % T + T) % T))] += amp * std::exp(-e);
                }
        }
    Image im(Image::ARGB, T, T, true);
    Image::BitmapData bd(im, Image::BitmapData::writeOnly);
    const float LX = -0.55f, LY = -0.68f, LZ = 0.48f, ln = std::sqrt(LX * LX + LY * LY + LZ * LZ);
    const float lx = LX / ln, ly = LY / ln, lz = LZ / ln, k = 0.9f;
    const float soft = jlimit(0.55f, 1.0f, sc * 0.5f);   /* (at one pixel a unit the bumps are pixels: quieter) */
    for (int y = 0; y < T; y++)
        for (int x = 0; x < T; x++) {
            float dx = h[(size_t)(y * T + (x + 1) % T)] - h[(size_t)(y * T + (x + T - 1) % T)];
            float dy = h[(size_t)(((y + 1) % T) * T + x)] - h[(size_t)(((y + T - 1) % T) * T + x)];
            float nx = -dx * k, ny = -dy * k, nl = std::sqrt(nx * nx + ny * ny + 1.0f);
            float v = (nx * lx + ny * ly + lz) / nl - lz + (rnd.nextFloat() - 0.5f) * 0.05f;
            Colour c = v > 0.0f ? Colour(0xffe8e6e0).withAlpha(soft * jmin(0.08f, v * 0.22f)) : Colours::black.withAlpha(soft * jmin(0.45f, -v * 0.8f));
            bd.setPixelColour(x, y, c);
        }
    return im;
}

void drawPanelFace(juce::Graphics &g, juce::Rectangle<float> r, float sc)
{
    using namespace juce;
    /* the steel's black, a little warmer and cooler in large soft patches (the coat thicker and thinner) */
    g.setColour(Colour(0xff141416));
    g.fillRect(r);
    Random rnd(1923);
    for (int i = 0; i < (int)(r.getWidth() * r.getHeight() / 30000.0f); i++) {
        float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
        float s = 40.0f + rnd.nextFloat() * 180.0f;
        Colour c = rnd.nextFloat() < 0.5f ? Colour(0xff26262a) : Colour(0xff0a0a0b);
        ColourGradient cg(c.withAlpha(0.08f), x, y, c.withAlpha(0.0f), x + s, y, true);
        g.setGradientFill(cg);
        g.fillEllipse(x - s, y - s, s * 2.0f, s * 2.0f);
    }
    /* the coat, at this density */
    {
        auto &cache = get().coat;
        int key = jlimit(4, 32, (int)std::lround(sc * 4.0f));
        auto it = cache.find(key);
        if (it == cache.end())
            it = cache.emplace(key, makeCoat((float)key / 4.0f)).first;
        Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(r.toNearestInt());
        float k = 4.0f / (float)key;
        g.setFillType(FillType(it->second, AffineTransform::scale(k).translated(r.getX(), r.getY())));
        g.fillRect(r);
    }
    /* the light across the sheet: brighter at the upper left, a soft sheen across the middle */
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.035f), r.getX(), r.getY(), Colours::black.withAlpha(0.16f), r.getRight(),
                                     r.getBottom(), false));
    g.fillRect(r);
    ColourGradient sheen(Colours::white.withAlpha(0.0f), r.getX(), r.getY() + r.getHeight() * 0.1f, Colours::white.withAlpha(0.0f),
                         r.getRight(), r.getBottom(), false);
    sheen.addColour(0.42, Colours::white.withAlpha(0.025f));
    g.setGradientFill(sheen);
    g.fillRect(r);
}

/* A few years of use, over the print: gentle. */
/* A few years of use, over the print: the coat rubbed glossy round the knobs played most, light scuffs, fine
 * scratches (most along the hand's way across the panel), a few chips at the edges, the print worn thin in pin-holes.
 * Gentle: a cared-for instrument, not a relic. px: one screen pixel in panel units (hairlines a pixel wide). Fixed
 * seeds: the same panel every time. */
void drawWear(juce::Graphics &g, juce::Rectangle<float> r, float px, const std::vector<juce::Point<float>> &played)
{
    using namespace juce;
    Random rnd(4242);
    auto P = [&](float mx, float my) { return Point<float>(r.getX() + mx * r.getWidth(), r.getY() + my * r.getHeight()); };
    /* round the knobs played most: the coat polished in a ring, fine circular scratches from the fingers */
    for (auto c : played) {
        float rr = 30.0f + rnd.nextFloat() * 8.0f;
        ColourGradient cg(Colours::white.withAlpha(0.0f), c.x, c.y, Colours::white.withAlpha(0.0f), c.x + rr * 1.6f, c.y, true);
        cg.addColour(0.62, Colours::white.withAlpha(0.035f + 0.02f * rnd.nextFloat()));
        g.setGradientFill(cg);
        g.fillEllipse(Rectangle<float>(rr * 3.2f, rr * 3.2f).withCentre(c));
        for (int k = 0; k < 26; k++) {
            float a0 = rnd.nextFloat() * 6.283f, len = 0.3f + rnd.nextFloat() * 1.1f, rad = rr * (0.85f + rnd.nextFloat() * 0.6f);
            Path p;
            p.addCentredArc(c.x, c.y, rad, rad, 0.0f, a0, a0 + len, true);
            g.setColour(Colour(0xffcfccc4).withAlpha(0.05f + 0.07f * rnd.nextFloat()));
            g.strokePath(p, PathStrokeType(px));
        }
    }
    /* light scuffs */
    for (int i = 0; i < 9; i++) {
        auto c = P(rnd.nextFloat(), rnd.nextFloat());
        float w = 30.0f + rnd.nextFloat() * 120.0f, h = w * (0.2f + rnd.nextFloat() * 0.3f), ang = rnd.nextFloat() * 3.14f;
        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::rotation(ang, c.x, c.y));
        float a = 0.025f + 0.04f * rnd.nextFloat();
        g.setGradientFill(ColourGradient(Colour(0xff8a8780).withAlpha(a), c.x, c.y, Colour(0xff8a8780).withAlpha(0.0f), c.x + w * 0.5f, c.y, true));
        g.fillEllipse(c.x - w * 0.5f, c.y - h * 0.5f, w, h);
        for (int k = 0; k < 10; k++) {
            float yy = c.y + (rnd.nextFloat() - 0.5f) * h * 0.8f, xx = c.x + (rnd.nextFloat() - 0.5f) * w * 0.6f;
            g.setColour(Colour(0xff9a968c).withAlpha(a * 0.9f));
            g.drawLine(xx - w * 0.2f, yy, xx + w * 0.2f, yy, px);
        }
    }
    /* fine scratches: a groove's shadow and its lit lip */
    for (int i = 0; i < 260; i++) {
        auto a = P(rnd.nextFloat(), rnd.nextFloat());
        float len = rnd.nextFloat() < 0.85f ? 4.0f + rnd.nextFloat() * 30.0f : 30.0f + rnd.nextFloat() * 120.0f;
        float ang = rnd.nextFloat() < 0.6f ? -0.3f + rnd.nextFloat() * 0.6f : rnd.nextFloat() * 3.14f;
        auto b = a + Point<float>(std::cos(ang), std::sin(ang)) * len;
        Path p;
        p.startNewSubPath(a);
        p.quadraticTo((a + b) * 0.5f + Point<float>(rnd.nextFloat() - 0.5f, rnd.nextFloat() - 0.5f) * len * 0.15f, b);
        float w = px * (1.0f + rnd.nextFloat() * 0.6f);
        PathStrokeType st(w, PathStrokeType::curved, PathStrokeType::rounded);
        g.setColour(Colours::black.withAlpha(0.25f));
        g.strokePath(p, st, AffineTransform::translation(w * 0.7f, w * 0.7f));
        g.setColour(Colour(0xffc4c1b8).withAlpha(len > 50.0f ? 0.035f + 0.04f * rnd.nextFloat() : 0.05f + 0.1f * rnd.nextFloat()));
        g.strokePath(p, st);
    }
    /* a few chips at the edges, bare steel with a dark lip */
    for (int i = 0; i < 26; i++) {
        float t = rnd.nextFloat();
        int side = rnd.nextInt(2);
        Point<float> c = side == 0 ? P(t, 0.0f) : P(t, 1.0f);
        c.y += (side == 0 ? 1.0f : -1.0f) * rnd.nextFloat() * 3.0f;
        Path chip;
        int n = 7 + rnd.nextInt(4);
        float s = 0.8f + rnd.nextFloat() * rnd.nextFloat() * 3.5f;
        for (int k = 0; k < n; k++) {
            float a = (float)k / (float)n * 6.283f;
            auto q = c + Point<float>(std::cos(a), std::sin(a)) * s * (0.45f + rnd.nextFloat() * 0.7f);
            if (k)
                chip.lineTo(q);
            else
                chip.startNewSubPath(q);
        }
        chip.closeSubPath();
        g.setColour(Colours::black.withAlpha(0.5f));
        g.strokePath(chip, PathStrokeType(px * 1.4f));
        g.setGradientFill(ColourGradient(Colour(0xffc8c9c4), c.x - s, c.y - s, Colour(0xff626360), c.x + s, c.y + s, false));
        g.fillPath(chip);
    }
    /* the print worn: pin-holes of the coat over it */
    const int n = (int)(r.getWidth() * r.getHeight() * 0.0025f / jmax(0.25f, px));
    for (int i = 0; i < n; i++) {
        auto c = P(rnd.nextFloat(), rnd.nextFloat());
        float s = px * (0.8f + rnd.nextFloat() * 1.4f);
        g.setColour(Colour(0xff141416).withAlpha(0.5f + 0.5f * rnd.nextFloat()));
        g.fillEllipse(c.x, c.y, s, s * (0.6f + rnd.nextFloat() * 0.8f));
    }
}

void drawWood(juce::Graphics &g, juce::Rectangle<float> r, int seed)
{
    using namespace juce;
    ColourGradient base(Colour(0xff4a2412), r.getX(), 0.0f, Colour(0xff3c1d0f), r.getRight(), 0.0f, false);
    base.addColour(0.35, Colour(0xff7a3e1f));
    base.addColour(0.7, Colour(0xff653319));
    g.setGradientFill(base);
    g.fillRect(r);
    Random rnd(seed);
    for (int i = 0; i < 34; i++) {
        float x0 = r.getX() + rnd.nextFloat() * r.getWidth(), amp = 1.0f + rnd.nextFloat() * 3.5f,
              f = 0.004f + rnd.nextFloat() * 0.01f, ph = rnd.nextFloat() * 6.28f;
        Path p;
        p.startNewSubPath(x0, r.getY());
        for (float y = r.getY(); y <= r.getBottom(); y += 12.0f)
            p.lineTo(x0 + amp * std::sin(y * f + ph) + 1.2f * std::sin(y * f * 3.7f + ph * 2.0f), y);
        bool dark = rnd.nextFloat() < 0.7f;
        g.setColour(dark ? Colour(0xff200d05).withAlpha(0.16f + 0.2f * rnd.nextFloat())
                         : Colour(0xffa86a40).withAlpha(0.10f + 0.12f * rnd.nextFloat()));
        g.strokePath(p, PathStrokeType(0.5f + rnd.nextFloat() * 1.6f));
    }
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(0.10f), r.getX(), 0.0f, Colours::transparentWhite, r.getX() + r.getWidth() * 0.3f, 0.0f, false));
    g.fillRect(r);
}

void drawTag(juce::Graphics &g, juce::Rectangle<float> r)
{
    const juce::Image &tag = get().tag;
    if (tag.isNull())
        return;
    auto fit = juce::RectanglePlacement(juce::RectanglePlacement::centred).appliedTo(tag.getBounds().toFloat(), r);
    g.setColour(col::legend);
    g.drawImage(tag, fit, juce::RectanglePlacement::stretchToFit, true);
}

void drawSym(juce::Graphics &g, Sym s, juce::Rectangle<float> r, float thick)
{
    using namespace juce;
    Path p;
    float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom(), ym = r.getCentreY(), w = r.getWidth();
    switch (s) {
    case Sym::sine:
        p.startNewSubPath(x0, ym);
        for (int i = 1; i <= 16; i++)
            p.lineTo(x0 + w * (float)i / 16.0f, ym - (y1 - y0) * 0.5f * std::sin((float)i / 16.0f * 6.2832f));
        break;
    case Sym::tri:
        p.startNewSubPath(x0, y1);
        p.lineTo(x0 + w * 0.5f, y0);
        p.lineTo(x1, y1);
        break;
    case Sym::saw:
        p.startNewSubPath(x0, y1);
        p.lineTo(x1, y0);
        p.lineTo(x1, y1);
        break;
    case Sym::rampDown:
        p.startNewSubPath(x0, y1);
        p.lineTo(x0, y0);
        p.lineTo(x1, y1);
        break;
    case Sym::square:
        p.startNewSubPath(x0, y1);
        p.lineTo(x0, y0);
        p.lineTo(x0 + w * 0.5f, y0);
        p.lineTo(x0 + w * 0.5f, y1);
        p.lineTo(x1, y1);
        p.lineTo(x1, y0);
        break;
    case Sym::sh:
        p.startNewSubPath(x0, ym);
        p.lineTo(x0 + w * 0.25f, ym);
        p.lineTo(x0 + w * 0.25f, y0);
        p.lineTo(x0 + w * 0.5f, y0);
        p.lineTo(x0 + w * 0.5f, y1);
        p.lineTo(x0 + w * 0.75f, y1);
        p.lineTo(x0 + w * 0.75f, ym);
        p.lineTo(x1, ym);
        break;
    }
    g.setColour(col::legend);
    g.strokePath(p, PathStrokeType(thick, PathStrokeType::mitered, PathStrokeType::square));
}

} // namespace pkui
