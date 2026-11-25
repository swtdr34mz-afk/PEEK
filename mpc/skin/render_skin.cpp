/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* The MPC skin's pictures, drawn with the VST3's own art (src/ui/Look.cpp: the panel, the knobs with their aluminium
 * caps, the buttons, the pill LEDs, the sections under their blue bars, the legends, the maker's tag), on the MPC's
 * six function keys, each with two pages (a second press: the other page):
 *
 *   OSC      OSCILLATOR 1, 2  /  OSCILLATOR 3, the oscillator menu (VSYNC, SAWDENSE, DENSEDET)
 *   FILTER   MIXER, FILTER    /  VOICE, GLIDE and the common oscillator page, PEEK's VOICES and QUALITY
 *   ENV      AMP ENV, MOD ENV 1  /  MOD ENV 2, ANIMATE
 *   LFO      LFO 1, LFO 2     /  LFO 3 and 4, ARP and CLOCK
 *   MOD      matrix slots 1-8 /  9-16
 *   FX       DISTORTION, CHORUS, DELAY  /  REVERB, the FX's global page
 *
 * Each page is drawn at the MPC's 1280 x 628 and cut up: its background, and every control at every value it can
 * take (a knob's film strip, a button's look for each value, an LED list's rows lit and unlit). manifest.json says
 * what each is, for make_skin.py.
 *
 *   peek_skin_render <out folder> */
#include "Params.h"
#include "ui/Look.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;
using namespace pkui;
using namespace pk;

namespace {
constexpr int TW = 1280, TH = 628;
constexpr float BW = 56.0f, BH = 28.0f;  /* a button */
constexpr float LR = 22.0f;              /* an LED row */

struct Ctl {
    enum Kind { knob, button, leds } kind;
    String id;                           /* PARAM[].id, or "voices", "quality", "anim1", "anim2", "hold" */
    float x, y;                          /* the centre (knob, button) or the top left (LEDs) */
    String legend;
    StringArray names;                   /* LEDs: each row's name */
    float w = 0.0f;                      /* LEDs: the rows' width */
    bool cycle = false;                  /* a button that steps through the values (else a switch) */
    float ks = 70.0f;                    /* a knob's size */
    bool momentary = false;
};
struct Sec {
    Rectangle<float> r;
    String title;
};
struct Page {
    String name;                         /* the function key's name */
    int key = 0, sub = 0;
    std::vector<Sec> secs;
    std::vector<Ctl> ctls;
    std::vector<std::pair<Point<float>, String>> notes;
};

int paramIndex(const String &id)
{
    for (int i = 0; i < NPARAM; i++)
        if (id == PARAM[i].id)
            return i;
    return -1;
}
struct Range {
    int lo, hi;
    StringArray names;
};
Range rangeOf(const String &id)
{
    int p = paramIndex(id);
    if (p >= 0) {
        const ParamDef &d = PARAM[p];
        Range r{d.lo, d.hi, {}};
        if (d.kind == K_CHOICE || d.kind == K_ONOFF)
            for (int k = d.lo; k <= d.hi; k++)
                r.names.add(d.choice[k - d.lo]);
        return r;
    }
    if (id == "voices")
        return {0, 1, {"8 Voice", "16 Voice"}};
    if (id == "quality")
        return {0, 1, {"ECO", "HIGH"}};
    if (id == "anim1" || id == "anim2" || id == "hold")
        return {0, 1, {"Off", "On"}};
    std::fprintf(stderr, "unknown %s\n", id.toRawUTF8());
    std::exit(1);
}
bool bipolarOf(const String &id)
{
    int p = paramIndex(id);
    return p >= 0 && PARAM[p].lo < 0;
}

/* ------------------------------------------------------------------------------- the pages --- */
const float T1 = 82, T2 = 210, B1 = 370, B2 = 498;   /* the knob rows: top band, bottom band */
struct Builder {
    Page pg;
    Builder(const char *name, int key, int sub)
    {
        pg.name = name;
        pg.key = key;
        pg.sub = sub;
    }
    void sec(float x0, float y0, float x1, float y1, const String &t) { pg.secs.push_back({{x0, y0, x1 - x0, y1 - y0}, t}); }
    void K(float x, float y, const String &id, const String &leg, float ks = 70.0f)
    {
        Ctl c{Ctl::knob, id, x, y, leg, {}, 0.0f, false};
        c.ks = ks;
        pg.ctls.push_back(c);
    }
    void B(float x, float y, const String &id, const String &leg, bool cycle = false, bool momentary = false)
    {
        Ctl c{Ctl::button, id, x, y, leg, {}, 0.0f, cycle};
        c.momentary = momentary;
        pg.ctls.push_back(c);
    }
    void L(float x, float y, const String &id, float w, StringArray names = {})
    {
        auto r = rangeOf(id);
        pg.ctls.push_back({Ctl::leds, id, x, y, "", names.isEmpty() ? r.names : names, w, false});
    }
    void note(float x, float y, const String &t) { pg.notes.push_back({{x, y}, t}); }
};

/* an oscillator in a band (y0: the band's top) */
void oscBand(Builder &b, int k, float y0)
{
    String o = "o" + String(k);
    float y = y0 + 66.0f, y2 = y0 + 194.0f;
    b.sec(14, y0, 1266, y0 + 268, "OSCILLATOR " + String(k));
    b.L(34, y0 + 36, o + "_range", 60);
    b.L(124, y0 + 36, o + "_wave", 100);
    b.K(330, y, o + "_coarse", "Coarse");
    b.K(450, y, o + "_fine", "Fine");
    b.K(570, y, o + "_env2", "Mod Env 2 Depth");
    b.K(690, y, o + "_lfo2", "LFO 2 Depth");
    b.K(330, y2, o + "_shape", "Shape Amount");
    b.K(450, y2, o + "_shenv", "Shape Mod Env 1");
    b.K(570, y2, o + "_shlfo", "Shape LFO 1");
    b.K(690, y2, o + "_more", "WaveMore");
    b.K(850, y, o + "_bend", "Bend Range");
    b.K(850, y2, o + "_fixnote", "Fixed Note");
    b.note(1080, y0 + 70, "LFO 2 and Mod Env 2:");
    b.note(1080, y0 + 92, "the pitch");
    b.note(1080, y0 + 130, "SHAPE: by hand, by Mod Env 1,");
    b.note(1080, y0 + 152, "by LFO 1 (all three add up)");
}
void envBand(Builder &b, const String &p, const String &title, float y0)
{
    float y = y0 + 66.0f, y2 = y0 + 194.0f;
    b.sec(14, y0, 1266, y0 + 268, title);
    const char *k1[4][2] = {{"_att", "Attack"}, {"_dec", "Decay"}, {"_sus", "Sustain"}, {"_rel", "Release"}};
    const char *k2[4][2] = {{"_dly", "Delay"}, {"_hold", "Hold"}, {"_vel", "Velocity"}, {"_rep", "Repeats"}};
    for (int i = 0; i < 4; i++) {
        b.K(110.0f + 170.0f * (float)i, y, p + k1[i][0], k1[i][1]);
        b.K(110.0f + 170.0f * (float)i, y2, p + k2[i][0], k2[i][1]);
    }
    b.B(860, y - 8, p + "_trig", "MonoTrig: Re-Trig");
    b.note(1080, y0 + 70, "REPEATS: Attack, Hold, Decay again;");
    b.note(1080, y0 + 92, "turned fully up: for as long as held");
}
void lfoBand(Builder &b, int k, float y0)
{
    String l = "lfo" + String(k);
    float y = y0 + 66.0f, y2 = y0 + 194.0f;
    b.sec(14, y0, 1266, y0 + 268, "LFO " + String(k));
    b.L(34, y0 + 36, l + "_type", 100);
    b.L(34, y0 + 160, l + "_range", 100);
    b.K(300, y, l + "_rate", "Rate");
    b.K(420, y, l + "_sync", "Rate Sync");
    b.K(540, y, l + "_fade", "Fade Time");
    b.K(660, y, l + "_fmode", "Fade Mode");
    b.K(300, y2, l + "_phase", "Phase");
    b.K(420, y2, l + "_slew", "Slew");
    b.K(540, y2, l + "_rep", "Repeats");
    b.K(660, y2, l + "_trig", "MonoTrig");
    b.B(820, y - 8, l + "_fsync", "Fade Sync");
    b.B(820, y2 - 8, l + "_common", "Common");
    b.note(1080, y0 + 70, String(k == 1 ? "LFO 1: the filter (LFO 1 Depth)" : "LFO 2: the pitch (LFO 2 Depth)"));
    b.note(1080, y0 + 92, String(k == 1 ? "and the oscillators' shape" : "of each oscillator"));
}

std::vector<Page> pages()
{
    std::vector<Page> ps;
    {
        Builder b("OSC", 0, 0);
        oscBand(b, 1, 16);
        oscBand(b, 2, 304);
        ps.push_back(b.pg);
    }
    {
        Builder b("OSC", 0, 1);
        oscBand(b, 3, 16);
        b.sec(14, 304, 1266, 572, "OSCILLATOR MENU");
        for (int k = 0; k < 3; k++) {
            String o = "o" + String(k + 1);
            float x = 120.0f + 400.0f * (float)k;
            b.K(x, B1, o + "_vsync", "Osc " + String(k + 1) + " VSync");
            b.K(x + 130.0f, B1, o + "_dense", "Osc " + String(k + 1) + " SawDense");
            b.K(x + 65.0f, B2, o + "_ddet", "Osc " + String(k + 1) + " DenseDet");
        }
        ps.push_back(b.pg);
    }
    {
        Builder b("FILTER", 1, 0);
        b.sec(14, 16, 1266, 284, "MIXER");
        const char *mx[6][2] = {{"lvl1", "Osc 1"}, {"lvl2", "Osc 2"}, {"lvl3", "Osc 3"}, {"lvl_noise", "Noise"}, {"lvl_ring", "Ring 1*2"}, {"vca_gain", "VCA Gain"}};
        for (int i = 0; i < 6; i++)
            b.K(110.0f + 150.0f * (float)i, T1, mx[i][0], mx[i][1]);
        b.K(110, T2, "noise_lpf", "Noise LPF");
        b.K(260, T2, "noise_hpf", "Noise HPF");
        b.K(410, T2, "postdrv", "Post Drive");
        b.K(560, T2, "fdiverge", "Filter Diverge");
        b.sec(14, 304, 1266, 572, "FILTER");
        b.K(110, B1, "freq", "Frequency", 84.0f);
        b.K(260, B1, "reso", "Resonance");
        b.K(410, B1, "drive", "Overdrive");
        b.K(560, B1, "keytrk", "Key Tracking");
        b.K(110, B2, "fenv1", "Mod Env 1 Depth");
        b.K(260, B2, "fenva", "Amp Env Depth");
        b.K(410, B2, "flfo1", "LFO 1 Depth");
        b.K(560, B2, "fosc3", "Osc 3 Filter Mod");
        b.L(720, B1 - 40, "fshape", 60);
        b.L(720, B2 - 30, "slope", 80);
        b.note(1040, B1 - 20, "OVERDRIVE before the filter: fuller");
        b.note(1040, B1 + 2, "POST DRIVE after it: brighter, grittier");
        ps.push_back(b.pg);
    }
    {
        Builder b("FILTER", 1, 1);
        b.sec(14, 16, 1266, 284, "VOICE");
        b.L(34, 50, "mode", 90);
        b.L(170, 50, "unison", 50);
        b.K(330, T1, "unidet", "Unison Detune");
        b.K(450, T1, "unispread", "Spread");
        b.K(570, T1, "spreadmode", "Spread Mode");
        b.K(690, T1, "pan", "Pan Position");
        b.K(330, T2, "patchlvl", "Patch Level");
        b.K(450, T2, "glide", "Glide Time");
        b.K(570, T2, "preglide", "PreGlide");
        b.B(690, T2 - 8, "glide_on", "Glide On");
        b.L(900, 50, "voices", 100);
        b.L(900, 130, "quality", 100, {"ECO (2x)", "HIGH (4x)"});
        b.note(1060, T2 + 10, "HIGH: the voices at 4x (more CPU)");
        b.sec(14, 304, 1266, 572, "OSCILLATOR COMMON");
        b.K(110, B1, "diverge", "Diverge");
        b.K(260, B1, "drift", "Drift");
        b.B(410, B1 - 8, "keysync", "Key Sync");
        b.note(800, B1 - 10, "DIVERGE: each voice its own tuning;");
        b.note(800, B1 + 12, "DRIFT: slowly wandering");
        ps.push_back(b.pg);
    }
    {
        Builder b("ENV", 2, 0);
        envBand(b, "aenv", "AMP ENVELOPE", 16);
        envBand(b, "m1env", "MOD ENVELOPE 1", 304);
        ps.push_back(b.pg);
    }
    {
        Builder b("ENV", 2, 1);
        envBand(b, "m2env", "MOD ENVELOPE 2", 16);
        b.sec(14, 304, 1266, 572, "ANIMATE");
        b.B(110, B1 - 8, "anim1", "Animate 1", false, true);
        b.B(260, B1 - 8, "anim2", "Animate 2", false, true);
        b.B(410, B1 - 8, "hold", "Hold");
        b.K(110, B2, "an1_att", "Animate 1 Attack");
        b.K(260, B2, "an1_rel", "Animate 1 Release");
        b.K(410, B2, "an2_att", "Animate 2 Attack");
        b.K(560, B2, "an2_rel", "Animate 2 Release");
        b.note(900, B1 - 10, "ANIMATE: sources in the matrix;");
        b.note(900, B1 + 12, "HOLD keeps them on");
        ps.push_back(b.pg);
    }
    {
        Builder b("LFO", 3, 0);
        lfoBand(b, 1, 16);
        lfoBand(b, 2, 304);
        ps.push_back(b.pg);
    }
    {
        Builder b("LFO", 3, 1);
        for (int k = 3; k <= 4; k++) {
            float x0 = k == 3 ? 14.0f : 652.0f;
            String l = "lfo" + String(k);
            b.sec(x0, 16, x0 + 614, 284, "LFO " + String(k));
            b.L(x0 + 20, 50, l + "_type", 90);
            b.K(x0 + 230, T1, l + "_rate", "Rate");
            b.K(x0 + 350, T1, l + "_sync", "Rate Sync");
            b.K(x0 + 470, T1, l + "_fade", "Fade Time");
            b.K(x0 + 230, T2, l + "_phase", "Phase");
            b.K(x0 + 350, T2, l + "_slew", "Slew");
        }
        b.sec(14, 304, 1266, 572, "ARP");
        b.B(80, B1 - 8, "arp_on", "On");
        b.B(80, B2 - 8, "arp_latch", "Key Latch");
        b.K(230, B1, "arp_type", "Type");
        b.K(350, B1, "arp_rhythm", "Rhythm");
        b.K(470, B1, "arp_oct", "Octaves");
        b.K(590, B1, "arp_rate", "Sync Rate");
        b.K(230, B2, "arp_gate", "Gate");
        b.K(350, B2, "arp_swing", "Swing");
        b.K(470, B2, "arp_chance", "Chance");
        b.B(590, B2 - 8, "arp_keysync", "Key Sync");
        b.note(1000, B1, "on the MPC's tempo");
        ps.push_back(b.pg);
    }
    for (int half = 0; half < 2; half++) {
        Builder b("MOD", 4, half);
        b.sec(14, 16, 1266, 284, "MOD MATRIX " + String(half * 8 + 1) + "-" + String(half * 8 + 4));
        b.sec(14, 304, 1266, 572, "MOD MATRIX " + String(half * 8 + 5) + "-" + String(half * 8 + 8));
        for (int k = 0; k < 8; k++) {
            int slot = half * 8 + k + 1;
            float x0 = 30.0f + 310.0f * (float)(k % 4), yA = k < 4 ? 88.0f : 376.0f, yB = yA + 126.0f;
            String m = "mod" + String(slot);
            b.K(x0 + 70.0f, yA, m + "_sa", "Source A", 60.0f);
            b.K(x0 + 180.0f, yA, m + "_sb", "Source B", 60.0f);
            b.K(x0 + 70.0f, yB, m + "_dst", "Destination", 60.0f);
            b.K(x0 + 180.0f, yB, m + "_depth", "Depth", 60.0f);
            b.note(x0 + 125.0f, yA - 40.0f, "SLOT " + String(slot));
        }
        ps.push_back(b.pg);
    }
    {
        Builder b("FX", 5, 0);
        b.sec(14, 16, 200, 284, "DISTORTION");
        b.K(107, T1, "dist", "Level");
        b.sec(212, 16, 1266, 284, "CHORUS");
        b.L(232, 50, "ch_type", 40);
        b.L(232, 150, "ch_mode", 90);
        b.K(440, T1, "ch_rate", "Rate");
        b.K(560, T1, "ch_level", "Level");
        b.K(680, T1, "ch_depth", "Depth");
        b.K(800, T1, "ch_fb", "Feedback");
        b.K(440, T2, "ch_lp", "LoPass");
        b.K(560, T2, "ch_hp", "HiPass");
        b.sec(14, 304, 1266, 572, "DELAY");
        b.K(110, B1, "dl_time", "Time");
        b.B(230, B1 - 8, "dl_sync", "Sync");
        b.K(350, B1, "dl_syncrate", "Sync Rate");
        b.K(470, B1, "dl_fb", "Feedback");
        b.K(590, B1, "dl_level", "Level");
        b.K(110, B2, "dl_lpd", "LP Damp");
        b.K(230, B2, "dl_hpd", "HP Damp");
        b.K(350, B2, "dl_ratio", "L/R Ratio");
        b.K(470, B2, "dl_slew", "Slew Rate");
        b.K(590, B2, "dl_width", "Width");
        b.K(710, B2, "dl_tmode", "Time Mode");
        ps.push_back(b.pg);
    }
    {
        Builder b("FX", 5, 1);
        b.sec(14, 16, 1266, 284, "REVERB");
        const char *rt[5][2] = {{"rv_time", "Time"}, {"rv_level", "Level"}, {"rv_size", "Size"}, {"rv_pre", "PreDelay"}, {"rv_mdepth", "Mod Depth"}};
        const char *rb[5][2] = {{"rv_lpd", "LP Damp"}, {"rv_hpd", "HP Damp"}, {"rv_lp", "LoPass"}, {"rv_hp", "HiPass"}, {"rv_mrate", "Mod Rate"}};
        for (int i = 0; i < 5; i++) {
            b.K(110.0f + 140.0f * (float)i, T1, rt[i][0], rt[i][1]);
            b.K(110.0f + 140.0f * (float)i, T2, rb[i][0], rb[i][1]);
        }
        b.sec(14, 304, 1266, 572, "EFFECTS");
        b.K(110, B1, "fx_wet", "Wet Level");
        b.K(250, B1, "fx_dry", "Dry Level");
        b.K(390, B1, "fx_route", "Routing");
        b.B(530, B1 - 8, "fx_bypass", "Bypass");
        b.note(900, B1 - 10, "BYPASS: chorus, delay and reverb out;");
        b.note(900, B1 + 12, "the distortion stays");
        ps.push_back(b.pg);
    }
    return ps;
}

/* ----------------------------------------------------------------------------- drawing --- */
void drawPage(Graphics &g, const Page &pg)
{
    auto r = Rectangle<float>(0.0f, 0.0f, (float)TW, (float)TH);
    drawPanelFace(g, r, 1.0f);
    for (auto &s : pg.secs)
        drawSection(g, s.r, s.title);
    for (auto &c : pg.ctls) {
        if (c.kind == Ctl::knob)
            drawLegend(g, c.legend, Rectangle<float>(c.x - 70.0f, c.y + c.ks * 0.5f + 3.0f, 140.0f, 20.0f), 16.0f);
        else if (c.kind == Ctl::button)
            drawLegend(g, c.legend, Rectangle<float>(c.x - 80.0f, c.y + BH * 0.5f + 6.0f, 160.0f, 20.0f), 15.0f);
        else {
            g.setColour(col::legend);
            g.setFont(font(Face::label, 15.0f));
            for (int i = 0; i < c.names.size(); i++)
                g.drawText(c.names[i], Rectangle<float>(c.x + 26.0f, c.y + LR * (float)i, c.w, LR), Justification::centredLeft, false);
        }
    }
    g.setColour(col::legend);
    g.setFont(font(Face::label, 14.0f));
    for (auto &[p, t] : pg.notes)
        g.drawText(t, Rectangle<float>(p.x - 170.0f, p.y - 9.0f, 340.0f, 18.0f), Justification::centred, false);
    /* the bottom strip: the name, the page; the maker's tag */
    g.setFont(font(Face::logo, 22.0f));
    g.drawText("PEEK", Rectangle<float>(18.0f, 586.0f, 100.0f, 30.0f), Justification::centredLeft, false);
    g.setColour(col::blue);
    g.fillRect(Rectangle<float>(18.0f, 614.0f, 62.0f, 2.0f));
    g.setColour(col::legend);
    g.setFont(font(Face::title, 16.0f));
    g.drawText(pg.name + String("   ") + String(pg.sub ? "2 / 2" : "1 / 2") + "   (press the key again: the other page)",
               Rectangle<float>(120.0f, 586.0f, 700.0f, 30.0f), Justification::centredLeft, false);
    drawTag(g, Rectangle<float>(1280.0f - 18.0f - 200.0f, 628.0f - 14.0f - 24.0f, 200.0f, 24.0f));
    drawWear(g, r, 1.0f, {});
}

Image snapBackground(const Page &pg)
{
    Image img(Image::RGB, TW, TH, true);
    Graphics g(img);
    drawPage(g, pg);
    return img;
}

bool writePng(const Image &img, const File &f)
{
    f.deleteFile();
    FileOutputStream out(f);
    PNGImageFormat png;
    return out.openedOk() && png.writeImageToStream(img, out);
}
var rectVar(Rectangle<int> r) { return Array<var>{r.getX(), r.getY(), r.getWidth(), r.getHeight()}; }

Image drawCtl(const Image &bg, const Ctl &c, Rectangle<int> r, int index, int steps, int row = -1)
{
    Image img(Image::RGB, r.getWidth(), r.getHeight(), true);
    Graphics g(img);
    g.drawImageAt(bg.getClippedImage(r), 0, 0);
    g.addTransform(AffineTransform::translation((float)-r.getX(), (float)-r.getY()));
    if (c.kind == Ctl::knob) {
        float pos = steps > 1 ? (float)index / (float)(steps - 1) : 0.0f;
        auto kr = Rectangle<float>(c.ks, c.ks).withCentre({c.x, c.y});
        drawKnob(g, kr, bipolarOf(c.id), false, c.ks > 80.0f);
        drawKnobPointer(g, kr, pos, false);
    } else if (c.kind == Ctl::button) {
        bool lit = !c.cycle && index > 0;
        drawButton(g, Rectangle<float>(BW, BH).withCentre({c.x, c.y}), BtnStyle::pale, lit, false, false);
    } else {
        drawLed(g, {c.x + 12.0f, c.y + LR * ((float)row + 0.5f)}, 18.0f, index == row ? 1.0f : 0.0f);
    }
    return img;
}
} // namespace

int main(int argc, char **argv)
{
    ScopedJuceInitialiser_GUI gui;
    SharedResourcePointer<UiResources> res;
    if (argc < 2) {
        std::fprintf(stderr, "usage: peek_skin_render <out folder>\n");
        return 2;
    }
    File dir = File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.deleteRecursively();
    dir.createDirectory();
    Array<var> pageList;
    for (auto &pg : pages()) {
        String tag = pg.name.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789") + String(pg.sub + 1);
        Image bg = snapBackground(pg);
        writePng(bg, dir.getChildFile(tag + "_panel.png"));
        Array<var> list;
        int n = 0;
        for (auto &c : pg.ctls) {
            auto rg = rangeOf(c.id);
            int steps = rg.hi - rg.lo + 1;
            auto *o = new DynamicObject();
            o->setProperty("id", c.id);
            o->setProperty("steps", steps);
            o->setProperty("band", c.y < 300.0f ? 0 : 1);
            Array<var> names;
            for (auto &t : rg.names)
                names.add(t);
            o->setProperty("names", names);
            String base = tag + "_" + String(n++).paddedLeft('0', 3) + "_" + c.id.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
            if (c.kind == Ctl::knob) {
                auto r = Rectangle<float>(c.ks + 6.0f, c.ks + 6.0f).withCentre({c.x, c.y}).toNearestInt();
                int frames = jmax(2, jmin(steps, 128, 9800 / r.getHeight()));
                Image strip(Image::RGB, r.getWidth(), r.getHeight() * frames, true);
                {
                    Graphics sg(strip);
                    for (int f = 0; f < frames; f++) {
                        int idx = frames == steps ? f : (int)std::lround((double)f * (steps - 1) / (frames - 1));
                        sg.drawImageAt(drawCtl(bg, c, r, idx, steps), 0, f * r.getHeight());
                    }
                }
                writePng(strip, dir.getChildFile(base + "_knob.png"));
                auto fr = r.getUnion(Rectangle<float>(c.x - 64.0f, c.y + c.ks * 0.5f, 128.0f, 24.0f).toNearestInt());
                o->setProperty("kind", "knob");
                o->setProperty("bipolar", bipolarOf(c.id));
                o->setProperty("rect", rectVar(r));
                o->setProperty("frame", rectVar(fr));
                o->setProperty("frames", frames);
                o->setProperty("strip", base + "_knob.png");
            } else if (c.kind == Ctl::button) {
                auto r = Rectangle<float>(BW + 6.0f, BH + 6.0f).withCentre({c.x, c.y}).toNearestInt();
                o->setProperty("kind", "button");
                o->setProperty("rect", rectVar(r));
                o->setProperty("momentary", c.momentary);
                Array<var> rs;
                for (int i = 0; i < steps; i++) {
                    auto *ro = new DynamicObject();
                    ro->setProperty("a", i);
                    ro->setProperty("b", i);
                    ro->setProperty("next", c.cycle ? (i + 1) % steps : (i ? 0 : 1));
                    String f = base + "_button_" + String(i) + ".png";
                    writePng(drawCtl(bg, c, r, i, steps), dir.getChildFile(f));
                    ro->setProperty("image", f);
                    rs.add(var(ro));
                }
                o->setProperty("ranges", rs);
            } else {
                o->setProperty("kind", "leds");
                Array<var> rows;
                for (int k = 0; k < c.names.size(); k++) {
                    auto r = Rectangle<float>(c.x, c.y + LR * (float)k, 26.0f + c.w, LR).toNearestInt();
                    auto *ro = new DynamicObject();
                    ro->setProperty("rect", rectVar(r));
                    String on = base + "_led" + String(k) + "_on.png", off = base + "_led" + String(k) + "_off.png";
                    writePng(drawCtl(bg, c, r, k, steps, k), dir.getChildFile(on));
                    writePng(drawCtl(bg, c, r, -1, steps, k), dir.getChildFile(off));
                    ro->setProperty("on", on);
                    ro->setProperty("off", off);
                    rows.add(var(ro));
                }
                o->setProperty("rows", rows);
            }
            list.add(var(o));
        }
        auto *po = new DynamicObject();
        po->setProperty("name", pg.name);
        po->setProperty("key", pg.key);
        po->setProperty("sub", pg.sub);
        po->setProperty("background", tag + "_panel.png");
        po->setProperty("controls", list);
        pageList.add(var(po));
        std::printf("%-8s %d: %d controls\n", pg.name.toRawUTF8(), pg.sub + 1, (int)pg.ctls.size());
    }
    auto *m = new DynamicObject();
    m->setProperty("pages", pageList);
    Array<var> ids;
    for (int i = 0; i < NPARAM; i++)
        ids.add(String(PARAM[i].id));
    m->setProperty("params", ids);
    dir.getChildFile("manifest.json").replaceWithText(JSON::toString(var(m)));
    std::printf("manifest.json in %s\n", dir.getFullPathName().toRawUTF8());
    return 0;
}
