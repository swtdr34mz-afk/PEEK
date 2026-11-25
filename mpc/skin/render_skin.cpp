/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* The MPC skin's pictures: four pages, each a part of the VST3's own panel (PeekPanel, src/PluginEditor.cpp) drawn
 * as large as the MPC's screen takes it, as on the Peak:
 *
 *   OSC     OSCILLATOR 1 to 3 and the MIXER
 *   FILTER  the FILTER, DISTORTION, CHORUS, DELAY, EFFECTS, REVERB
 *   ENV     ARP, LFO 1 and 2, GLIDE, the AMP and MOD ENVELOPES
 *   SCREEN  MASTER, ANIMATE, PATCH and the MENU: its menu buttons, the display with its three row buttons, PAGE,
 *           VALUE and the data knob, working as on the Peak (the display's pages are the VST's own menu pages)
 *
 * The background is the VST's panel with its controls taken off; every control is cut out at every value it can take
 * (a knob's film strip, a button's look, each LED lit and unlit). Where the VST shows one of two parameters on one
 * knob (SHAPE AMOUNT and its source, the filter's ENV DEPTH, the LFOs' RATE, the delay's TIME, the MOD ENVELOPES'
 * sliders) the MPC has one control for each, shown while the other parameter says so. manifest.json says what each
 * is, with its MPC handles, for make_skin.py.
 *
 *   peek_skin_render <out folder> [--preview] */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/PixelFont.h"

using namespace juce;
using namespace pkui;
using namespace pk;

namespace {
constexpr int TW = 1280, TH = 628;

/* the MPC plugin's parameters (mpc/plugin/src/PeekMpc.cpp): 0 VOLUME, 1.. the patch's, then PEEK's own */
int mpcIndex(const String &id)
{
    static const char *G[] = {"voices", "quality", "anim1", "anim2", "hold", "target", "vdec", "vinc", "init",
                              "audition", "prog", "pdec", "pinc", "rvtype"};
    if (id == "volume")
        return 0;
    for (int i = 0; i < NPARAM; i++)
        if (id == PARAM[i].id)
            return 1 + i;
    for (int i = 0; i < (int)(sizeof G / sizeof G[0]); i++)
        if (id == G[i])
            return 1 + NPARAM + i;
    std::fprintf(stderr, "unknown parameter %s\n", id.toRawUTF8());
    std::exit(1);
}
String H(const String &id) { return "Parameter " + String(mpcIndex(id)); }
int paramIndex(const String &id)
{
    for (int i = 0; i < NPARAM; i++)
        if (id == PARAM[i].id)
            return i;
    return -1;
}
constexpr int NTARGET = NPARAM + 3;      /* the display's row picked: a patch parameter, VOICES, QUALITY, the patch */
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
    if (id == "volume")
        return {0, 127, {}};
    if (id == "voices")
        return {0, 1, {"8 Voice", "16 Voice"}};
    if (id == "quality")
        return {0, 1, {"ECO", "HIGH"}};
    if (id == "rvtype")
        return {0, 2, {"1", "2", "3"}};
    if (id == "prog") {
        Range r{0, NFACTORY - 1, {}};
        for (int i = 0; i < NFACTORY; i++)
            r.names.add(FACTORY[i].name);
        return r;
    }
    if (id == "target")
        return {0, NTARGET - 1, {}};
    return {0, 1, {"Off", "On"}};        /* (the switches and momentary keys) */
}
int targetOf(const String &id)
{
    int p = paramIndex(id);
    return p >= 0 ? p : id == "voices" ? NPARAM : id == "quality" ? NPARAM + 1 : NPARAM + 2;
}

/* a condition for a control to be shown: handle h at a step in a .. b of its steps */
struct Cond {
    String h;
    int a, b, steps;
};
Cond when(const String &id, int a, int b = -1)
{
    auto r = rangeOf(id);
    return {H(id), a - r.lo, (b < 0 ? a : b) - r.lo, r.hi - r.lo + 1};
}

struct Ctl {
    enum Kind { knob, fader, button, leds, image, label } kind;
    String id;                           /* the parameter ("" for a local handle: see local) */
    Rectangle<float> r;                  /* on the page */
    /* knobs */
    bool big = false, endless = false;
    /* buttons: how a press changes the value (cycle: the next; toggle; momentary: on while held; set: to `set`) */
    enum Mode { cycle, toggle, momentary, set, still } mode = toggle;
    BtnStyle style = BtnStyle::rubber;
    std::vector<int> next;               /* cycle: the value after each (empty: + 1) */
    int setTo = 0;
    String local;                        /* a UI-only handle ("Local/n/screen") instead of a parameter */
    int localSteps = 0;
    bool litAlways = false, litNever = false;
    String text;                         /* (ANIMATE 1 and 2: their number) */
    /* LEDs: each LED's centre and value; their width */
    std::vector<std::pair<Point<float>, int>> pts;
    float lw = 0.0f;
    /* images (shown only): the picture */
    Image pic;
    /* labels: the value's text */
    float textH = 0.0f;
    Justification just = Justification::centredRight;
    std::vector<Cond> show;
    int q = -1;                          /* its Q-Link: 0..15, the top row first */
};
struct Page {
    String name;
    int key = 0;
    Rectangle<float> region;             /* the panel's part, in its pixels */
    float s = 1.0f;
    Point<float> off;
    std::vector<Ctl> ctls;
    std::vector<std::function<void(Graphics &)>> extra;   /* drawn on the background */
};

