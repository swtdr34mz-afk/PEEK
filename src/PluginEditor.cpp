/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "PluginEditor.h"
#include "ui/PixelFont.h"

using namespace pkui;
using namespace pk;
using Panel = PeekPanel;

namespace
{
constexpr float KD = 64.0f;                      /* a knob's size */
constexpr float LS = 11.0f;                      /* the legends' size */

/* the panel's places, from the Peak's photo (its pixels: about 1.8 panel units each) */
float X(float px) { return 38.0f + (px - 45.0f) * 1.797f; }
float Y(float py) { return (py - 95.0f) * 1.833f; }
juce::Point<float> at(float px, float py) { return {X(px), Y(py)}; }
juce::Rectangle<float> R(float x0, float y0, float x1, float y1) { return {X(x0), Y(y0), X(x1) - X(x0), Y(y1) - Y(y0)}; }

/* ------------------------------------------------------------------------------- a knob --- */
class Knob : public juce::Component, public Panel::Live
{
public:
    using Target = std::function<juce::RangedAudioParameter *()>;
    Knob(Target t, std::function<void(juce::RangedAudioParameter *)> touched, bool big = false)
        : target(std::move(t)), onTouch(std::move(touched)), big(big)
    {
    }
    Knob(std::function<void(int)> turn) : onTurn(std::move(turn)) { endless = true; }

    void paint(juce::Graphics &g) override
    {
        float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
        auto r = getLocalBounds().toFloat();
        if (body.isNull() || sc != bodyScale || bipolar != bodyBipolar) {
            bodyScale = sc;
            bodyBipolar = bipolar;
            body = juce::Image(juce::Image::ARGB, juce::jmax(1, (int)std::ceil(r.getWidth() * sc)), juce::jmax(1, (int)std::ceil(r.getHeight() * sc)), true);
            juce::Graphics bg(body);
            bg.addTransform(juce::AffineTransform::scale(sc));
            drawKnob(bg, r, bipolar, endless, big);
        }
        g.drawImage(body, r, juce::RectanglePlacement::stretchToFit);
        float pos = endless ? turned * 0.04f : shownPos;
        drawKnobPointer(g, r, endless ? pos - std::floor(pos) : pos, endless);
    }
    void poll() override
    {
        if (endless)
            return;
        auto *p = target();
        float pos = p ? p->getValue() : 0.0f;
        bool bip = false;
        if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p))
            bip = ip->getRange().getStart() < 0;
        if (pos != shownPos || bip != bipolar) {
            shownPos = pos;
            bipolar = bip;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        startY = e.position.y;
        acc = 0.0f;
        if (endless)
            return;
        if (auto *p = target()) {
            if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p))
                startV = ip->get();
            else
                startV = (int)std::lround(p->getValue() * 1000.0f);
            p->beginChangeGesture();
            if (onTouch)
                onTouch(p);
        }
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        float dy = startY - e.position.y;
        bool fine = e.mods.isShiftDown();
        if (endless) {
            float st = fine ? 16.0f : 8.0f;
            int n = (int)((dy - acc) / st);
            if (n != 0) {
                acc += (float)n * st;
                turned += (float)n;
                onTurn(n);
                repaint();
            }
            return;
        }
        auto *p = target();
        if (!p)
            return;
        if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p)) {
            int lo = ip->getRange().getStart(), hi = ip->getRange().getEnd(), range = hi - lo;
            float px = juce::jlimit(1.0f, 18.0f, 250.0f / (float)juce::jmax(1, range)) * (fine ? 4.0f : 1.0f);
            p->setValueNotifyingHost(p->convertTo0to1((float)juce::jlimit(lo, hi, startV + (int)std::lround(dy / px))));
        } else
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, (float)startV / 1000.0f + dy / (fine ? 900.0f : 220.0f)));
        if (onTouch)
            onTouch(p);
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (!endless)
            if (auto *p = target())
                p->endChangeGesture();
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d == 0)
            return;
        if (endless) {
            turned += (float)d;
            onTurn(d);
            repaint();
            return;
        }
        if (auto *p = target()) {
            p->beginChangeGesture();
            if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(p))
                p->setValueNotifyingHost(p->convertTo0to1((float)juce::jlimit(ip->getRange().getStart(), ip->getRange().getEnd(), ip->get() + d)));
            else
                p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + 0.01f * (float)d));
            p->endChangeGesture();
            if (onTouch)
                onTouch(p);
        }
    }
    void mouseDoubleClick(const juce::MouseEvent &) override
    {
        if (endless)
            return;
        if (auto *p = target()) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getDefaultValue());
            p->endChangeGesture();
            if (onTouch)
                onTouch(p);
        }
    }

private:
    Target target;
    std::function<void(juce::RangedAudioParameter *)> onTouch;
    std::function<void(int)> onTurn;
    bool endless = false, bipolar = false, bodyBipolar = false, big = false;
    float shownPos = -1.0f, startY = 0.0f, acc = 0.0f, turned = 0.0f, bodyScale = 0.0f;
    juce::Image body;
    int startV = 0;
};

/* ----------------------------------------------------------------------------- a slider --- */
class Fader : public juce::Component, public Panel::Live
{
public:
    Fader(Knob::Target t, std::function<void(juce::RangedAudioParameter *)> touched) : target(std::move(t)), onTouch(std::move(touched)) {}
    void paint(juce::Graphics &g) override
    {
        auto r = getLocalBounds().toFloat();
        drawSliderBack(g, r);
        drawSliderCap(g, r, shownPos);
    }
    void poll() override
    {
        auto *p = target();
        float pos = p ? p->getValue() : 0.0f;
        if (pos != shownPos) {
            shownPos = pos;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (auto *p = target()) {
            p->beginChangeGesture();
            startPos = p->getValue();
            startY = e.position.y;
            float top = 8.0f, bot = (float)getHeight() - 8.0f;
            float capY = bot - (bot - top) * startPos;
            if (std::fabs(e.position.y - capY) > 14.0f) {   /* (a click beside the cap: the cap jumps there) */
                startPos = juce::jlimit(0.0f, 1.0f, (bot - e.position.y) / (bot - top));
                p->setValueNotifyingHost(startPos);
            }
            if (onTouch)
                onTouch(p);
        }
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (auto *p = target()) {
            float span = (float)getHeight() - 16.0f;
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, startPos + (startY - e.position.y) / span * (e.mods.isShiftDown() ? 0.25f : 1.0f)));
            if (onTouch)
                onTouch(p);
        }
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (auto *p = target())
            p->endChangeGesture();
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (auto *p = dynamic_cast<juce::AudioParameterInt *>(target()); p && d) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1((float)juce::jlimit(p->getRange().getStart(), p->getRange().getEnd(), p->get() + d)));
            p->endChangeGesture();
            if (onTouch)
                onTouch(p);
        }
    }

private:
    Knob::Target target;
    std::function<void(juce::RangedAudioParameter *)> onTouch;
    float shownPos = -1.0f, startPos = 0.0f, startY = 0.0f;
};

