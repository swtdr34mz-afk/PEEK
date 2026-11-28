/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* The editor: the Peak's top panel, drawn at 1720 x 880 and scaled to the window (drag the corner). As on the Peak:
 * MASTER, ANIMATE, PATCH, the MENU (its nine menu buttons, the OLED with its three row buttons, the data knob,
 * PAGE/SELECT and VALUE - / +, the active voices), ARP, LFO 1, LFO 2, GLIDE, the AMP and MOD ENVELOPES' sliders,
 * OSCILLATOR 1 to 3, the MIXER, the FILTER, DISTORTION, CHORUS, DELAY, EFFECTS, REVERB. PEEK's own, by its name: the
 * voices (8 or 16) and HOST SYNC. No keys: played from MIDI (the standalone app: a MIDI keyboard).
 *
 * The OLED works as the Peak's: it shows what was touched last for a moment; a menu button opens its pages, PAGE
 * steps through them, a row button picks the row the data knob and VALUE change. With the mouse: the wheel over the
 * display turns the data knob, a click on a row picks from its list. */
#pragma once
#include "PluginProcessor.h"
#include "ui/Look.h"

class PeekPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1720, H = 880;
    explicit PeekPanel(PeekProcessor &);
    ~PeekPanel() override;
    void paint(juce::Graphics &) override;

    PeekProcessor &proc;

    /* the menus and their pages */
    enum Menu { M_PATCH, M_OSC, M_ENV, M_LFO, M_ARP, M_MOD, M_VOICE, M_FX, M_SETTINGS, NMENU };
    struct Entry {
        juce::String label;
        std::function<int()> get;
        std::function<void(int)> set;
        int lo = 0, hi = 127;
        std::function<juce::String(int)> text;
        int param = -1;                              /* (a patch parameter's index, if it is one) */
    };
    struct Page {
        juce::String title;
        std::vector<Entry> rows;
        bool mod = false;                            /* a matrix slot's page (the first row: A / B) */
        int modBase = -1;
    };
    Menu menu = M_PATCH;
    int pageOf[NMENU] = {};
    int row = 0, modAB = 0;
    std::vector<Page> pages(Menu m) const;
    Page page() const;
    void openMenu(Menu m);
    void pageStep(int d);
    void valueStep(int d);
    void rowMenu(int r, juce::Rectangle<int> screen);
    void patchMenu(juce::Rectangle<int> screen);
    void saveDialog();
    void changePatch(int d);

    /* what was touched last (shown for a moment) */
    int shown = -1;
    juce::RangedAudioParameter *shownGlobal = nullptr;
    double shownAt = 0.0;
    void touch(int p);
    void touchGlobal(juce::RangedAudioParameter *);
    bool showingTouch() const;

    juce::Component *oledView = nullptr;
    int shownVoices = 8;

    /* the sections (the panel's own coordinates) and the section zoom: a click on a section's name or its magnifier */
    struct Section {
        juce::Rectangle<float> r;
        juce::String title;
        bool arrow;
    };
    std::vector<Section> sections;
    bool zoomIcons = true;               /* (the magnifiers on the names; off for the MPC skin's pictures) */
    std::function<void(int)> onSection;
    int tabAt(juce::Point<float>) const;
    void setZoom(juce::Rectangle<float> r);   /* the section shown large (empty: none): drawn sharp at its size */
    void mouseDown(const juce::MouseEvent &) override;
    void mouseMove(const juce::MouseEvent &) override;

    struct Live {
        virtual ~Live() = default;
        virtual void poll() = 0;
    };

private:
    void timerCallback() override;
    void build();
    void paintBackground(juce::Graphics &, float sc);

    pkui::Look look;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::vector<Live *> live;
    juce::Image background;
    float backgroundScale = 0.0f;
    juce::Rectangle<float> zoomR;        /* (zoomed: that part of the panel drawn again at the zoom's resolution) */
    juce::Image zoomCache;
    float zoomScale = 0.0f;
    juce::Component::SafePointer<juce::AlertWindow> dialog;
    template <class T, class... A> T *add(A &&...a);
    friend class Oled;
};

class PeekEditor : public juce::AudioProcessorEditor
{
public:
    explicit PeekEditor(PeekProcessor &);
    ~PeekEditor() override;
    void resized() override;
    bool sized = false;                      /* (resizes during the constructor keep nothing) */
    void zoomTo(int section);                /* the section zoom: a section as large as the window allows; -1: back */
    int zoomedSection() const { return zoomed; }

private:
    juce::AffineTransform panelTransform(int section) const;
    void animate();
    PeekProcessor &proc;
    juce::SharedResourcePointer<pkui::UiResources> resources;   // (before the panel: freed after it)
    PeekPanel panel;
    int zoomed = -1;
    juce::AffineTransform animFrom, animTo;
    double animStart = 0.0;
    struct Shade;
    std::unique_ptr<juce::Component> shade;
    struct Anim : juce::Timer {
        PeekEditor &ed;
        explicit Anim(PeekEditor &e) : ed(e) {}
        void timerCallback() override { ed.animate(); }
    } anim{*this};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeekEditor)
};