float PX(float px) { return 38.0f + (px - 45.0f) * 1.797f; }
float PY(float py) { return (py - 95.0f) * 1.833f; }

/* ------------------------------------------------------------------------------- the pages --- */
struct Builder {
    Page pg;
    Builder(const char *name, int key, float x0, float y0, float x1, float y1)
    {
        pg.name = name;
        pg.key = key;
        pg.region = {PX(x0), PY(y0), PX(x1) - PX(x0), PY(y1) - PY(y0)};
        const float aw = 1256.0f, ah = 566.0f;
        pg.s = jmin(aw / pg.region.getWidth(), ah / pg.region.getHeight());
        float w = pg.region.getWidth() * pg.s, h = pg.region.getHeight() * pg.s;
        pg.off = {12.0f + (aw - w) * 0.5f, 10.0f + (ah - h) * 0.5f};
    }
    Point<float> at(float px, float py) const
    {
        return {pg.off.x + (PX(px) - pg.region.getX()) * pg.s, pg.off.y + (PY(py) - pg.region.getY()) * pg.s};
    }
    float sz(float v) const { return v * pg.s; }
    Rectangle<float> box(float px, float py, float w, float h) const { return Rectangle<float>(sz(w), sz(h)).withCentre(at(px, py)); }
    Ctl &add(Ctl c)
    {
        pg.ctls.push_back(std::move(c));
        return pg.ctls.back();
    }
    Ctl &K(float px, float py, const String &id, std::vector<Cond> show = {}, float d = 64.0f, bool big = false)
    {
        Ctl c{Ctl::knob, id, box(px, py, d, d)};
        c.big = big;
        c.show = std::move(show);
        return add(c);
    }
    Ctl &F(float pxc, float pyc, const String &id, std::vector<Cond> show = {})
    {
        Ctl c{Ctl::fader, id, Rectangle<float>(sz(60.0f), (PY(289) - PY(217)) * pg.s).withCentre(at(pxc, pyc))};
        c.show = std::move(show);
        return add(c);
    }
    Ctl &B(float px, float py, const String &id, Ctl::Mode m, BtnStyle st = BtnStyle::rubber, float w = 52.0f, float h = 26.0f)
    {
        Ctl c{Ctl::button, id, box(px, py, w, h)};
        c.mode = m;
        c.style = st;
        return add(c);
    }
    Ctl &L(const String &id, std::vector<std::pair<Point<float>, int>> pts, float w = 21.0f)
    {
        Ctl c{Ctl::leds, id, {}};
        for (auto &[p, v] : pts)
            c.pts.push_back({at(p.x, p.y), v});
        c.lw = sz(w);
        return add(c);
    }
    void Q(std::initializer_list<std::initializer_list<const char *>> rows)
    {
        int row = 0;
        for (auto &r : rows) {
            int col = 0;
            for (auto *id : r) {
                for (auto &c : pg.ctls)
                    if (c.id == id && c.kind != Ctl::label && c.kind != Ctl::image && c.q < 0) {
                        c.q = row * 4 + col;
                        break;
                    }
                col++;
            }
            row++;
        }
    }
};

Page oscPage()
{
    Builder b("OSC", 0, 48, 306, 570, 545);
    for (int k = 0; k < 3; k++) {
        String o = "o" + String(k + 1);
        float oy = 80.0f * (float)k;
        b.L(o + "_range", {{{64, 330 + oy}, 0}, {{64, 342 + oy}, 1}, {{90, 330 + oy}, 2}, {{90, 342 + oy}, 3}});
        b.B(78, 366 + oy, o + "_range", Ctl::cycle);
        b.K(125, 343 + oy, o + "_coarse");
        b.K(181, 343 + oy, o + "_fine");
        b.L(o + "_wave", {{{216, 326 + oy}, W_SINE}, {{241, 326 + oy}, W_SQUARE}, {{216, 338 + oy}, W_TRI},
                          {{241, 338 + oy}, W_MORE}, {{216, 350 + oy}, W_SAW}});
        auto &w = b.B(230, 366 + oy, o + "_wave", Ctl::cycle);
        w.next = {W_TRI, W_SAW, W_SQUARE, W_MORE, W_SINE};   /* (as the VST: sine, triangle, saw, square, more) */
        b.K(277, 343 + oy, o + "_env2");
        b.K(333, 343 + oy, o + "_lfo2");
        b.K(389, 343 + oy, o + "_shape", {when(o + "_shsrc", SS_MANUAL)});
        b.K(389, 343 + oy, o + "_shenv", {when(o + "_shsrc", SS_ENV1)});
        b.K(389, 343 + oy, o + "_shlfo", {when(o + "_shsrc", SS_LFO1)});
        b.L(o + "_shsrc", {{{421, 326 + oy}, 0}, {{421, 338 + oy}, 1}, {{421, 350 + oy}, 2}});
        b.B(428, 366 + oy, o + "_shsrc", Ctl::cycle);
    }
    b.K(489, 343, "lvl1");
    b.K(544, 343, "vca_gain");
    b.K(489, 425, "lvl2");
    b.K(544, 425, "lvl_ring");
    b.K(489, 506, "lvl3");
    b.K(544, 506, "lvl_noise");
    b.Q({{"o1_coarse", "o1_fine", "o1_env2", "o1_lfo2"}, {"o2_coarse", "o2_fine", "o2_env2", "o2_lfo2"},
         {"o3_coarse", "o3_fine", "o3_env2", "o3_lfo2"}, {"lvl1", "lvl2", "lvl3", "lvl_noise"}});
    return b.pg;
}