/* ------------------------------------------------------------------------------ a button --- */
class Btn : public juce::Component, public Panel::Live
{
public:
    Btn(BtnStyle s, std::function<void()> press, std::function<bool()> on, std::function<void()> release = {})
        : style(s), onPress(std::move(press)), isOn(std::move(on)), onRelease(std::move(release))
    {
        setRepaintsOnMouseActivity(true);
    }
    void paint(juce::Graphics &g) override
    {
        float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
        int k = (lit ? 1 : 0) | (down ? 2 : 0) | (isMouseOver() ? 4 : 0);
        auto r = getLocalBounds().toFloat();
        auto big = r.expanded(r.getWidth() * 0.5f, r.getHeight() * 1.0f);
        if (sc != cacheScale) {
            for (auto &c : cache)
                c = juce::Image();
            cacheScale = sc;
        }
        if (cache[k].isNull()) {
            cache[k] = juce::Image(juce::Image::ARGB, juce::jmax(1, (int)std::ceil(big.getWidth() * sc)), juce::jmax(1, (int)std::ceil(big.getHeight() * sc)), true);
            juce::Graphics cg(cache[k]);
            cg.addTransform(juce::AffineTransform::scale(sc).translated(-big.getX() * sc, -big.getY() * sc));
            drawButton(cg, r.reduced(1.0f, 2.0f), style, lit, down, isMouseOver());
        }
        g.drawImage(cache[k], big, juce::RectanglePlacement::stretchToFit);
        if (text.isNotEmpty()) {
            g.setColour(style == BtnStyle::pale ? juce::Colour(0xff2a2d31) : col::legend);
            g.setFont(font(Face::title, 13.0f));
            g.drawText(text, r, juce::Justification::centred, false);
        }
    }
    void poll() override
    {
        bool l = isOn ? isOn() : false;
        if (l != lit) {
            lit = l;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        down = true;
        repaint();
        if (onPress)
            onPress();
        poll();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        down = false;
        repaint();
        if (onRelease)
            onRelease();
    }
    juce::String text;                           /* (written on the cap: the animate buttons' numbers) */

private:
    BtnStyle style;
    std::function<void()> onPress;
    std::function<bool()> isOn;
    std::function<void()> onRelease;
    bool lit = false, down = false;
    juce::Image cache[8];
    float cacheScale = 0.0f;
};

/* ----------------------------------------------------------------------------- LEDs --- */
class Leds : public juce::Component, public Panel::Live
{
public:
    Leds(std::vector<juce::Point<float>> pts, float w, std::function<float(int)> level, std::function<void(int)> pick = {})
        : level(std::move(level)), pick(std::move(pick)), w(w)
    {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (auto &p : pts) {
            x0 = juce::jmin(x0, p.x);
            y0 = juce::jmin(y0, p.y);
            x1 = juce::jmax(x1, p.x);
            y1 = juce::jmax(y1, p.y);
        }
        auto bb = juce::Rectangle<float>(x0, y0, x1 - x0, y1 - y0).expanded(w * 1.3f, w * 0.9f);
        setBounds(bb.getSmallestIntegerContainer());
        for (auto &p : pts)
            c.push_back(p - getPosition().toFloat());
        shown.assign(c.size(), -1.0f);
        setInterceptsMouseClicks((bool)this->pick, false);
    }
    void paint(juce::Graphics &g) override
    {
        for (size_t i = 0; i < c.size(); i++)
            drawLed(g, c[i], w, shown[i]);
    }
    void poll() override
    {
        bool ch = false;
        for (size_t i = 0; i < c.size(); i++) {
            float v = std::round(juce::jlimit(0.0f, 1.0f, level((int)i)) * 16.0f) / 16.0f;
            if (v != shown[i]) {
                shown[i] = v;
                ch = true;
            }
        }
        if (ch)
            repaint();
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (!pick)
            return;
        float best = 1e9f;
        int bi = 0;
        for (size_t i = 0; i < c.size(); i++)
            if (float d = c[i].getDistanceFrom(e.position); d < best) {
                best = d;
                bi = (int)i;
            }
        pick(bi);
    }

private:
    std::vector<juce::Point<float>> c;
    std::function<float(int)> level;
    std::function<void(int)> pick;
    std::vector<float> shown;
    float w;
};
} // namespace

/* ---------------------------------------------------------------------------------- OLED --- */
class Oled : public juce::Component, public Panel::Live
{
public:
    explicit Oled(Panel &p) : panel(p) {}
    void poll() override
    {
        juce::String s = signature();
        if (s != last) {
            last = s;
            repaint();
        }
    }
    void paint(juce::Graphics &g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour(Colour(0xff000000));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(Colours::white.withAlpha(0.07f));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);
        /* the Peak's OLED: a dot matrix, 5 x 7 glyphs (descenders two dots below), four lines of 11 dots */
        auto in = r.reduced(8.0f, 6.0f);
        const float d = in.getHeight() / 44.0f, lh = 11.0f * d;
        const int cols = (int)((in.getWidth() + d) / (6.0f * d));
        auto line = [&](int i) { return Rectangle<float>(in.getX(), in.getY() + lh * (float)i, in.getWidth(), lh); };
        auto top = [&](int i) { return in.getY() + lh * (float)i + d; };
        auto fit = [](const String &t, int n) { return n <= 0 ? String() : t.substring(0, n); };
        if (panel.showingTouch()) {
            /* a control touched: its name, and its value large */
            String name, val;
            what(name, val);
            pix(g, fit(name, cols), in.getX(), top(0), d, true);
            float d2 = jmin(2.0f * d, (in.getWidth() + d) / jmax(1.0f, 6.0f * (float)val.length()));
            float w = width(val, d2, false);
            pix(g, val, in.getCentreX() - w * 0.5f, in.getY() + lh * 2.6f - 3.5f * d2, d2, false);
            return;
        }
        auto pg = panel.page();
        auto ps = panel.pages(panel.menu);
        int pi = juce::jlimit(0, (int)ps.size() - 1, panel.pageOf[panel.menu]);
        String pn = String(pi + 1) + "/" + String((int)ps.size());
        pix(g, fit(pg.title.toUpperCase(), (cols * 6) / 7 - pn.length() - 1), in.getX(), top(0), d, true);
        pix(g, pn, in.getRight() - width(pn, d, false), top(0), d, false);
        rowsAt.clear();
        for (int i = 0; i < 3; i++) {
            auto lr = line(i + 1);
            rowsAt.push_back(lr);
            if (i >= (int)pg.rows.size())
                continue;
            bool sel = panel.row == i;
            if (sel) {
                g.setColour(col::oled.withAlpha(0.10f));
                g.fillRect(lr.expanded(4.0f, 0.0f));
            }
            auto &e = pg.rows[(size_t)i];
            int v = e.get();
            String vt = e.text ? e.text(v) : String(v);
            float y = top(i + 1);
            if (pg.mod && i == 0) {
                /* the slot's two sources: the one in hand marked */
                int b = pg.modBase, half = cols / 2;
                String a = String(MODSRC_NAME[panel.proc.value(b + MOD_SA)]), bb = String(MODSRC_NAME[panel.proc.value(b + MOD_SB)]);
                String la = fit((panel.modAB == 0 && sel ? ">" : ":") + a, half - 1);
                String rb = fit(bb, half - 2) + (panel.modAB == 1 && sel ? "<" : ":");
                pix(g, la, in.getX(), y, d, false);
                pix(g, rb, in.getRight() - width(rb, d, false), y, d, false);
                continue;
            }
            vt = fit(vt, cols);
            pix(g, fit(e.label, cols - vt.length() - 1), in.getX(), y, d, false);
            pix(g, vt, in.getRight() - width(vt, d, false), y, d, false);
        }
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d)
            panel.valueStep(d);
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (panel.showingTouch()) {
            panel.shown = -1;
            panel.shownGlobal = nullptr;
            repaint();
            return;
        }
        for (int i = 0; i < (int)rowsAt.size(); i++)
            if (rowsAt[(size_t)i].contains(e.position)) {
                panel.row = i;
                if (panel.page().mod && i == 0)
                    panel.modAB = e.position.x > (float)getWidth() * 0.5f ? 1 : 0;
                if (panel.menu == Panel::M_PATCH && i == 0)
                    panel.patchMenu(getScreenBounds());
                else
                    panel.rowMenu(i, getScreenBounds());
                return;
            }
        if (panel.menu == Panel::M_PATCH)
            panel.patchMenu(getScreenBounds());
    }

private:
    /* one line in the OLED's dots: x, y its top left, d one dot; bold: every dot doubled to the right */
    static float width(const juce::String &t, float d, bool bold)
    {
        return t.isEmpty() ? 0.0f : ((float)t.length() * (bold ? 7.0f : 6.0f) - 1.0f) * d;
    }
    static void pix(juce::Graphics &g, const juce::String &t, float x, float y, float d, bool bold)
    {
        juce::RectangleList<float> dots;
        const float s = d * 0.86f;                  /* (a hair between the dots, as on the glass) */
        for (auto p = t.getCharPointer(); !p.isEmpty();) {
            juce::juce_wchar c = p.getAndAdvance();
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
        g.setColour(col::oled.withAlpha(0.16f));     /* (the faint glow round each dot) */
        for (auto &q : dots)
            g.fillRect(q.expanded(d * 0.22f));
        g.setColour(col::oled);
        g.fillRectList(dots);
    }
    void what(juce::String &name, juce::String &val) const
    {
        if (panel.shownGlobal) {
            name = panel.shownGlobal->getName(30);
            val = panel.shownGlobal->getCurrentValueAsText();
            return;
        }
        int p = panel.shown;
        if (p < 0)
            return;
        name = PARAM[p].name;
        char t[40];
        paramText(p, panel.proc.value(p), t, sizeof t);
        val = t;
    }
    juce::String signature() const
    {
        juce::String s;
        s << (int)panel.menu << "|" << panel.pageOf[panel.menu] << "|" << panel.row << "|" << panel.modAB << "|"
          << (int)panel.showingTouch() << "|" << panel.proc.getCurrentProgram() << "|" << panel.proc.patchName();
        if (panel.showingTouch()) {
            juce::String n, v;
            what(n, v);
            s << n << v;
        } else {
            auto pg = panel.page();
            s << pg.title;
            for (auto &e : pg.rows)
                s << "|" << e.get();
            if (pg.mod)
                s << panel.proc.value(pg.modBase + MOD_SA) << panel.proc.value(pg.modBase + MOD_SB);
        }
        return s;
    }
    Panel &panel;
    juce::String last;
    std::vector<juce::Rectangle<float>> rowsAt;
};

