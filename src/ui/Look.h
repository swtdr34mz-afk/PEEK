/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* The look of a Peak: a matt black steel panel, white legends, each section under a thin sky-blue bar, black rubber
 * knobs with a brushed aluminium cap and a black line, white dots around them, black rubber buttons (the menu's and the
 * ON buttons pale and lit from behind), orange pill LEDs, long black slider caps, a white OLED, walnut end cheeks. The
 * fonts are embedded (Fira Sans, Anonymous Pro: SIL OFL, fonts/). */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

namespace pkui
{
namespace col
{
const juce::Colour panel{0xff17181a}, legend{0xffeceff1}, dim{0xff8e959b}, blue{0xff3aa6e6}, led{0xffff7a1a},
    oledBg{0xff020304}, oled{0xffd4e7fb}, oledDim{0xff6a7580};
}

enum class Face { label, title, logo, mono };

struct UiResources
{
    UiResources() = default;
    ~UiResources();
};
juce::Font font(Face f, float height);
float textWidth(const juce::Font &, const juce::String &);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font &) override;
    juce::Font getPopupMenuFont() override { return font(Face::label, 15.0f); }
    void drawPopupMenuBackground(juce::Graphics &, int w, int h) override;
};

/* a knob in r (square), its body without the line (kept as a picture) and its line; big: the FREQUENCY knob */
void drawKnob(juce::Graphics &, juce::Rectangle<float> r, bool bipolar, bool endless, bool big);
void drawKnobPointer(juce::Graphics &, juce::Rectangle<float> r, float pos, bool endless);
/* a button: dark rubber, or pale (backlit: lit glows white, or red for PATCH-like ones) */
enum class BtnStyle { rubber, pale };
void drawButton(juce::Graphics &, juce::Rectangle<float> r, BtnStyle s, bool on, bool down, bool over);
/* a pill LED */
void drawLed(juce::Graphics &, juce::Point<float> c, float w, float level);
/* a slider: its slot, the scale, the cap at pos 0..1 */
void drawSliderBack(juce::Graphics &, juce::Rectangle<float> r);
void drawSliderCap(juce::Graphics &, juce::Rectangle<float> r, float pos);
/* a section: its blue bar along the top and its name under it (left), the frame's hairline */
void drawSection(juce::Graphics &, juce::Rectangle<float> r, const juce::String &title, bool arrow = false, bool zoom = false);
juce::Rectangle<float> sectionTab(juce::Rectangle<float> r, const juce::String &title);   /* its name and magnifier (a click: zoom) */
void drawLegend(juce::Graphics &, const juce::String &, juce::Rectangle<float>, float size = 12.0f,
                juce::Justification j = juce::Justification::centred);
void drawPanelFace(juce::Graphics &, juce::Rectangle<float> r, float sc);
void drawWear(juce::Graphics &, juce::Rectangle<float> r, float px, const std::vector<juce::Point<float>> &played);
void drawWood(juce::Graphics &, juce::Rectangle<float> r, int seed);
void drawTag(juce::Graphics &, juce::Rectangle<float> r);
/* the small wave symbols beside the LEDs */
enum class Sym { sine, tri, saw, square, rampDown, sh };
void drawSym(juce::Graphics &, Sym s, juce::Rectangle<float> r, float thick = 1.4f);
} // namespace pkui