Page filterPage()
{
    Builder b("FILTER", 1, 572, 306, 957, 545);
    b.K(598, 338, "flfo1");
    b.K(654, 338, "fosc3");
    b.K(709, 338, "keytrk");
    b.K(598, 414, "fenva", {when("fenv_src", 0)});
    b.K(598, 414, "fenv1", {when("fenv_src", 1)});
    b.K(654, 414, "reso");
    b.K(709, 414, "drive");
    b.L("fenv_src", {{{586, 449}, 0}, {{586, 460}, 1}});
    b.B(598, 477, "fenv_src", Ctl::cycle);
    b.L("slope", {{{586, 503}, 0}, {{586, 514}, 1}});
    b.B(598, 526, "slope", Ctl::cycle);
    b.K(654, 500, "freq", {}, 86.0f, true);
    b.L("fshape", {{{693, 490}, 0}, {{693, 502}, 1}, {{693, 514}, 2}});
    b.B(711, 526, "fshape", Ctl::cycle);
    b.K(763, 343, "dist");
    b.K(826, 343, "ch_rate");
    b.L("ch_type", {{{858, 326}, 0}, {{858, 338}, 1}, {{858, 350}, 2}});
    b.B(872, 368, "ch_type", Ctl::cycle);
    b.K(930, 343, "ch_level");
    b.K(766, 422, "dl_fb");
    b.K(836, 422, "dl_time", {when("dl_sync", 0)});
    b.K(836, 422, "dl_syncrate", {when("dl_sync", 1)});
    b.B(882, 412, "dl_sync", Ctl::toggle, BtnStyle::pale, 56, 26);
    b.K(930, 422, "dl_level");
    b.B(763, 500, "fx_bypass", Ctl::toggle, BtnStyle::pale, 56, 26);
    b.K(826, 500, "rv_time");
    b.L("rvtype", {{{858, 486}, 0}, {{858, 498}, 1}, {{858, 510}, 2}});
    b.B(872, 528, "rvtype", Ctl::cycle);
    b.K(930, 500, "rv_level");
    b.Q({{"freq", "reso", "drive", "keytrk"}, {"flfo1", "fosc3", "fenv1", "fenva"},
         {"dist", "ch_rate", "ch_level", "dl_fb"}, {"dl_time", "dl_level", "rv_time", "rv_level"}});
    return b.pg;
}

Page envPage()
{
    Builder b("ENV", 2, 462, 114, 957, 303);
    b.K(487, 165, "arp_gate");
    b.B(487, 228, "arp_latch", Ctl::toggle, BtnStyle::pale, 56, 26);
    b.B(487, 265, "arp_on", Ctl::toggle, BtnStyle::pale, 56, 26);
    for (int k = 0; k < 2; k++) {
        String l = "lfo" + String(k + 1);
        float ox = k ? 222.0f : 0.0f;
        b.L(l + "_type", {{{527 + ox, 130}, 0}, {{527 + ox, 142}, 1}, {{527 + ox, 154}, 2}, {{527 + ox, 166}, 3}});
        b.B(538 + ox, 181, l + "_type", Ctl::cycle);
        b.K(592 + ox, 158, l + "_fade");
        b.L(l + "_range", {{{636 + ox, 135}, 0}, {{636 + ox, 147}, 1}, {{636 + ox, 159}, 2}});
        b.B(650 + ox, 181, l + "_range", Ctl::cycle);
        b.K(706 + ox, 158, l + "_rate", {when(l + "_range", LR_LOW, LR_HIGH)});
        b.K(706 + ox, 158, l + "_sync", {when(l + "_range", LR_SYNC)});
    }
    b.K(540, 236, "glide");
    b.B(540, 282, "glide_on", Ctl::toggle, BtnStyle::pale, 56, 26);
    const char *E[4] = {"_att", "_dec", "_sus", "_rel"};
    for (int i = 0; i < 4; i++)
        b.F(592.4f + 40.7f * (float)i, 253.0f, String("aenv") + E[i]);
    for (int i = 0; i < 4; i++) {
        b.F(808.0f + 42.0f * (float)i, 253.0f, String("m1env") + E[i], {when("menv_sel", 0)});
        b.F(808.0f + 42.0f * (float)i, 253.0f, String("m2env") + E[i], {when("menv_sel", 1)});
    }
    b.L("menv_sel", {{{757, 238}, 0}, {{757, 250}, 1}});
    b.B(766, 276, "menv_sel", Ctl::cycle);
    b.Q({{"aenv_att", "aenv_dec", "aenv_sus", "aenv_rel"}, {"m1env_att", "m1env_dec", "m1env_sus", "m1env_rel"},
         {"lfo1_fade", "lfo1_rate", "lfo2_fade", "lfo2_rate"}, {"arp_gate", "glide", "lfo1_sync", "lfo2_sync"}});
    return b.pg;
}