/* ============================================================================ the panel === */
template <class T, class... A> T *Panel::add(A &&...a)
{
    auto c = std::make_unique<T>(std::forward<A>(a)...);
    T *raw = c.get();
    addAndMakeVisible(*raw);
    if (auto *lv = dynamic_cast<Live *>(raw))
        live.push_back(lv);
    parts.push_back(std::move(c));
    return raw;
}

Panel::PeekPanel(PeekProcessor &p) : proc(p)
{
    setLookAndFeel(&look);
    setSize(W, H);
    build();
    startTimerHz(30);
}
Panel::~PeekPanel()
{
    stopTimer();
    if (dialog != nullptr)
        delete dialog.getComponent();
    setLookAndFeel(nullptr);
}

void Panel::touch(int p)
{
    shown = p;
    shownGlobal = nullptr;
    shownAt = juce::Time::getMillisecondCounterHiRes();
}
void Panel::touchGlobal(juce::RangedAudioParameter *p)
{
    shownGlobal = p;
    shown = -1;
    shownAt = juce::Time::getMillisecondCounterHiRes();
}
bool Panel::showingTouch() const
{
    return (shown >= 0 || shownGlobal != nullptr) && juce::Time::getMillisecondCounterHiRes() - shownAt < 1800.0;
}

/* -------------------------------------------------------------------------------- the menus --- */
std::vector<Panel::Page> Panel::pages(Menu m) const
{
    std::vector<Page> ps;
    auto *self = const_cast<Panel *>(this);
    auto pe = [self](int p, const char *label) {
        Entry e;
        e.label = label;
        e.lo = PARAM[p].lo;
        e.hi = PARAM[p].hi;
        e.param = p;
        e.get = [self, p] { return self->proc.value(p); };
        e.set = [self, p](int v) { self->proc.setValue(p, v); };
        e.text = [p](int v) {
            char t[40];
            paramText(p, v, t, sizeof t);
            return juce::String(t);
        };
        return e;
    };
    auto page = [&](const juce::String &t, std::initializer_list<Entry> rows) {
        Page pg;
        pg.title = t;
        pg.rows = rows;
        ps.push_back(pg);
    };
    auto gl = [self](const char *id, const char *label) {
        auto *q = self->proc.apvts.getParameter(id);
        Entry e;
        e.label = label;
        auto *ip = dynamic_cast<juce::AudioParameterInt *>(q);
        auto *cp = dynamic_cast<juce::AudioParameterChoice *>(q);
        e.lo = ip ? ip->getRange().getStart() : 0;
        e.hi = ip ? ip->getRange().getEnd() : cp ? cp->choices.size() - 1 : 1;
        e.get = [q, ip, cp] { return ip ? ip->get() : cp ? cp->getIndex() : (q->getValue() > 0.5f ? 1 : 0); };
        e.set = [q, ip, cp](int v) {
            q->beginChangeGesture();
            q->setValueNotifyingHost(ip || cp ? q->convertTo0to1((float)v) : (v ? 1.0f : 0.0f));
            q->endChangeGesture();
        };
        e.text = [q, ip, cp](int v) { return q->getText(ip || cp ? q->convertTo0to1((float)v) : (v ? 1.0f : 0.0f), 20); };
        return e;
    };
    switch (m) {
    case M_PATCH: {
        Entry e;
        e.label = "Patch";
        e.lo = 0;
        e.hi = proc.getNumPrograms() - 1;
        e.get = [self] { return self->proc.getCurrentProgram(); };
        e.set = [self](int v) { self->proc.setCurrentProgram(v); };
        e.text = [](int v) { return v < NFACTORY ? "F" + juce::String(v + 1).paddedLeft('0', 3) : "U" + juce::String(v - NFACTORY + 1).paddedLeft('0', 3); };
        Entry n;
        n.label = "";
        n.lo = n.hi = 0;
        n.get = [] { return 0; };
        n.set = [](int) {};
        n.text = [self](int) { return self->proc.patchName(); };
        Entry c = n;
        c.label = "Category";
        c.text = [self](int) { return self->proc.patchCategory(); };
        page("PATCH", {e, n, c});
        break;
    }
    case M_OSC:
        page("OSC COMN 1", {pe(P_DIVERGE, "Diverge"), pe(P_DRIFT, "Drift"), pe(P_NOISE_LPF, "NoiseLPF")});
        page("OSC COMN 2", {pe(P_NOISE_HPF, "NoiseHPF"), pe(P_KEYSYNC, "KeySync")});
        for (int k = 0; k < 3; k++) {
            int b = P_OSC1 + k * OSC_N;
            juce::String t = "OSCILLATOR " + juce::String(k + 1);
            page(t, {pe(b + OSC_MORE, "WaveMore"), pe(b + OSC_FIXNOTE, "FixedNote"), pe(b + OSC_BEND, "BendRange")});
            page(t, {pe(b + OSC_VSYNC, "Vsync"), pe(b + OSC_DENSE, "SawDense"), pe(b + OSC_DDET, "DenseDet")});
        }
        break;
    case M_ENV: {
        const char *N[3] = {"AMP ENVELOPE", "MOD ENVELOPE 1", "MOD ENVELOPE 2"};
        for (int k = 0; k < 3; k++) {
            int b = k == 0 ? P_AENV : k == 1 ? P_M1ENV : P_M2ENV;
            page(N[k], {pe(b + ENV_VEL, "Velocity"), pe(b + ENV_TRIG, "MonoTrig"), pe(b + ENV_DLY, "Delay")});
            page(N[k], {pe(b + ENV_HOLD, "HoldTime"), pe(b + ENV_REP, "Repeats")});
        }
        page("ANIMATE 1", {pe(P_AN1_ATT, "Attack"), pe(P_AN1_REL, "Release")});
        page("ANIMATE 2", {pe(P_AN2_ATT, "Attack"), pe(P_AN2_REL, "Release")});
        break;
    }
    case M_LFO:
        for (int k = 0; k < 2; k++) {
            int b = k ? P_LFO2 : P_LFO1;
            juce::String t = "LFO " + juce::String(k + 1);
            page(t, {pe(b + LFO_PHASE, "Phase"), pe(b + LFO_SLEW, "Slew"), pe(b + LFO_TRIG, "MonoTrig")});
            page(t, {pe(b + LFO_FMODE, "FadeMode"), pe(b + LFO_FSYNC, "FadeSync"), pe(b + LFO_REP, "Repeats")});
            page(t, {pe(b + LFO_COMMON, "Common"), pe(b + LFO_SYNC, "RateSync")});
        }
        for (int k = 0; k < 2; k++) {
            int b = k ? P_LFO4 : P_LFO3;
            juce::String t = "LFO " + juce::String(k + 3);
            page(t, {pe(b + L34_WAVE, "Waveform"), pe(b + L34_RATE, "Rate"), pe(b + L34_SYNC, "RateSync")});
            page(t, {pe(b + L34_PHASE, "Phase"), pe(b + L34_SLEW, "Slew"), pe(b + L34_FADE, "FadeTime")});
        }
        break;
    case M_ARP:
        page("CLOCK", {pe(P_BPM, "ClockRate"), gl("host_sync", "Host Sync")});
        page("ARP", {pe(P_ARP_TYPE, "Type"), pe(P_ARP_RHYTHM, "Rhythm"), pe(P_ARP_OCT, "Octaves")});
        page("ARP", {pe(P_ARP_SWING, "Swing"), pe(P_ARP_RATE, "SyncRate"), pe(P_ARP_KEYSYNC, "KeySync")});
        page("ARP", {pe(P_ARP_CHANCE, "Chance")});
        break;
    case M_MOD:
        for (int k = 0; k < 16; k++) {
            int b = P_MOD1 + k * MOD_N;
            Page pg;
            pg.title = "MOD MATRIX " + juce::String(k + 1);
            pg.mod = true;
            pg.modBase = b;
            Entry src;
            src.label = "Source";
            src.lo = 0;
            src.hi = NMODSRC - 1;
            src.get = [self, b] { return self->proc.value(b + (self->modAB ? MOD_SB : MOD_SA)); };
            src.set = [self, b](int v) { self->proc.setValue(b + (self->modAB ? MOD_SB : MOD_SA), v); };
            src.text = [](int v) { return juce::String(MODSRC_NAME[juce::jlimit(0, NMODSRC - 1, v)]); };
            pg.rows = {src, pe(b + MOD_DST, "Destin"), pe(b + MOD_DEPTH, "Depth")};
            ps.push_back(pg);
        }
        break;
    case M_VOICE:
        page("VOICE", {pe(P_UNISON, "Unison"), pe(P_UNIDET, "UniDetune"), pe(P_UNISPREAD, "Spread")});
        page("VOICE", {pe(P_PREGLIDE, "PreGlide"), pe(P_MODE, "Mode"), pe(P_PATCHLVL, "PatchLevel")});
        page("VOICE", {pe(P_POSTDRV, "FltPostDrv"), pe(P_FDIVERGE, "FltDiverge")});
        page("VOICE", {pe(P_SPREADMODE, "SpreadMode"), pe(P_PAN, "PanPosn")});
        break;
    case M_FX:
        page("FX GLOBAL", {pe(P_FX_WET, "WetLevel"), pe(P_FX_DRY, "DryLevel"), pe(P_FX_ROUTE, "Routing")});
        page("CHORUS", {pe(P_CH_DEPTH, "ChorDepth"), pe(P_CH_FB, "ChorFback"), pe(P_CH_MODE, "Mode")});
        page("CHORUS", {pe(P_CH_LP, "LoPass"), pe(P_CH_HP, "HiPass")});
        page("DELAY", {pe(P_DL_SYNCRATE, "DelaySync"), pe(P_DL_LPD, "LP Damp"), pe(P_DL_HPD, "HP Damp")});
        page("DELAY", {pe(P_DL_RATIO, "L/R Ratio"), pe(P_DL_SLEW, "SlewRate"), pe(P_DL_WIDTH, "Width")});
        page("DELAY", {pe(P_DL_TMODE, "TimeMode")});
        page("REVERB", {pe(P_RV_PRE, "PreDelay"), pe(P_RV_LPD, "LP Damp"), pe(P_RV_HPD, "HP Damp")});
        page("REVERB", {pe(P_RV_SIZE, "RevSize"), pe(P_RV_MDEPTH, "ModDepth"), pe(P_RV_MRATE, "ModRate")});
        page("REVERB", {pe(P_RV_LP, "LoPass"), pe(P_RV_HP, "HiPass")});
        for (int k = 0; k < 4; k++) {
            int b = P_FXMOD1 + k * MOD_N;
            Page pg;
            pg.title = "FX MOD " + juce::String(k + 1);
            pg.mod = true;
            pg.modBase = b;
            Entry src;
            src.label = "Source";
            src.lo = 0;
            src.hi = NMODSRC - 1;
            src.get = [self, b] { return self->proc.value(b + (self->modAB ? MOD_SB : MOD_SA)); };
            src.set = [self, b](int v) { self->proc.setValue(b + (self->modAB ? MOD_SB : MOD_SA), v); };
            src.text = [](int v) { return juce::String(MODSRC_NAME[juce::jlimit(0, NMODSRC - 1, v)]); };
            pg.rows = {src, pe(b + MOD_DST, "FxDestin"), pe(b + MOD_DEPTH, "Depth")};
            ps.push_back(pg);
        }
        break;
    case M_SETTINGS: {
        Entry win;
        win.label = "Window";
        win.lo = 5;
        win.hi = 16;
        win.get = [self] { return (int)std::lround(self->proc.uiScale.load() * 10.0f); };
        win.set = [self](int v) {
            if (auto *ed = self->findParentComponentOfClass<PeekEditor>())
                ed->setSize((int)std::lround(W * v / 10.0), (int)std::lround(H * v / 10.0));
        };
        win.text = [](int v) { return juce::String(v * 10) + " %"; };
        Entry panic;
        panic.label = "Panic";
        panic.lo = 0;
        panic.hi = 1;
        panic.get = [] { return 0; };
        panic.set = [self](int) { self->proc.command(PeekProcessor::C_PANIC); };
        panic.text = [](int) { return juce::String("turn"); };
        page("SYSTEM", {gl("voices", "Voices"), gl("quality", "Quality"), gl("host_sync", "Host Sync")});
        page("SYNTH", {gl("transpose", "Transpose"), win, panic});
        break;
    }
    default: break;
    }
    return ps;
}

