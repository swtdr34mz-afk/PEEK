/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   PeekShot out.png [scale] [patch] [menu] [page] */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <typeinfo>

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String out = argc > 1 ? argv[1] : "shot.png";
    float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;
    int prog = argc > 3 ? std::atoi(argv[3]) : 0;
    PeekProcessor proc;
    proc.setCurrentProgram(prog);
    proc.uiScale = 1.0f;
    {
        /* the processor as a host drives it: prepare, blocks with notes, its state out and back in */
        proc.setPlayConfigDetails(0, 2, 48000.0, 512);
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        float peak = 0.0f;
        for (int b = 0; b < 200; b++) {
            juce::MidiBuffer m;
            if (b == 2)
                for (int n : {48, 55, 60, 64})
                    m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8)100), 10);
            if (b == 150)
                for (int n : {48, 55, 60, 64})
                    m.addEvent(juce::MidiMessage::noteOff(1, n), 0);
            buf.clear();
            proc.processBlock(buf, m);
            peak = juce::jmax(peak, buf.getMagnitude(0, 512));
        }
        juce::MemoryBlock st;
        proc.getStateInformation(st);
        proc.setStateInformation(st.getData(), (int)st.getSize());
        std::printf("processed: peak %.3f, state %d bytes\n", peak, (int)st.getSize());
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(PeekPanel::W, PeekPanel::H);
    if (auto *w = std::getenv("PEEK_W")) {       /* (a window of this width, e.g. a laptop's) */
        int wd = juce::String(w).getIntValue();
        ed->setSize(wd, (int)std::lround(wd * (double)PeekPanel::H / PeekPanel::W));
    }
    if (auto *z = std::getenv("PEEK_ZOOM"))     /* (a section shown large: its index) */
        if (auto *pe = dynamic_cast<PeekEditor *>(ed.get()))
            pe->zoomTo(juce::String(z).getIntValue());
    if (argc > 4)
        if (auto *panel = dynamic_cast<PeekPanel *>(ed->getChildComponent(0))) {
            panel->menu = (PeekPanel::Menu)std::atoi(argv[4]);
            panel->pageOf[panel->menu] = argc > 5 ? std::atoi(argv[5]) : 0;
        }
    for (auto *c : ed->getChildren())             /* (the live parts: their state, as the timer would) */
        for (auto *k : c->getChildren())
            if (auto *lv = dynamic_cast<PeekPanel::Live *>(k))
                lv->poll();
    if (std::getenv("PEEK_DUMP"))                 /* (every control's bounds, for the layout check) */
        if (auto *panel = ed->getChildComponent(0))
            for (auto *k : panel->getChildren()) {
                auto b = k->getBounds();
                std::printf("C %d %d %d %d %s\n", b.getX(), b.getY(), b.getRight(), b.getBottom(), typeid(*k).name());
            }
    if (std::getenv("PEEK_RCLICK")) {             /* (a right click and a right drag on every control: no value changes) */
        if (auto *panel = ed->getChildComponent(0)) {
            juce::MemoryBlock before, after;
            proc.getStateInformation(before);
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            int n = 0;
            bool left = juce::String(std::getenv("PEEK_RCLICK")) == "left";
            for (auto *k : panel->getChildren()) {
                if (left && n > 0)
                    break;                       /* (the check: the first control, VOLUME, dragged with the left button) */
                auto c = k->getLocalBounds().getCentre().toFloat();
                auto now = juce::Time::getCurrentTime();
                juce::ModifierKeys right(juce::String(std::getenv("PEEK_RCLICK")) == "left" ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys::rightButtonModifier);   /* ("left": the check that it would see a change) */
                juce::MouseEvent down(src, c, right, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, k, k, now, c, now, 1, false);
                juce::MouseEvent drag(src, c.translated(0.0f, -60.0f), right, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, k, k, now, c, now, 1, false);
                k->mouseDown(down);
                k->mouseDrag(drag);
                k->mouseUp(drag);
                n++;
            }
            proc.getStateInformation(after);
            bool same = before == after;
            std::printf("right click on %d controls: %s\n", n, same ? "nothing changed" : "A VALUE CHANGED");
            if (!same)
                return 1;
        }
    }
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(out);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