/* ------------------------------------------------------------------- the display's pages --- */
struct Row {
    String label;
    StringArray ids;                     /* its parameter (two: a matrix slot's sources A and B) */
};
struct Screen {
    int menu;
    String title;
    int index, count;                    /* (its page in its menu) */
    std::vector<Row> rows;
};
std::vector<Screen> screens(PeekPanel &panel)
{
    std::vector<Screen> out;
    for (int m = 0; m < PeekPanel::NMENU; m++) {
        if (m == PeekPanel::M_PATCH) {
            out.push_back({m, "PATCH", 0, 1, {{"Patch", {"prog"}}, {"Voices", {"voices"}}, {"Quality", {"quality"}}}});
            continue;
        }
        auto ps = panel.pages((PeekPanel::Menu)m);
        int n = 0;
        for (auto &pg : ps) {
            Screen s{m, pg.title, n++, (int)ps.size(), {}};
            for (size_t i = 0; i < pg.rows.size() && i < 3; i++) {
                auto &e = pg.rows[i];
                Row r{e.label, {}};
                if (pg.mod && i == 0) {
                    r.ids = {String(PARAM[pg.modBase + MOD_SA].id), String(PARAM[pg.modBase + MOD_SB].id)};
                    r.label = "";
                } else if (e.param >= 0)
                    r.ids.add(PARAM[e.param].id);
                else if (e.label == "Voices")
                    r.ids.add("voices");
                else if (e.label == "Quality")
                    r.ids.add("quality");
                s.rows.push_back(r);     /* (a row the MPC cannot have, HOST SYNC, stays empty) */
            }
            out.push_back(s);
        }
    }
    return out;
}

/* the OLED's dot-matrix font (as the VST's display) */
float pixWidth(const String &t, float d, bool bold) { return t.isEmpty() ? 0.0f : ((float)t.length() * (bold ? 7.0f : 6.0f) - 1.0f) * d; }
void pix(Graphics &g, const String &t, float x, float y, float d, bool bold)
{
    RectangleList<float> dots;
    const float s = d * 0.86f;
    for (auto p = t.getCharPointer(); !p.isEmpty();) {
        juce_wchar c = p.getAndAdvance();
        int gi = (c < 32 || c > 127) ? '?' - 32 : (int)c - 32;
        for (int row = 0; row < 9; row++) {
            unsigned bits = PIXFONT[gi][row];
            if (bold)
                bits = (bits << 1) | bits;
            for (int col = 0; col < (bold ? 6 : 5); col++)
                if (bits & (1u << ((bold ? 5 : 4) - col)))
                    dots.addWithoutMerging({x + (float)col * d, y + (float)row * d, s, s});
        }
        x += (bold ? 7.0f : 6.0f) * d;
    }
    g.setColour(col::oled.withAlpha(0.16f));
    for (auto &q : dots)
        g.fillRect(q.expanded(d * 0.22f));
    g.setColour(col::oled);
    g.fillRectList(dots);
}