Panel::Page Panel::page() const
{
    auto ps = pages(menu);
    if (ps.empty())
        return {};
    return ps[(size_t)juce::jlimit(0, (int)ps.size() - 1, pageOf[menu])];
}

void Panel::openMenu(Menu m)
{
    if (menu == m && !showingTouch())
        pageStep(1);                                 /* (pressed again: the next page) */
    menu = m;
    shown = -1;
    shownGlobal = nullptr;
    row = juce::jlimit(0, juce::jmax(0, (int)page().rows.size() - 1), row);
}

void Panel::pageStep(int d)
{
    shown = -1;
    shownGlobal = nullptr;
    int n = (int)pages(menu).size();
    if (n == 0)
        return;
    pageOf[menu] = ((pageOf[menu] + d) % n + n) % n;
    row = juce::jlimit(0, juce::jmax(0, (int)page().rows.size() - 1), row);
}

void Panel::valueStep(int d)
{
    if (showingTouch()) {
        if (shownGlobal) {
            shownGlobal->beginChangeGesture();
            if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(shownGlobal))
                shownGlobal->setValueNotifyingHost(shownGlobal->convertTo0to1((float)(ip->get() + d)));
            else
                shownGlobal->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, shownGlobal->getValue() + 0.01f * (float)d));
            shownGlobal->endChangeGesture();
            shownAt = juce::Time::getMillisecondCounterHiRes();
        } else if (shown >= 0) {
            proc.setValue(shown, proc.value(shown) + d);
            shownAt = juce::Time::getMillisecondCounterHiRes();
        }
        return;
    }
    auto pg = page();
    if (row < 0 || row >= (int)pg.rows.size())
        return;
    auto &e = pg.rows[(size_t)row];
    e.set(juce::jlimit(e.lo, e.hi, e.get() + d));
}

void Panel::rowMenu(int r, juce::Rectangle<int> screen)
{
    auto pg = page();
    if (r < 0 || r >= (int)pg.rows.size())
        return;
    auto e = pg.rows[(size_t)r];
    if (e.hi <= e.lo)
        return;
    juce::PopupMenu m;
    m.addSectionHeader(pg.mod && r == 0 ? juce::String(modAB ? "Source B" : "Source A") : e.label);
    int cur = e.get();
    if (e.hi - e.lo > 64) {
        for (int g0 = e.lo; g0 <= e.hi; g0 += 32) {
            juce::PopupMenu sub;
            int g1 = juce::jmin(e.hi, g0 + 31);
            for (int v = g0; v <= g1; v++)
                sub.addItem(v - e.lo + 1, e.text ? e.text(v) : juce::String(v), true, v == cur);
            m.addSubMenu((e.text ? e.text(g0) : juce::String(g0)) + "  ..  " + (e.text ? e.text(g1) : juce::String(g1)), sub, true,
                         nullptr, cur >= g0 && cur <= g1);
        }
    } else
        for (int v = e.lo; v <= e.hi; v++)
            m.addItem(v - e.lo + 1, e.text ? e.text(v) : juce::String(v), true, v == cur);
    juce::Component::SafePointer<Panel> self(this);
    int ab = modAB;
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screen).withMinimumWidth(200), [self, e, ab](int res) {
        if (self && res > 0) {
            self->modAB = ab;
            e.set(e.lo + res - 1);
        }
    });
}

void Panel::patchMenu(juce::Rectangle<int> screen)
{
    juce::PopupMenu m;
    m.addSectionHeader("FACTORY");
    for (int i = 0; i < NFACTORY; i++)
        m.addItem(i + 1, "F" + juce::String(i + 1).paddedLeft('0', 3) + "   " + proc.getProgramName(i), true, i == proc.getCurrentProgram());
    m.addSectionHeader("YOUR PATCHES");
    for (int i = NFACTORY; i < proc.getNumPrograms(); i++)
        m.addItem(i + 1, "U" + juce::String(i - NFACTORY + 1).paddedLeft('0', 3) + "   " + proc.getProgramName(i), true,
                  i == proc.getCurrentProgram());
    if (proc.userPatches.empty())
        m.addItem(-1, "(SAVE keeps the sound as a patch of your own)", false, false);
    juce::Component::SafePointer<Panel> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screen).withMinimumWidth(260), [self](int r) {
        if (self && r > 0) {
            self->proc.setCurrentProgram(r - 1);
            self->shown = -1;
            self->shownGlobal = nullptr;
        }
    });
}

void Panel::changePatch(int d)
{
    int n = proc.getNumPrograms();
    proc.setCurrentProgram(((proc.getCurrentProgram() + d) % n + n) % n);
    shown = -1;
    shownGlobal = nullptr;
    menu = M_PATCH;
}

void Panel::saveDialog()
{
    auto *w = new juce::AlertWindow("SAVE", "Keep the sound as a patch of your own:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&look);
    w->addTextEditor("name", proc.patchName(), "Name");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Panel> self(this);
    dialog = w;
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
                           if (self && r == 1)
                               self->proc.savePatch(w->getTextEditorContents("name"));
                       }),
                       true);
}

