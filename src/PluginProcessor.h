/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK as a plugin (VST3, AU on the Mac) and a standalone app: the engine (dsp/Engine.h), the patch's parameters for
 * the host (automation, saved with the project), the factory patches and the user's own as the host's programs, MIDI
 * in (notes, velocity, channel and poly pressure, pitch bend, mod wheel, the pedals, CV, sustain), the host's tempo
 * and position for the clock. The panel's buttons that act on the engine (ANIMATE, HOLD, AUDITION, ...) come through
 * a small queue. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "Engine.h"
#include "Presets.h"

class PeekProcessor : public juce::AudioProcessor
{
public:
    PeekProcessor();
    ~PeekProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PEEK"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    /* A VST3 host (Cubase, FL Studio) sizes its program parameter when the plugin opens: a patch saved later lay
     * past that size, so the host answered the save with another patch. The VST3 host is told a fixed number
     * of slots instead; the slots past the real patches are empty. */
    static constexpr int HOST_PROGRAMS = 1024;
    int getNumPrograms() override { return wrapperType == wrapperType_VST3 ? juce::jmax(HOST_PROGRAMS, programCount()) : programCount(); }
    int programCount() const { return pk::NFACTORY + (int)userPatches.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter *param(int p) const { return params[p]; }
    int value(int p) const;                     /* a parameter now (its integer) */
    void setValue(int p, int v);                /* (from the editor: a gesture, the host told) */

    /* the panel's commands to the engine (any thread; done at the next block) */
    enum Cmd { C_ANIMATE, C_HOLD, C_AUDITION, C_PANIC };
    void command(Cmd c, int a = 0, int v = 0);

    /* what the engine shows (the editor's LEDs) */
    std::atomic<float> voiceLevel[pk::MAXV] = {};
    std::atomic<float> lfoNow[4] = {};
    std::atomic<bool> beatLed{false}, animLed[2] = {}, holdLed{false};
    std::atomic<float> bpmNow{120.0f};
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector uiMidi;
    std::atomic<float> uiScale{0.9f};
    std::atomic<bool> uiScaleSet{false};         /* (a size chosen: else the window opens as large as the screen allows) */

    juce::String patchName() const;
    juce::String patchCategory() const;
    void setPatchName(const juce::String &);

    /* the user's patches (SAVE): files in Documents/PEEK/Patches, after the factory ones */
    std::vector<pk::Patch> userPatches;
    static juce::File patchFolder();
    void loadUserPatches();
    /* the folder read again (after an import): the sound playing keeps its number */
    void rescanUser()
    {
        auto cur = getProgramName(program);
        loadUserPatches();
        if (getProgramName(program) != cur)
            for (int i = programCount(); --i >= 0;)
                if (getProgramName(i) == cur) {
                    program = i;
                    break;
                }
        updateHostDisplay(ChangeDetails().withProgramChanged(true));
    }
    int savePatch(const juce::String &name);
    pk::Patch currentPatch() const;             /* the sound as it is now, whole */
    bool compare = false;                       /* COMPARE: the stored patch heard, the edit kept aside */
    void setCompare(bool on);
    void initialise();                          /* INITIALISE: the Init Patch */

private:
    pk::Engine engine;
    juce::RangedAudioParameter *params[pk::NPARAM] = {};
    int16_t sent[pk::NPARAM] = {};
    int sentVoices = -1;
    int program = 0;
    float lastGain = -1.0f;
    int transpose = 0;
    bool auditioning = false;

    mutable juce::SpinLock nameLock;
    pk::Patch live;                             /* its name and category */
    std::atomic<bool> pendingSet{false};
    pk::Patch editKept;

    struct Command {
        int c, a, v;
    };
    juce::AbstractFifo fifo{256};
    std::array<Command, 256> cmds;

    void handleMidi(const juce::MidiMessage &m);
    void applyPatch(const pk::Patch &p);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void patchToXml(const pk::Patch &p, juce::XmlElement &x);
    static bool patchFromXml(const juce::XmlElement &x, pk::Patch &p);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeekProcessor)
};