Page screenPage(PeekPanel &panel, std::vector<Screen> &scr)
{
    Builder b("SCREEN", 3, 48, 114, 460, 303);
    const int NS = (int)scr.size();
    const String LS = "Local/" + String(NS) + "/screen";
    auto onScreen = [&](int k) { return Cond{LS, k, k, NS}; };
    /* MASTER, ANIMATE */
    b.K(80, 150, "volume");
    b.B(79, 230, "anim1", Ctl::momentary, BtnStyle::pale, 44, 40).text = "1";
    b.B(79, 256, "anim2", Ctl::momentary, BtnStyle::pale, 44, 40).text = "2";
    b.B(79, 282, "hold", Ctl::toggle, BtnStyle::rubber, 40, 16);
    /* PATCH: INITIALISE, AUDITION; the patch knob, - and +; (COMPARE and SAVE: the MPC keeps its programs itself) */
    b.B(131.5f, 141, "init", Ctl::momentary);
    b.B(131.5f, 171, "audition", Ctl::momentary);
    b.K(149, 222, "prog");
    b.B(131.5f, 270, "pdec", Ctl::momentary);
    b.B(166.5f, 270, "pinc", Ctl::momentary);
    /* MENU: a menu button each, lit while one of its pages is shown */
    std::vector<Point<float>> mb = {{207, 160}};
    for (int i = 0; i < 8; i++)
        mb.push_back({262.0f + 38.0f * (float)(i % 4), i < 4 ? 133.0f : 158.0f});
    const int MS[9] = {PeekPanel::M_PATCH, PeekPanel::M_OSC, PeekPanel::M_ENV, PeekPanel::M_LFO, PeekPanel::M_ARP,
                       PeekPanel::M_MOD, PeekPanel::M_VOICE, PeekPanel::M_FX, PeekPanel::M_SETTINGS};
    for (int i = 0; i < 9; i++) {
        int first = -1, last = -1;
        for (int k = 0; k < NS; k++)
            if (scr[(size_t)k].menu == MS[i]) {
                if (first < 0)
                    first = k;
                last = k;
            }
        if (first < 0)
            continue;
        for (int part = 0; part < 3; part++) {   /* lit within its pages; unlit before and after them */
            int a = part == 0 ? first : part == 1 ? 0 : last + 1, z = part == 0 ? last : part == 1 ? first - 1 : NS - 1;
            if (a > z)
                continue;
            auto &c = b.B(mb[(size_t)i].x, mb[(size_t)i].y, "", Ctl::set, BtnStyle::pale, 56, 26);
            c.local = LS;
            c.localSteps = NS;
            c.setTo = first;
            c.litAlways = part == 0;
            c.litNever = part != 0;
            c.show = {Cond{LS, a, z, NS}};
        }
    }
    /* the display: each page's picture (its title, its rows' names) and its rows' values (the MPC's own text) */
    auto oled = Rectangle<float>(PX(226), PY(196), PX(372) - PX(226), PY(260) - PY(196));
    auto o = Rectangle<float>(b.pg.off.x + (oled.getX() - b.pg.region.getX()) * b.pg.s,
                              b.pg.off.y + (oled.getY() - b.pg.region.getY()) * b.pg.s, oled.getWidth() * b.pg.s,
                              oled.getHeight() * b.pg.s);
    auto in = o.reduced(8.0f * b.pg.s, 6.0f * b.pg.s);
    const float d = in.getHeight() / 44.0f, lh = 11.0f * d;
    for (int k = 0; k < NS; k++) {
        auto &s = scr[(size_t)k];
        Image img(Image::RGB, (int)std::ceil(o.getWidth()), (int)std::ceil(o.getHeight()), true);
        {
            Graphics g(img);
            g.fillAll(Colours::black);
            g.addTransform(AffineTransform::translation(-o.getX(), -o.getY()));
            String pn = String(s.index + 1) + "/" + String(s.count);
            pix(g, s.title.toUpperCase().substring(0, 13), in.getX(), in.getY() + d, d, true);
            pix(g, pn, in.getRight() - pixWidth(pn, d, false), in.getY() + d, d, false);
            for (size_t i = 0; i < s.rows.size(); i++) {
                auto &r = s.rows[i];
                float y = in.getY() + lh * (float)(i + 1) + d;
                if (r.ids.size() == 2) {
                    pix(g, "A:", in.getX(), y, d, false);
                    pix(g, "B:", in.getCentreX() + 4.0f * d, y, d, false);
                } else
                    pix(g, r.label, in.getX(), y, d, false);
            }
        }
        Ctl c{Ctl::image, "", o};
        c.pic = img;
        c.show = {onScreen(k)};
        b.add(c);
        for (size_t i = 0; i < s.rows.size(); i++) {
            auto &r = s.rows[i];
            auto lr = Rectangle<float>(in.getX(), in.getY() + lh * (float)(i + 1), in.getWidth(), lh);
            for (int j = 0; j < r.ids.size(); j++) {
                Ctl v{Ctl::label, r.ids[j], r.ids.size() == 2 ? (j == 0 ? lr.withTrimmedLeft(14.0f * d).withWidth(lr.getWidth() * 0.5f - 16.0f * d)
                                                                         : lr.withTrimmedLeft(lr.getWidth() * 0.5f + 16.0f * d))
                                                             : lr.withTrimmedLeft(lr.getWidth() * 0.45f)};
                v.textH = lh * 0.82f;
                v.just = r.ids.size() == 2 ? Justification::centredLeft : Justification::centredRight;
                v.show = {onScreen(k)};
                b.add(v);
            }
        }
    }
    /* the row buttons: each picks its row (the parameter the data knob and VALUE change) */
    for (int k = 0; k < NS; k++)
        for (size_t i = 0; i < scr[(size_t)k].rows.size(); i++) {
            auto &r = scr[(size_t)k].rows[i];
            if (r.ids.isEmpty())
                continue;
            auto &c = b.B(206, 205.0f + 19.0f * (float)i, "target", Ctl::set, BtnStyle::rubber, 50, 24);
            c.setTo = targetOf(r.ids[0]);
            c.show = {onScreen(k)};
        }
    /* the data knob and VALUE - / +: on the row picked, while it is on the page shown */
    for (int k = 0; k < NS; k++)
        for (auto &r : scr[(size_t)k].rows)
            for (auto &id : r.ids) {
                std::vector<Cond> sh = {onScreen(k), Cond{H("target"), targetOf(id), targetOf(id), NTARGET}};
                auto &kn = b.K(427, 224, id, sh, 70.0f);
                kn.endless = true;
                auto &dn = b.B(412, 274, "vdec", Ctl::momentary, BtnStyle::rubber, 50, 24);
                dn.show = sh;
                auto &up = b.B(442, 274, "vinc", Ctl::momentary, BtnStyle::rubber, 50, 24);
                up.show = sh;
            }
    /* PAGE: the menu's other pages */
    for (int k = 0; k < NS; k++) {
        auto &s = scr[(size_t)k];
        int first = k - s.index;
        for (int dir = 0; dir < 2; dir++) {
            int to = first + (s.index + (dir ? 1 : s.count - 1)) % s.count;
            auto &c = b.B(dir ? 320.0f : 285.0f, 274, "", Ctl::set, BtnStyle::pale, 56, 24);
            c.local = LS;
            c.localSteps = NS;
            c.setTo = to;
            c.litNever = true;
            c.show = {onScreen(k)};
        }
    }
    /* what never changes on the MPC, drawn on the background: COMPARE and SAVE, the row buttons, PAGE, VALUE, the
       data knob (each covered while it works), the active voices' LEDs */
    auto pgp = b.pg;
    b.pg.extra.push_back([pgp, d](Graphics &g) {
        Builder bb("", 0, 48, 114, 460, 303);
        for (auto p : std::vector<Point<float>>{{166.5f, 141}, {166.5f, 171}})
            drawButton(g, bb.box(p.x, p.y, 52, 26).reduced(bb.sz(1), bb.sz(2)), BtnStyle::rubber, false, false, false);
        for (int i = 0; i < 3; i++)
            drawButton(g, bb.box(206, 205.0f + 19.0f * (float)i, 50, 24).reduced(bb.sz(1), bb.sz(2)), BtnStyle::rubber, false, false, false);
        for (auto p : std::vector<Point<float>>{{412, 274}, {442, 274}})
            drawButton(g, bb.box(p.x, p.y, 50, 24).reduced(bb.sz(1), bb.sz(2)), BtnStyle::rubber, false, false, false);
        auto kr = bb.box(427, 224, 70, 70);
        drawKnob(g, kr, false, true, false);
        drawKnobPointer(g, kr, 0.0f, true);
        for (int i = 0; i < 8; i++)
            drawLed(g, bb.at(407.3f + 13.2f * (float)(i % 4), i < 4 ? 148.0f : 159.0f), bb.sz(17.0f), 0.0f);
        (void)d;
    });
    b.Q({{"volume"}});
    (void)panel;
    return b.pg;
}