/* ------------------------------------------------------------------------------- the build --- */
void Panel::build()
{
    using juce::Rectangle;
    auto P = [this](int p) { return [this, p]() -> juce::RangedAudioParameter * { return proc.param(p); }; };
    auto touchP = [this](int p) { return [this, p](juce::RangedAudioParameter *) { touch(p); }; };
    auto touchAny = [this]() {
        return [this](juce::RangedAudioParameter *q) {
            for (int i = 0; i < NPARAM; i++)
                if (proc.param(i) == q) {
                    touch(i);
                    return;
                }
            touchGlobal(q);
        };
    };
    auto place = [](juce::Component *c, float px, float py, float w, float h) {
        c->setBounds(Rectangle<float>(w, h).withCentre(at(px, py)).toNearestInt());
    };
    auto knob = [&](float px, float py, int p) {
        auto *k = add<Knob>(P(p), touchP(p));
        place(k, px, py, KD, KD);
        return k;
    };
    auto dknob = [&](float px, float py, Knob::Target t, float size = KD, bool big = false) {   /* (a knob whose target moves) */
        auto *k = add<Knob>(std::move(t), touchAny(), big);
        place(k, px, py, size, size);
        return k;
    };
    auto button = [&](float px, float py, BtnStyle s, std::function<void()> press, std::function<bool()> on, float w = 52.0f,
                      float h = 26.0f) {
        auto *b = add<Btn>(s, std::move(press), std::move(on));
        place(b, px, py, w, h);
        return b;
    };
    auto leds = [&](std::vector<juce::Point<float>> pts, std::function<float(int)> lv, std::function<void(int)> pick = {}) {
        std::vector<juce::Point<float>> q;
        for (auto &p : pts)
            q.push_back(at(p.x, p.y));
        return add<Leds>(q, 21.0f, std::move(lv), std::move(pick));
    };
    auto cycle = [this](int p, int n) {
        return [this, p, n] {
            proc.setValue(p, (proc.value(p) + 1) % n);
            touch(p);
        };
    };
    auto toggle = [this](int p) {
        return [this, p] {
            proc.setValue(p, proc.value(p) ? 0 : 1);
            touch(p);
        };
    };
    auto isOn = [this](int p) { return [this, p] { return proc.value(p) != 0; }; };
    auto is = [this](int p, int v) { return [this, p, v](int i) { return proc.value(p) == v + i ? 1.0f : 0.0f; }; };
    auto setTo = [this](int p, int base = 0) { return [this, p, base](int i) { proc.setValue(p, base + i); touch(p); }; };
    auto glob = [this](const char *id) { return proc.apvts.getParameter(id); };

    /* MASTER */
    {
        auto *k = add<Knob>([glob] { return glob("volume"); }, [this](juce::RangedAudioParameter *q) { touchGlobal(q); });
        place(k, 80, 150, KD, KD);
    }
    /* ANIMATE: 1 and 2 (held: on), HOLD */
    for (int k = 0; k < 2; k++) {
        auto *b = add<Btn>(BtnStyle::pale, [this, k] { proc.command(PeekProcessor::C_ANIMATE, k, 1); },
                           [this, k] { return proc.animLed[k].load(); }, [this, k] { proc.command(PeekProcessor::C_ANIMATE, k, 0); });
        b->text = juce::String(k + 1);
        place(b, 68, k ? 250.0f : 225.0f, 44, 40);
    }
    button(68, 271, BtnStyle::rubber, [this] { proc.command(PeekProcessor::C_HOLD, 0, proc.holdLed.load() ? 0 : 1); },
           [this] { return proc.holdLed.load(); }, 40, 16);
    /* PATCH: Initialise, Compare, Audition, Save; the patch knob, - and + */
    button(135, 137, BtnStyle::rubber, [this] { proc.initialise(); menu = M_PATCH; }, [] { return false; });
    button(170, 137, BtnStyle::rubber, [this] { proc.setCompare(!proc.compare); }, [this] { return proc.compare; });
    {
        auto *b = add<Btn>(BtnStyle::rubber, [this] { proc.command(PeekProcessor::C_AUDITION, 0, 1); }, [] { return false; },
                           [this] { proc.command(PeekProcessor::C_AUDITION, 0, 0); });
        place(b, 135, 161, 52, 26);
    }
    button(170, 161, BtnStyle::rubber, [this] { saveDialog(); }, [] { return false; });
    {
        auto *k = add<Knob>([this](int d) { changePatch(d); });
        place(k, 150, 205, KD, KD);
    }
    button(130, 252, BtnStyle::rubber, [this] { changePatch(-1); }, [] { return false; });
    button(168, 252, BtnStyle::rubber, [this] { changePatch(1); }, [] { return false; });
    /* MENU: the menu buttons */
    {
        button(207, 160, BtnStyle::pale, [this] { openMenu(M_PATCH); }, [this] { return menu == M_PATCH && !showingTouch(); }, 56, 26);
        const Menu MS[8] = {M_OSC, M_ENV, M_LFO, M_ARP, M_MOD, M_VOICE, M_FX, M_SETTINGS};
        for (int i = 0; i < 8; i++) {
            float px = 262.0f + 38.0f * (float)(i % 4), py = i < 4 ? 133.0f : 158.0f;
            Menu m = MS[i];
            button(px, py, BtnStyle::pale, [this, m] { openMenu(m); }, [this, m] { return menu == m && !showingTouch(); }, 56, 26);
        }
    }
    /* the display, its row buttons, the data knob, PAGE, VALUE */
    {
        for (int i = 0; i < 3; i++)
            button(206, 200.0f + 19.0f * (float)i, BtnStyle::rubber, [this, i] {
                if (page().mod && i == 0 && row == 0)
                    modAB ^= 1;                      /* (the source row's button: A, then B) */
                row = i;
                shown = -1;
                shownGlobal = nullptr;
            }, [this, i] { return row == i && !showingTouch(); }, 50, 24);
        auto *o = add<Oled>(*this);
        o->setBounds(R(226, 191, 372, 255).toNearestInt());
        oledView = o;
        auto *k = add<Knob>([this](int d) { valueStep(d); });
        place(k, 412, 216, 70, 70);
        button(285, 266, BtnStyle::pale, [this] { pageStep(-1); }, [] { return false; }, 56, 24);
        button(320, 266, BtnStyle::pale, [this] { pageStep(1); }, [] { return false; }, 56, 24);
        button(420, 266, BtnStyle::rubber, [this] { valueStep(-1); }, [] { return false; }, 50, 24);
        button(452, 266, BtnStyle::rubber, [this] { valueStep(1); }, [] { return false; }, 50, 24);
    }
    /* ARP */
    knob(487, 165, P_ARP_GATE);
    button(487, 228, BtnStyle::pale, toggle(P_ARP_LATCH), isOn(P_ARP_LATCH), 56, 26);
    button(487, 265, BtnStyle::pale, toggle(P_ARP_ON), isOn(P_ARP_ON), 56, 26);
    /* LFO 1, LFO 2 */
    for (int k = 0; k < 2; k++) {
        int b = k ? P_LFO2 : P_LFO1;
        float ox = k ? 222.0f : 0.0f;
        leds({{527 + ox, 130}, {527 + ox, 142}, {527 + ox, 154}, {527 + ox, 166}}, is(b + LFO_TYPE, 0), setTo(b + LFO_TYPE));
        button(538 + ox, 181, BtnStyle::rubber, cycle(b + LFO_TYPE, NLFOTYPE), [] { return false; });
        knob(592 + ox, 158, b + LFO_FADE);
        leds({{636 + ox, 135}, {636 + ox, 147}, {636 + ox, 159}}, is(b + LFO_RANGE, 0), setTo(b + LFO_RANGE));
        button(650 + ox, 181, BtnStyle::rubber, cycle(b + LFO_RANGE, NLFORANGE), [] { return false; });
        dknob(706 + ox, 158, [this, b]() -> juce::RangedAudioParameter * {
            return proc.param(proc.value(b + LFO_RANGE) == LR_SYNC ? b + LFO_SYNC : b + LFO_RATE);
        });
    }
    /* GLIDE */
    knob(540, 236, P_GLIDE);
    button(540, 282, BtnStyle::pale, toggle(P_GLIDE_ON), isOn(P_GLIDE_ON), 56, 26);
    /* AMP ENVELOPE, MOD ENVELOPES */
    {
        const int E[4] = {ENV_ATT, ENV_DEC, ENV_SUS, ENV_REL};
        for (int i = 0; i < 4; i++) {
            int p = P_AENV + E[i];
            auto *f = add<Fader>(P(p), touchP(p));
            f->setBounds(Rectangle<float>(60.0f, Y(289) - Y(217)).withCentre(at(592.0f + 42.0f * (float)i, 253.0f)).toNearestInt());
        }
        for (int i = 0; i < 4; i++) {
            int e = E[i];
            auto *f = add<Fader>([this, e]() -> juce::RangedAudioParameter * { return proc.param((proc.value(P_MENV_SEL) ? P_M2ENV : P_M1ENV) + e); },
                                 touchAny());
            f->setBounds(Rectangle<float>(60.0f, Y(289) - Y(217)).withCentre(at(808.0f + 42.0f * (float)i, 253.0f)).toNearestInt());
        }
        leds({{757, 238}, {757, 250}}, is(P_MENV_SEL, 0), setTo(P_MENV_SEL));
        button(766, 276, BtnStyle::rubber, cycle(P_MENV_SEL, 2), [] { return false; });
    }
    /* OSCILLATOR 1..3 */
    for (int k = 0; k < 3; k++) {
        int b = P_OSC1 + k * OSC_N;
        float oy = 80.0f * (float)k;
        /* RANGE: 16' 4' over 8' 2' */
        leds({{64, 330 + oy}, {64, 342 + oy}, {90, 330 + oy}, {90, 342 + oy}},
             [this, b](int i) { static const int M[4] = {0, 1, 2, 3}; return proc.value(b + OSC_RANGE) == M[i] ? 1.0f : 0.0f; },
             setTo(b + OSC_RANGE));
        button(78, 366 + oy, BtnStyle::rubber, cycle(b + OSC_RANGE, 4), [] { return false; });
        knob(125, 343 + oy, b + OSC_COARSE);
        knob(181, 343 + oy, b + OSC_FINE);
        /* WAVE: sine, square / triangle, more / sawtooth */
        leds({{216, 326 + oy}, {241, 326 + oy}, {216, 338 + oy}, {241, 338 + oy}, {216, 350 + oy}},
             [this, b](int i) { static const int M[5] = {W_SINE, W_SQUARE, W_TRI, W_MORE, W_SAW}; return proc.value(b + OSC_WAVE) == M[i] ? 1.0f : 0.0f; },
             [this, b](int i) { static const int M[5] = {W_SINE, W_SQUARE, W_TRI, W_MORE, W_SAW}; proc.setValue(b + OSC_WAVE, M[i]); touch(b + OSC_WAVE); });
        button(230, 366 + oy, BtnStyle::rubber, [this, b] {
            static const int NEXT[NWAVE] = {W_TRI, W_SAW, W_SQUARE, W_MORE, W_SINE};
            proc.setValue(b + OSC_WAVE, NEXT[proc.value(b + OSC_WAVE)]);
            touch(proc.value(b + OSC_WAVE) == W_MORE ? b + OSC_MORE : b + OSC_WAVE);
        }, [] { return false; });
        knob(277, 343 + oy, b + OSC_ENV2);
        knob(333, 343 + oy, b + OSC_LFO2);
        dknob(389, 343 + oy, [this, b]() -> juce::RangedAudioParameter * {
            int s = proc.value(b + OSC_SHSRC);
            return proc.param(b + (s == SS_ENV1 ? OSC_SHENV : s == SS_LFO1 ? OSC_SHLFO : OSC_SHMAN));
        });
        leds({{421, 326 + oy}, {421, 338 + oy}, {421, 350 + oy}}, [this, b](int i) {
            int s = proc.value(b + OSC_SHSRC);
            if (s == i)
                return 1.0f;
            int amt = proc.value(b + (i == 0 ? OSC_SHMAN : i == 1 ? OSC_SHENV : OSC_SHLFO));
            return amt != 0 ? 0.25f : 0.0f;                  /* (a source in use, dimly) */
        }, setTo(b + OSC_SHSRC));
        button(428, 368 + oy, BtnStyle::rubber, cycle(b + OSC_SHSRC, NSHSRC), [] { return false; });
    }
    /* MIXER */
    knob(489, 343, P_LVL1);
    knob(544, 343, P_VCA_GAIN);
    knob(489, 425, P_LVL2);
    knob(544, 425, P_LVL_RING);
    knob(489, 506, P_LVL3);
    knob(544, 506, P_LVL_NOISE);
    /* FILTER */
    knob(598, 338, P_FLFO1);
    knob(654, 338, P_FOSC3);
    knob(709, 338, P_KEYTRK);
    dknob(598, 414, [this]() -> juce::RangedAudioParameter * { return proc.param(proc.value(P_FENV_SRC) ? P_FENV1 : P_FENVA); });
    knob(654, 414, P_RESO);
    knob(709, 414, P_DRIVE);
    leds({{586, 449}, {586, 460}}, [this](int i) {
        if (proc.value(P_FENV_SRC) == i)
            return 1.0f;
        return proc.value(i ? P_FENV1 : P_FENVA) != 0 ? 0.25f : 0.0f;
    }, setTo(P_FENV_SRC));
    button(598, 477, BtnStyle::rubber, cycle(P_FENV_SRC, 2), [] { return false; });
    leds({{586, 503}, {586, 514}}, is(P_SLOPE, 0), setTo(P_SLOPE));
    button(598, 526, BtnStyle::rubber, cycle(P_SLOPE, 2), [] { return false; });
    {
        auto *k = add<Knob>(P(P_FREQ), touchP(P_FREQ), true);
        place(k, 654, 500, 86, 86);
    }
    leds({{693, 490}, {693, 502}, {693, 514}}, is(P_FSHAPE, 0), setTo(P_FSHAPE));
    button(711, 526, BtnStyle::rubber, cycle(P_FSHAPE, NFSHAPE), [] { return false; });
    /* DISTORTION, CHORUS */
    knob(763, 343, P_DIST);
    knob(826, 343, P_CH_RATE);
    leds({{858, 326}, {858, 338}, {858, 350}}, is(P_CH_TYPE, 0), setTo(P_CH_TYPE));
    button(872, 368, BtnStyle::rubber, cycle(P_CH_TYPE, 3), [] { return false; });
    knob(930, 343, P_CH_LEVEL);
    /* DELAY */
    knob(766, 422, P_DL_FB);
    dknob(836, 422, [this]() -> juce::RangedAudioParameter * { return proc.param(proc.value(P_DL_SYNC) ? P_DL_SYNCRATE : P_DL_TIME); });
    button(882, 412, BtnStyle::pale, toggle(P_DL_SYNC), isOn(P_DL_SYNC), 56, 26);
    knob(930, 422, P_DL_LEVEL);
    /* EFFECTS: BYPASS */
    button(763, 500, BtnStyle::pale, toggle(P_FX_BYPASS), isOn(P_FX_BYPASS), 56, 26);
    /* REVERB: TYPE sets the size to 0, 64 or 127 */
    knob(826, 500, P_RV_TIME);
    leds({{858, 486}, {858, 498}, {858, 510}}, [this](int i) {
        int s = proc.value(P_RV_SIZE);
        int t = s < 32 ? 0 : s < 96 ? 1 : 2;
        return t == i ? 1.0f : 0.0f;
    }, [this](int i) { proc.setValue(P_RV_SIZE, i == 0 ? 0 : i == 1 ? 64 : 127); touch(P_RV_SIZE); });
    button(872, 528, BtnStyle::rubber, [this] {
        int s = proc.value(P_RV_SIZE);
        int t = s < 32 ? 0 : s < 96 ? 1 : 2;
        t = (t + 1) % 3;
        proc.setValue(P_RV_SIZE, t == 0 ? 0 : t == 1 ? 64 : 127);
        touch(P_RV_SIZE);
    }, [] { return false; });
    knob(930, 500, P_RV_LEVEL);

    /* PEEK's own, in the top strip: the voices (8 or 16), HOST SYNC */
    {
        button(560, 561, BtnStyle::rubber, [this, glob] {
            auto *q = glob("voices");
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->getValue() > 0.5f ? 0.0f : 1.0f);
            q->endChangeGesture();
            touchGlobal(q);
        }, [] { return false; }, 40, 18);
        leds({{582, 556}, {582, 566}}, [this](int i) { return (proc.apvts.getRawParameterValue("voices")->load() > 0.5f) == (i == 1) ? 1.0f : 0.0f; },
             [this, glob](int i) {
                 auto *q = glob("voices");
                 q->beginChangeGesture();
                 q->setValueNotifyingHost(i ? 1.0f : 0.0f);
                 q->endChangeGesture();
                 touchGlobal(q);
             });
        button(640, 561, BtnStyle::rubber, [this, glob] {
            auto *q = glob("host_sync");
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->getValue() > 0.5f ? 0.0f : 1.0f);
            q->endChangeGesture();
            touchGlobal(q);
        }, [this] { return proc.apvts.getRawParameterValue("host_sync")->load() > 0.5f; }, 40, 18);
    }
    /* the active voices: two rows of four (16 voices: each LED two voices, both lit as one) */
    {
        std::vector<juce::Point<float>> pts;
        for (int i = 0; i < 8; i++)
            pts.push_back(at(407.3f + 13.2f * (float)(i % 4), i < 4 ? 148.0f : 159.0f));
        auto *act = add<Leds>(pts, 17.0f, [this](int i) {
            bool sixteen = proc.apvts.getRawParameterValue("voices")->load() > 0.5f;
            float a = proc.voiceLevel[i].load();
            if (sixteen)
                a = juce::jmax(a, proc.voiceLevel[i + 8].load());
            return std::sqrt(a);
        });
        (void)act;
    }
}