/* ----------------------------------------------------------------------------- drawing --- */
Image background(PeekPanel &panel, const Page &pg)
{
    Image img(Image::RGB, TW, TH, true);
    Graphics g(img);
    drawPanelFace(g, Rectangle<float>(0.0f, 0.0f, (float)TW, (float)TH), 1.0f);
    for (auto *c : panel.getChildren())
        c->setVisible(false);
    auto part = panel.createComponentSnapshot(pg.region.getSmallestIntegerContainer(), true, pg.s);
    g.drawImage(part, Rectangle<float>(pg.off.x, pg.off.y, pg.region.getWidth() * pg.s, pg.region.getHeight() * pg.s),
                RectanglePlacement::stretchToFit);
    for (auto &f : pg.extra)
        f(g);
    /* the bottom strip: PEEK, the page; the maker's tag */
    g.setColour(col::legend);
    g.setFont(font(Face::logo, 22.0f));
    g.drawText("PEEK", Rectangle<float>(18.0f, 588.0f, 100.0f, 30.0f), Justification::centredLeft, false);
    g.setColour(col::blue);
    g.fillRect(Rectangle<float>(18.0f, 616.0f, 62.0f, 2.0f));
    g.setColour(col::legend.withAlpha(0.75f));
    g.setFont(font(Face::title, 15.0f));
    g.drawText(pg.name, Rectangle<float>(110.0f, 588.0f, 300.0f, 30.0f), Justification::centredLeft, false);
    drawTag(g, Rectangle<float>(1280.0f - 18.0f - 200.0f, 628.0f - 12.0f - 24.0f, 200.0f, 24.0f));
    return img;
}

/* a control's picture at a value (LEDs: the LED `row`, lit or not) */
void paintCtl(Graphics &g, const Ctl &c, int v, int row = -1, bool lit = false)
{
    auto rg = rangeOf(c.id.isEmpty() ? String("hold") : c.id);
    float pos = rg.hi > rg.lo ? (float)(v - rg.lo) / (float)(rg.hi - rg.lo) : 0.0f;
    if (c.kind == Ctl::knob) {
        int p = paramIndex(c.id);
        drawKnob(g, c.r, p >= 0 && PARAM[p].lo < 0, c.endless, c.big);
        drawKnobPointer(g, c.r, c.endless ? pos * 0.999f : pos, c.endless);
    } else if (c.kind == Ctl::fader) {
        drawSliderBack(g, c.r);
        drawSliderCap(g, c.r, pos);
    } else if (c.kind == Ctl::button) {
        drawButton(g, c.r.reduced(c.r.getWidth() * 0.02f, c.r.getHeight() * 0.077f), c.style, lit, false, false);
        if (c.text.isNotEmpty()) {
            g.setColour(c.style == BtnStyle::pale ? Colour(0xff2a2d31) : col::legend);
            g.setFont(font(Face::title, c.r.getHeight() * 0.4f));
            g.drawText(c.text, c.r, Justification::centred, false);
        }
    } else if (c.kind == Ctl::leds) {
        drawLed(g, c.pts[(size_t)row].first, c.lw, lit ? 1.0f : 0.0f);
    }
}