/* ------------------------------------------------------------------------------ painting --- */
void Panel::paintBackground(juce::Graphics &g, float sc)
{
    using namespace juce;
    auto all = Rectangle<float>(0.0f, 0.0f, (float)W, (float)H);
    const float cheek = 38.0f;
    auto face = all.reduced(cheek, 0.0f);
    drawPanelFace(g, face, sc);
    drawWood(g, all.withWidth(cheek), 3);
    drawWood(g, all.withLeft((float)W - cheek), 4);
    for (float x : {cheek, (float)W - cheek}) {
        bool left = x < 100.0f;
        g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.6f), x, 0.0f, Colours::transparentBlack, x + (left ? 12.0f : -12.0f), 0.0f, false));
        g.fillRect(left ? Rectangle<float>(x, 0.0f, 12.0f, (float)H) : Rectangle<float>(x - 12.0f, 0.0f, 12.0f, (float)H));
    }
    /* the top strip: the maker's tag left, PEEK right */
    drawTag(g, R(50, 98, 135, 111));
    {
        auto f = font(Face::logo, 60.0f);
        GlyphArrangement ga;
        ga.addLineOfText(f, "PEEK", 0.0f, 0.0f);
        Path p;
        ga.createPath(p);
        auto b = p.getBounds();
        auto dst = R(890, 98.5f, 955, 110.5f);
        p.applyTransform(AffineTransform::translation(-b.getX(), -b.getY()).scaled(dst.getWidth() / b.getWidth(), dst.getHeight() / b.getHeight())
                             .translated(dst.getX(), dst.getY()));
        g.setColour(col::legend);
        g.fillPath(p);
        g.setColour(col::blue);
        g.fillRect(Rectangle<float>(dst.getX(), dst.getBottom() + 3.0f, dst.getWidth(), 2.0f));
    }
    auto small = [&](Rectangle<float> r, const String &t, Justification j = Justification::centredLeft, float size = LS) {
        g.setFont(font(Face::label, size));
        g.setColour(col::legend);
        g.drawText(t, r, j, false);
    };
    small(R(590, 551, 610, 561), "8 VOICE", Justification::centredLeft, 10.0f);
    small(R(590, 561, 610, 571), "16 VOICE", Justification::centredLeft, 10.0f);
    small(R(532, 556, 548, 566), "VOICES", Justification::centredRight, 10.0f);
    small(R(651, 556, 690, 566), "HOST SYNC", Justification::centredLeft, 10.0f);

    /* the sections */
    struct Sec {
        float x0, y0, x1, y1;
        const char *t;
        bool arrow;
    };
    const Sec S[] = {{48, 114, 110, 200, "MASTER", false},     {48, 203, 110, 287, "ANIMATE", false},
                     {112, 114, 186, 287, "PATCH", false},     {188, 114, 460, 287, "MENU", false},
                     {462, 114, 510, 303, "ARP", false},       {512, 114, 735, 200, "LFO 1", false},
                     {737, 114, 957, 200, "LFO 2", false},     {512, 203, 570, 303, "GLIDE", false},
                     {572, 203, 735, 303, "AMP ENVELOPE", false}, {737, 203, 957, 303, "MOD ENVELOPES", false},
                     {48, 306, 460, 384, "OSCILLATOR 1", true}, {48, 386, 460, 464, "OSCILLATOR 2", true},
                     {48, 466, 460, 545, "OSCILLATOR 3", true}, {462, 306, 570, 545, "MIXER", false},
                     {572, 306, 735, 545, "FILTER", true},     {737, 306, 790, 384, "DISTORTION", false},
                     {792, 306, 957, 384, "CHORUS", false},    {737, 386, 957, 464, "DELAY", false},
                     {737, 466, 790, 545, "EFFECTS", false},   {792, 466, 957, 545, "REVERB", false}};
    for (auto &s : S)
        drawSection(g, R(s.x0, s.y0, s.x1, s.y1), s.t, s.arrow);
    /* VCA GAIN's own box inside the mixer */
    {
        auto r = R(516, 306, 570, 377);
        g.setColour(Colours::black.withAlpha(0.6f));
        g.drawRect(r.withTrimmedTop(4.0f), 1.4f);
        Path p;
        float x = r.getRight() - 16.0f, y = r.getY() + 9.0f;
        p.addTriangle(x, y, x, y + 12.0f, x + 10.0f, y + 6.0f);
        g.setColour(col::blue);
        g.fillPath(p);
    }

    auto under = [&](float px, float py, const String &t, float d = KD) {
        auto c = at(px, py);
        drawLegend(g, t, Rectangle<float>(c.x - 60.0f, c.y + d * 0.5f + 1.0f, 120.0f, 16.0f), LS + 1.5f);
    };
    auto underB = [&](float px, float py, const String &t) {
        auto c = at(px, py);
        drawLegend(g, t, Rectangle<float>(c.x - 60.0f, c.y + 13.0f, 120.0f, 16.0f), LS + 1.5f);
    };
    /* a bracket under two or three controls with its word in the middle (PITCH, SOURCE, RANGE, TIME) */
    auto bracket = [&](float pxA, float pxB, float py, const String &t, bool hookB = true) {
        float y = Y(py), xa = X(pxA), xb = X(pxB);
        auto f = font(Face::label, LS + 1.5f);
        float tw = textWidth(f, t);
        float cx = 0.5f * (xa + xb);
        g.setColour(col::legend);
        g.fillRect(Rectangle<float>(xa, y, cx - tw * 0.5f - 6.0f - xa, 1.3f));
        g.fillRect(Rectangle<float>(cx + tw * 0.5f + 6.0f, y, xb - (cx + tw * 0.5f + 6.0f), 1.3f));
        g.fillRect(Rectangle<float>(xa, y - 7.0f, 1.3f, 7.0f));
        if (hookB)
            g.fillRect(Rectangle<float>(xb - 1.3f, y - 7.0f, 1.3f, 7.0f));
        g.setFont(f);
        g.drawText(t, Rectangle<float>(cx - tw * 0.5f - 4.0f, y - 9.0f, tw + 8.0f, 18.0f), Justification::centred, false);
    };
    under(80, 150, "Volume");
    /* PATCH */
    underB(135, 137, "Initialise");
    underB(170, 137, "Compare");
    underB(135, 161, "Audition");
    underB(170, 161, "Save");
    small(R(124, 262, 136, 274), "-", Justification::centred, 14.0f);
    small(R(162, 262, 174, 274), "+", Justification::centred, 14.0f);
    small(R(136, 262, 162, 274), "Patch", Justification::centred);
    small(R(58, 275.5f, 78, 283.5f), "Hold", Justification::centred, 9.0f);
    /* MENU */
    underB(207, 160, "Patch");
    {
        const char *MN[8] = {"Osc", "Env", "LFO", "Arp/Clock", "Mod", "Voice", "FX", "Settings"};
        for (int i = 0; i < 8; i++)
            underB(262.0f + 38.0f * (float)(i % 4), i < 4 ? 133.0f : 158.0f, MN[i]);
        for (int i = 0; i < 4; i++) {
            small(Rectangle<float>(X(404.0f + 13.2f * (float)i + 3.3f) - 8.0f, Y(140) - 6.0f, 16.0f, 12.0f), String(i + 1), Justification::centred, 10.0f);
            small(Rectangle<float>(X(404.0f + 13.2f * (float)i + 3.3f) - 8.0f, Y(167) - 6.0f, 16.0f, 12.0f), String(i + 5), Justification::centred, 10.0f);
        }
        small(R(398, 171, 460, 181), "Active Voices", Justification::centred, 10.0f);
    }
    small(R(270, 272.5f, 335, 284.5f), String::fromUTF8("\xe2\x97\x80  Page/Select  \xe2\x96\xb6"), Justification::centred);
    small(R(410, 272.5f, 462, 284.5f), "-   Value   +", Justification::centred);
    /* ARP */
    under(487, 165, "Gate");
    underB(487, 228, "Key Latch");
    underB(487, 265, "On");
    /* LFOs */
    for (int k = 0; k < 2; k++) {
        float ox = k ? 222.0f : 0.0f;
        const Sym SY[4] = {Sym::tri, Sym::rampDown, Sym::square, Sym::sh};
        for (int i = 0; i < 3; i++)
            drawSym(g, SY[i], Rectangle<float>(X(535 + ox), Y(130 + 12.0f * (float)i) - 5.0f, 14.0f, 10.0f));
        small(Rectangle<float>(X(534 + ox), Y(166) - 7.0f, 40.0f, 14.0f), "S+H");
        underB(538 + ox, 181, "Type");
        under(592 + ox, 158, "Fade Time");
        small(Rectangle<float>(X(643 + ox), Y(135) - 7.0f, 40.0f, 14.0f), "Low");
        small(Rectangle<float>(X(643 + ox), Y(147) - 7.0f, 40.0f, 14.0f), "High");
        small(Rectangle<float>(X(643 + ox), Y(159) - 7.0f, 40.0f, 14.0f), "Sync");
        under(706 + ox, 158, "Rate");
        bracket(650 + ox, 706 + ox, 193, "Range", true);
    }
    /* GLIDE */
    under(540, 236, "Time");
    underB(540, 282, "On");
    /* ENVELOPES */
    {
        const char *EN[4] = {"Attack", "Decay", "Sustain", "Release"};
        for (int i = 0; i < 4; i++) {
            small(Rectangle<float>(X(592.0f + 42.0f * (float)i) - 40.0f, Y(290), 80.0f, 16.0f), EN[i], Justification::centred, LS + 1.5f);
            small(Rectangle<float>(X(808.0f + 42.0f * (float)i) - 40.0f, Y(290), 80.0f, 16.0f), EN[i], Justification::centred, LS + 1.5f);
        }
        small(Rectangle<float>(X(765), Y(238) - 7.0f, 20.0f, 14.0f), "1");
        small(Rectangle<float>(X(765), Y(250) - 7.0f, 20.0f, 14.0f), "2");
        underB(766, 276, "Select");
    }
    /* OSCILLATORS */
    for (int k = 0; k < 3; k++) {
        float oy = 80.0f * (float)k;
        small(Rectangle<float>(X(72), Y(330 + oy) - 7.0f, 30.0f, 14.0f), "16'");
        small(Rectangle<float>(X(72), Y(342 + oy) - 7.0f, 30.0f, 14.0f), "8'");
        small(Rectangle<float>(X(98), Y(330 + oy) - 7.0f, 30.0f, 14.0f), "4'");
        small(Rectangle<float>(X(98), Y(342 + oy) - 7.0f, 30.0f, 14.0f), "2'");
        underB(78, 366 + oy, "Range");
        under(125, 343 + oy, "Coarse");
        under(181, 343 + oy, "Fine");
        drawSym(g, Sym::sine, Rectangle<float>(X(223), Y(326 + oy) - 5.0f, 14.0f, 10.0f));
        drawSym(g, Sym::square, Rectangle<float>(X(248), Y(326 + oy) - 5.0f, 14.0f, 10.0f));
        drawSym(g, Sym::tri, Rectangle<float>(X(223), Y(338 + oy) - 5.0f, 14.0f, 10.0f));
        small(Rectangle<float>(X(248), Y(338 + oy) - 7.0f, 40.0f, 14.0f), "more", Justification::centredLeft, 10.0f);
        drawSym(g, Sym::saw, Rectangle<float>(X(223), Y(350 + oy) - 5.0f, 14.0f, 10.0f));
        underB(230, 366 + oy, "Wave");
        under(277, 343 + oy, "Mod Env 2 Depth");
        under(333, 343 + oy, "LFO 2 Depth");
        under(389, 343 + oy, "Shape Amount");
        small(Rectangle<float>(X(429), Y(326 + oy) - 7.0f, 70.0f, 14.0f), "Manual");
        small(Rectangle<float>(X(429), Y(338 + oy) - 7.0f, 70.0f, 14.0f), "Mod Env 1");
        small(Rectangle<float>(X(429), Y(350 + oy) - 7.0f, 70.0f, 14.0f), "LFO 1");
        bracket(277, 333, 377 + oy, "Pitch");
        bracket(389, 428, 377 + oy, "Source", false);
    }
    /* MIXER */
    under(489, 343, "Osc 1");
    under(544, 343, "VCA Gain");
    under(489, 425, "Osc 2");
    under(544, 425, "Ring 1*2");
    under(489, 506, "Osc 3");
    under(544, 506, "Noise");
    /* FILTER */
    under(598, 338, "LFO 1 Depth");
    under(654, 338, "Osc 3 Filter Mod");
    under(709, 338, "Key Tracking");
    under(598, 414, "Env Depth");
    under(654, 414, "Resonance");
    under(709, 414, "Overdrive");
    small(Rectangle<float>(X(594), Y(449) - 7.0f, 70.0f, 14.0f), "Amp Env");
    small(Rectangle<float>(X(594), Y(460) - 7.0f, 70.0f, 14.0f), "Mod Env 1");
    underB(598, 477, "Source");
    small(Rectangle<float>(X(594), Y(503) - 7.0f, 70.0f, 14.0f), "12 dB");
    small(Rectangle<float>(X(594), Y(514) - 7.0f, 70.0f, 14.0f), "24 dB");
    underB(598, 526, "Slope");
    under(654, 500, "Frequency", 86.0f);
    small(Rectangle<float>(X(700), Y(490) - 7.0f, 40.0f, 14.0f), "LP");
    small(Rectangle<float>(X(700), Y(502) - 7.0f, 40.0f, 14.0f), "BP");
    small(Rectangle<float>(X(700), Y(514) - 7.0f, 40.0f, 14.0f), "HP");
    underB(711, 526, "Shape");
    /* EFFECTS */
    under(763, 343, "Level");
    under(826, 343, "Rate");
    for (int i = 0; i < 3; i++) {
        small(Rectangle<float>(X(866), Y(326 + 12.0f * (float)i) - 7.0f, 20.0f, 14.0f), String(i + 1));
        small(Rectangle<float>(X(866), Y(486 + 12.0f * (float)i) - 7.0f, 20.0f, 14.0f), String(i + 1));
    }
    underB(872, 368, "Type");
    under(930, 343, "Level");
    under(766, 422, "Feedback");
    under(836, 422, "Time");
    underB(882, 412, "Sync");
    under(930, 422, "Level");
    underB(763, 500, "Bypass");
    under(826, 500, "Time");
    underB(872, 528, "Type");
    under(930, 500, "Level");
    /* the bottom strip */
    small(R(52, 547, 300, 571), "NUMERICALLY CONTROLLED OSCILLATORS", Justification::centredLeft, 17.0f);
    /* the wear: gentle, round the knobs played most */
    drawWear(g, face, 1.0f / sc, {});
}