bool writePng(const Image &img, const File &f)
{
    f.deleteFile();
    FileOutputStream out(f);
    PNGImageFormat png;
    return out.openedOk() && png.writeImageToStream(img, out);
}
var rectVar(Rectangle<int> r) { return Array<var>{r.getX(), r.getY(), r.getWidth(), r.getHeight()}; }
Image cut(const Image &bg, Rectangle<int> r, std::function<void(Graphics &)> paint)
{
    Image img(Image::RGB, r.getWidth(), r.getHeight(), true);
    Graphics g(img);
    g.drawImageAt(bg.getClippedImage(r), 0, 0);
    g.addTransform(AffineTransform::translation((float)-r.getX(), (float)-r.getY()));
    paint(g);
    return img;
}
var condsVar(const std::vector<Cond> &cs)
{
    Array<var> a;
    for (auto &c : cs)
        a.add(c.a == c.b ? "IndexedEnabling/" + String(c.a) + "/" + String(c.steps) + "/" + c.h
                         : "RangedEnabling/" + String(c.a) + "/" + String(c.b) + "/" + String(c.steps) + "/" + c.h);
    return a;
}
int defaultOf(const String &id)
{
    int p = paramIndex(id);
    Patch init;
    patchInit(init);
    return p >= 0 ? init.v[p] : id == "quality" ? 1 : id == "volume" ? 90 : 0;
}
} // namespace

int main(int argc, char **argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 2) {
        std::fprintf(stderr, "usage: peek_skin_render <out folder> [--preview]\n");
        return 2;
    }
    File dir = File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.deleteRecursively();
    dir.createDirectory();
    bool preview = argc > 2 && String(argv[2]) == "--preview";

    PeekProcessor proc;
    std::unique_ptr<AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(PeekPanel::W, PeekPanel::H);
    auto *panel = dynamic_cast<PeekPanel *>(ed->getChildComponent(0));
    if (!panel) {
        std::fprintf(stderr, "no panel\n");
        return 1;
    }
    auto scr = screens(*panel);
    std::vector<Page> pages = {oscPage(), filterPage(), envPage(), screenPage(*panel, scr)};

    Array<var> pageList;
    int n = 0;
    for (auto &pg : pages) {
        String tag = pg.name.toLowerCase();
        Image bg = background(*panel, pg);
        if (preview) {                   /* (the page as the MPC shows it: the Init Patch's values, the first menu page) */
            Graphics g(bg);
            for (auto &c : pg.ctls) {
                bool shown = true;
                for (auto &cd : c.show) {
                    int v = cd.h.startsWith("Local") ? 0 : cd.h == H("target") ? 0 : -1;
                    if (v < 0) {
                        for (int i = 0; i < NPARAM; i++)
                            if (cd.h == H(PARAM[i].id))
                                v = defaultOf(PARAM[i].id) - PARAM[i].lo;
                    }
                    shown = shown && v >= cd.a && v <= cd.b;
                }
                if (!shown)
                    continue;
                int v = c.id.isEmpty() ? 0 : defaultOf(c.id);
                if (c.kind == Ctl::leds)
                    for (size_t i = 0; i < c.pts.size(); i++)
                        paintCtl(g, c, v, (int)i, c.pts[i].second == v);
                else if (c.kind == Ctl::button)
                    paintCtl(g, c, v, -1, c.litAlways || (!c.litNever && c.mode == Ctl::toggle && v > 0));
                else if (c.kind == Ctl::image)
                    g.drawImageAt(c.pic, (int)c.r.getX(), (int)c.r.getY());
                else if (c.kind == Ctl::label) {
                    auto rg = rangeOf(c.id);
                    int p = paramIndex(c.id);
                    char t[48] = "";
                    if (p >= 0)
                        paramText(p, v, t, sizeof t);
                    g.setColour(col::oled);
                    g.setFont(FontOptions(c.textH * 0.9f));
                    g.drawText(p >= 0 ? String(t) : rg.names[jlimit(0, rg.names.size() - 1, v - rg.lo)], c.r, c.just, false);
                } else
                    paintCtl(g, c, v);
            }
            writePng(bg, dir.getChildFile(String(++n) + "-" + pg.name + ".png"));
            continue;
        }
        writePng(bg, dir.getChildFile(tag + "_panel.png"));
        Array<var> list;
        int k = 0;
        for (auto &c : pg.ctls) {
            auto *o = new DynamicObject();
            String base = tag + "_" + String(k++).paddedLeft('0', 3) + "_" + c.id.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
            o->setProperty("id", c.id);
            o->setProperty("handle", c.local.isNotEmpty() ? c.local : c.id.isEmpty() ? String() : H(c.id));
            o->setProperty("show", condsVar(c.show));
            o->setProperty("q", c.q);
            auto rg = rangeOf(c.id.isEmpty() ? String("hold") : c.id);
            int steps = c.local.isNotEmpty() ? c.localSteps : rg.hi - rg.lo + 1;
            o->setProperty("steps", steps);
            Array<var> names;
            for (auto &t : rg.names)
                names.add(t);
            o->setProperty("names", c.local.isNotEmpty() ? Array<var>() : names);
            if (c.kind == Ctl::knob || c.kind == Ctl::fader) {
                auto r = c.r.expanded(3.0f).getSmallestIntegerContainer();
                int frames = c.endless ? 64 : jmax(2, jmin(steps, 128, 16000 / (r.getHeight() + 1)));   /* (the data knob: one picture for all; a strip under 16384 px) */
                static bool dataDone = false;
                if (c.endless && dataDone) {
                    o->setProperty("kind", "knob");
                    o->setProperty("bipolar", rg.lo < 0);
                    o->setProperty("rect", rectVar(r));
                    o->setProperty("frames", frames);
                    o->setProperty("strip", String("screen_data_knob.png"));
                    o->setProperty("vertical", false);
                    list.add(var(o));
                    continue;
                }
                dataDone = dataDone || c.endless;
                Image strip(Image::RGB, r.getWidth(), r.getHeight() * frames, true);
                {
                    Graphics sg(strip);
                    for (int f = 0; f < frames; f++) {
                        int idx = frames == steps ? f : (int)std::lround((double)f * (steps - 1) / (frames - 1));
                        Ctl cc = c;
                        if (c.endless)
                            cc.id = "volume";    /* (its pointer: a turn of the frames, whatever it changes) */
                        int v = c.endless ? (int)std::lround(127.0 * f / (frames - 1)) : rg.lo + idx;
                        sg.drawImageAt(cut(bg, r, [&](Graphics &g) { paintCtl(g, cc, v); }), 0, f * r.getHeight());
                    }
                }
                String f = c.endless ? String("screen_data_knob.png") : base + "_knob.png";
                writePng(strip, dir.getChildFile(f));
                o->setProperty("kind", "knob");
                o->setProperty("bipolar", rg.lo < 0);
                o->setProperty("rect", rectVar(r));
                o->setProperty("frames", frames);
                o->setProperty("strip", f);
                o->setProperty("vertical", c.kind == Ctl::fader);
            } else if (c.kind == Ctl::button) {
                auto r = c.r.getSmallestIntegerContainer();
                o->setProperty("kind", "button");
                o->setProperty("rect", rectVar(r));
                o->setProperty("momentary", c.mode == Ctl::momentary);
                String on = base + "_on.png", off = base + "_off.png";
                writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, 0, -1, !c.litNever); }), dir.getChildFile(on));
                writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, 0, -1, c.litAlways); }), dir.getChildFile(off));
                o->setProperty("on", on);
                o->setProperty("off", off);
                /* how it acts: a toggle; a key held; to a value (a radio button); a step to the next value (one
                   picture a value, each shown while the value is its own) */
                String mode = c.mode == Ctl::cycle ? "cycle" : c.mode == Ctl::momentary ? "momentary" : c.mode == Ctl::set ? "set" : "toggle";
                o->setProperty("mode", mode);
                o->setProperty("set", c.setTo);
                if (c.mode == Ctl::cycle) {
                    Array<var> nx;
                    for (int v = 0; v < steps; v++)
                        nx.add(c.next.empty() ? (v + 1) % steps : c.next[(size_t)v]);
                    o->setProperty("next", nx);
                }
            } else if (c.kind == Ctl::leds) {
                o->setProperty("kind", "leds");
                Array<var> rows;
                for (size_t i = 0; i < c.pts.size(); i++) {
                    auto pt = c.pts[i].first;
                    auto r = Rectangle<float>(c.lw * 1.25f, c.lw * 0.8f).withCentre(pt).getSmallestIntegerContainer();
                    auto *ro = new DynamicObject();
                    ro->setProperty("rect", rectVar(r));
                    ro->setProperty("value", c.pts[i].second - rg.lo);
                    String on = base + "_led" + String((int)i) + "_on.png", off = base + "_led" + String((int)i) + "_off.png";
                    writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, 0, (int)i, true); }), dir.getChildFile(on));
                    writePng(cut(bg, r, [&](Graphics &g) { paintCtl(g, c, 0, (int)i, false); }), dir.getChildFile(off));
                    ro->setProperty("on", on);
                    ro->setProperty("off", off);
                    rows.add(var(ro));
                }
                o->setProperty("rows", rows);
            } else if (c.kind == Ctl::image) {
                String f = base + "_oled.png";
                writePng(c.pic, dir.getChildFile(f));
                o->setProperty("kind", "image");
                o->setProperty("rect", rectVar(c.r.getSmallestIntegerContainer().withSize(c.pic.getWidth(), c.pic.getHeight())));
                o->setProperty("image", f);
            } else {
                o->setProperty("kind", "label");
                o->setProperty("rect", rectVar(c.r.getSmallestIntegerContainer()));
                o->setProperty("height", c.textH);
                o->setProperty("right", c.just == Justification::centredRight);
            }
            list.add(var(o));
        }
        auto *po = new DynamicObject();
        po->setProperty("name", pg.name);
        po->setProperty("key", pg.key);
        po->setProperty("background", tag + "_panel.png");
        po->setProperty("controls", list);
        pageList.add(var(po));
        std::printf("%-8s %d controls\n", pg.name.toRawUTF8(), (int)pg.ctls.size());
    }
    if (preview)
        return 0;
    auto *m = new DynamicObject();
    m->setProperty("pages", pageList);
    m->setProperty("screens", (int)scr.size());
    Array<var> ids;
    for (int i = 0; i < NPARAM; i++)
        ids.add(String(PARAM[i].id));
    m->setProperty("params", ids);
    dir.getChildFile("manifest.json").replaceWithText(JSON::toString(var(m)));
    /* the screen table for the MPC plugin: each display page's rows (for VALUE - / +, nothing else needs it) */
    std::printf("manifest.json in %s (%d display pages)\n", dir.getFullPathName().toRawUTF8(), (int)scr.size());
    ed.reset();
    return 0;
}