void Panel::paint(juce::Graphics &g)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (sc != backgroundScale || background.isNull()) {
        backgroundScale = sc;
        background = juce::Image(juce::Image::RGB, juce::jmax(1, (int)std::ceil(W * sc)), juce::jmax(1, (int)std::ceil(H * sc)), true);
        juce::Graphics bg(background);
        bg.addTransform(juce::AffineTransform::scale(sc));
        paintBackground(bg, sc);
    }
    g.drawImage(background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    /* the voices, in the bottom strip (8 or 16) */
    bool sixteen = proc.apvts.getRawParameterValue("voices")->load() > 0.5f;
    g.setFont(font(Face::title, 18.0f));
    g.setColour(col::legend);
    g.drawText(sixteen ? "SIXTEEN-VOICE POLYPHONIC SYNTHESISER" : "EIGHT-VOICE POLYPHONIC SYNTHESISER", R(700, 547, 955, 571),
               juce::Justification::centredRight, false);
}

void Panel::timerCallback()
{
    for (auto *l : live)
        l->poll();
    int v = proc.apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    if (v != shownVoices) {
        shownVoices = v;
        repaint(R(690, 545, 960, 575).toNearestInt());
    }
}

/* ============================================================================== the editor === */
PeekEditor::PeekEditor(PeekProcessor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, PeekPanel::W, PeekPanel::H);
    const double W = PeekPanel::W, H = PeekPanel::H;
    auto area = juce::Rectangle<int>(0, 0, 1920, 1080);
    if (auto *d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area = d->userArea;
    double fit = juce::jmin((area.getWidth() - 40) / W, (area.getHeight() - 90) / H);
    fit = juce::jlimit(0.45, 1.6, fit);
    setResizable(true, true);
    setResizeLimits((int)(W * 0.45), (int)(H * 0.45), (int)(W * juce::jmax(0.5, fit)), (int)(H * juce::jmax(0.5, fit)));
    getConstrainer()->setFixedAspectRatio(W / H);
    double s = proc.uiScaleSet ? (double)proc.uiScale.load() : juce::jmin(fit, 1.0);
    s = juce::jlimit(0.45, fit, s);
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void PeekEditor::resized()
{
    float s = (float)getWidth() / (float)PeekPanel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
    proc.uiScaleSet = true;
}
