/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "PluginProcessor.h"
#include "PanelDialog.h"
#include "PluginEditor.h"

using namespace pk;

/* ------------------------------------------------------------------------------- parameters --- */
juce::AudioProcessorValueTreeState::ParameterLayout PeekProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    /* the host's groups: the Peak's sections */
    struct G {
        int from, to;
        const char *id, *name;
    };
    static const G GROUPS[] = {
        {P_OSC1, P_OSC1 + OSC_N, "osc1", "Oscillator 1"},     {P_OSC1 + OSC_N, P_OSC1 + 2 * OSC_N, "osc2", "Oscillator 2"},
        {P_OSC1 + 2 * OSC_N, P_OSC_END, "osc3", "Oscillator 3"}, {P_DIVERGE, P_LVL1, "osccommon", "Oscillator Common"},
        {P_LVL1, P_FREQ, "mixer", "Mixer"},                    {P_FREQ, P_GLIDE_ON, "filter", "Filter"},
        {P_GLIDE_ON, P_AENV, "voice", "Glide / Voice"},        {P_AENV, P_LFO1, "env", "Envelopes / Animate"},
        {P_LFO1, P_ARP_ON, "lfo", "LFOs"},                     {P_ARP_ON, P_DIST, "arp", "Arp / Clock"},
        {P_DIST, P_MOD1, "fx", "Effects"},                     {P_MOD1, P_MOD_END, "mod", "Mod Matrix"},
        {P_FXMOD1, P_FXMOD_END, "fxmod", "FX Mod Matrix"}};
    for (auto &g : GROUPS) {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>(g.id, g.name, " | ");
        for (int p = g.from; p < g.to; p++) {
            const ParamDef &d = PARAM[p];
            group->addChild(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{d.id, 1}, d.name, d.lo, d.hi, d.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction([p](int v, int) {
                    char t[40];
                    paramText(p, v, t, sizeof t);
                    return juce::String(t);
                })));
        }
        layout.add(std::move(group));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"voices", 1}, "Voices",
                                                            juce::StringArray{"8 Voice", "16 Voice"}, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"host_sync", 1}, "Host Sync", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality", 1}, "Quality",
                                                            juce::StringArray{"ECO (2x)", "HIGH (4x)"}, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"transpose", 1}, "Transpose", -12, 12, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1}, "Master Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v <= 0.0001f ? juce::String("-inf") : juce::String(20.0f * std::log10(v * v * 2.0f), 1) + " dB";
        })));
    return layout;
}

PeekProcessor::PeekProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PEEK", createLayout())
{
    for (int p = 0; p < NPARAM; p++)
        params[p] = apvts.getParameter(PARAM[p].id);
    loadUserPatches();
    live = FACTORY[0];
    applyPatch(FACTORY[0]);
}

int PeekProcessor::value(int p) const { return (int)std::lround(params[p]->convertFrom0to1(params[p]->getValue())); }
void PeekProcessor::setValue(int p, int v)
{
    auto *q = params[p];
    v = juce::jlimit(PARAM[p].lo, PARAM[p].hi, v);
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1((float)v));
    q->endChangeGesture();
}

void PeekProcessor::command(Cmd c, int a, int v)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0)
        cmds[(size_t)s1] = {(int)c, a, v};
    else if (n2 > 0)
        cmds[(size_t)s2] = {(int)c, a, v};
    fifo.finishedWrite(n1 + n2);
}

/* ------------------------------------------------------------------------------- patches --- */
void PeekProcessor::applyPatch(const Patch &p)
{
    {
        juce::SpinLock::ScopedLockType lock(nameLock);
        live = p;
    }
    for (int i = 0; i < NPARAM; i++)
        params[i]->setValueNotifyingHost(params[i]->convertTo0to1((float)p.v[i]));
    pendingSet.store(true);
}

void PeekProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= programCount())
        return;
    program = index;
    compare = false;
    applyPatch(index < NFACTORY ? FACTORY[index] : userPatches[(size_t)(index - NFACTORY)]);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

const juce::String PeekProcessor::getProgramName(int index)
{
    if (index >= 0 && index < NFACTORY)
        return juce::String(FACTORY[index].name);
    if (index >= NFACTORY && index < programCount())
        return juce::String(userPatches[(size_t)(index - NFACTORY)].name);
    return {};
}

juce::String PeekProcessor::patchName() const
{
    juce::SpinLock::ScopedLockType lock(nameLock);
    return juce::String(live.name);
}
juce::String PeekProcessor::patchCategory() const
{
    juce::SpinLock::ScopedLockType lock(nameLock);
    return juce::String(live.category);
}
void PeekProcessor::setPatchName(const juce::String &n)
{
    juce::SpinLock::ScopedLockType lock(nameLock);
    std::snprintf(live.name, NAME_LEN, "%s", n.substring(0, NAME_LEN - 1).toRawUTF8());
}

Patch PeekProcessor::currentPatch() const
{
    Patch p;
    {
        juce::SpinLock::ScopedLockType lock(nameLock);
        p = live;
    }
    for (int i = 0; i < NPARAM; i++)
        p.v[i] = (int16_t)value(i);
    return p;
}

void PeekProcessor::setCompare(bool on)
{
    if (on == compare)
        return;
    if (on) {
        editKept = currentPatch();
        compare = true;
        applyPatch(program < NFACTORY ? FACTORY[program] : userPatches[(size_t)(program - NFACTORY)]);
    } else {
        compare = false;
        applyPatch(editKept);
    }
}

void PeekProcessor::initialise()
{
    compare = false;
    program = NFACTORY - 1;
    applyPatch(FACTORY[NFACTORY - 1]);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

juce::File PeekProcessor::patchFolder()
{
    return pdlg::docsOrAppData().getChildFile("PEEK").getChildFile("Patches");
}

void PeekProcessor::patchToXml(const Patch &p, juce::XmlElement &x)
{
    x.setAttribute("name", juce::String(p.name));
    x.setAttribute("category", juce::String(p.category));
    x.setAttribute("version", 1);
    for (int i = 0; i < NPARAM; i++)
        x.setAttribute(PARAM[i].id, (int)p.v[i]);
}

bool PeekProcessor::patchFromXml(const juce::XmlElement &x, Patch &p)
{
    patchInit(p);
    std::snprintf(p.name, NAME_LEN, "%s", x.getStringAttribute("name", "Patch").substring(0, NAME_LEN - 1).toRawUTF8());
    std::snprintf(p.category, sizeof p.category, "%s", x.getStringAttribute("category", "None").substring(0, 11).toRawUTF8());
    for (int i = 0; i < NPARAM; i++)
        p.v[i] = (int16_t)juce::jlimit(PARAM[i].lo, PARAM[i].hi, x.getIntAttribute(PARAM[i].id, PARAM[i].def));
    return true;
}

void PeekProcessor::loadUserPatches()
{
    userPatches.clear();
    auto files = patchFolder().findChildFiles(juce::File::findFiles, false, "*.peek");
    files.sort();
    for (auto &f : files)
        if (auto xml = juce::XmlDocument::parse(f))
            if (xml->hasTagName("PEEKPATCH")) {
                Patch p;
                if (patchFromXml(*xml, p))
                    userPatches.push_back(p);
            }
}

int PeekProcessor::savePatch(const juce::String &name)
{
    auto clean = name.trim().substring(0, NAME_LEN - 1);
    if (clean.isEmpty())
        return -1;
    Patch p = currentPatch();
    std::snprintf(p.name, NAME_LEN, "%s", clean.toRawUTF8());
    juce::XmlElement xml("PEEKPATCH");
    patchToXml(p, xml);
    auto folder = patchFolder();
    folder.createDirectory();
    if (!xml.writeTo(folder.getChildFile(juce::File::createLegalFileName(clean) + ".peek")))
        return -1;
    setPatchName(clean);
    loadUserPatches();
    for (size_t i = 0; i < userPatches.size(); i++)
        if (juce::String(userPatches[i].name) == clean) {
            program = NFACTORY + (int)i;
            updateHostDisplay(ChangeDetails().withProgramChanged(true));
            return program;
        }
    return -1;
}

/* ------------------------------------------------------------------------------- the block --- */
bool PeekProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PeekProcessor::prepareToPlay(double sampleRate, int)
{
    uiMidi.reset(sampleRate);
    engine.prepare(sampleRate);
    pendingSet.store(true);
    sentVoices = -1;
    lastGain = -1.0f;
}

void PeekProcessor::handleMidi(const juce::MidiMessage &m)
{
    if (m.isNoteOn())
        engine.noteOn(juce::jlimit(0, 127, m.getNoteNumber() + transpose), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff(juce::jlimit(0, 127, m.getNoteNumber() + transpose));
    else if (m.isPitchWheel())
        engine.bend((float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.pressure((float)m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.polyPressure(juce::jlimit(0, 127, m.getNoteNumber() + transpose), (float)m.getAfterTouchValue() / 127.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        engine.sustain(m.isSustainPedalOn());
    else if (m.isController()) {
        int cc = m.getControllerNumber(), raw = m.getControllerValue();
        float v = (float)raw / 127.0f;
        switch (cc) {
        case 1: engine.modWheel(v); break;
        case 2: engine.breath(v); break;
        case 3: engine.cv(juce::jlimit(-1.0f, 1.0f, ((float)raw - 64.0f) / 63.0f)); break;
        case 4: engine.breath(v); break;
        case 11: engine.expression(v); break;
        case 80: engine.animate(0, raw >= 64); break;
        case 81: engine.animate(1, raw >= 64); break;
        case 120: engine.panic(); break;
        case 123: engine.allOff(); break;
        case 121:
            engine.bend(0.0f);
            engine.modWheel(0.0f);
            engine.pressure(0.0f);
            engine.sustain(false);
            break;
        default: break;
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allOff();
}

void PeekProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float *L = buffer.getWritePointer(0), *R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    /* the parameters, when they change (a whole patch: all of them) */
    bool all = pendingSet.exchange(false);
    for (int i = 0; i < NPARAM; i++) {
        int v = (int)std::lround(params[i]->convertFrom0to1(params[i]->getValue()));
        if (all || v != sent[i]) {
            engine.setParam(i, v);
            sent[i] = (int16_t)v;
        }
    }
    int voices = apvts.getRawParameterValue("voices")->load() > 0.5f ? 16 : 8;
    if (voices != sentVoices) {
        engine.setVoices(voices);
        sentVoices = voices;
    }
    transpose = (int)std::lround(apvts.getRawParameterValue("transpose")->load());
    engine.setQuality(apvts.getRawParameterValue("quality")->load() > 0.5f ? 1 : 0);

    /* the panel's commands */
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead(fifo.getNumReady(), s1, n1, s2, n2);
        auto run = [&](const Command &c) {
            switch (c.c) {
            case C_ANIMATE: engine.animate(c.a, c.v != 0); break;
            case C_HOLD: engine.animateHold(c.v != 0); break;
            case C_AUDITION:
                if (c.v && !auditioning)
                    engine.noteOn(60, 0.8f);
                else if (!c.v && auditioning)
                    engine.noteOff(60);
                auditioning = c.v != 0;
                break;
            case C_PANIC: engine.panic(); break;
            default: break;
            }
        };
        for (int i = 0; i < n1; i++)
            run(cmds[(size_t)(s1 + i)]);
        for (int i = 0; i < n2; i++)
            run(cmds[(size_t)(s2 + i)]);
        fifo.finishedRead(n1 + n2);
    }

    /* the host's tempo and position */
    bool sync = apvts.getRawParameterValue("host_sync")->load() > 0.5f;
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition())
                ppq = *q;
        }
    engine.host(playing, ppq, bpm, sync);

    uiMidi.removeNextBlockOfMessages(midi, n);
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    int done = 0;
    for (const auto meta : midi) {
        int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > done) {
            engine.render(L + done, R + done, at - done);
            done = at;
        }
        handleMidi(meta.getMessage());
    }
    if (n > done)
        engine.render(L + done, R + done, n - done);

    /* for the editor */
    for (int v = 0; v < MAXV; v++)
        voiceLevel[v].store(engine.voiceLevel(v), std::memory_order_relaxed);
    for (int k = 0; k < 4; k++)
        lfoNow[k].store(engine.lfoValue(k), std::memory_order_relaxed);
    beatLed.store(engine.clockBeat(), std::memory_order_relaxed);
    bpmNow.store(engine.bpmNow(), std::memory_order_relaxed);
    animLed[0].store(engine.animateOn(0), std::memory_order_relaxed);
    animLed[1].store(engine.animateOn(1), std::memory_order_relaxed);
    holdLed.store(engine.animateHolding(), std::memory_order_relaxed);

    float vol = apvts.getRawParameterValue("volume")->load();
    float gain = vol * vol * 2.0f;
    if (lastGain < 0.0f)
        lastGain = gain;
    buffer.applyGainRamp(0, 0, n, lastGain, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGainRamp(1, 0, n, lastGain, gain);
    lastGain = gain;
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

/* ------------------------------------------------------------------------------ state --- */
void PeekProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    state.setProperty("uiScale", (double)uiScale.load(), nullptr);
    state.setProperty("uiScaleSet", uiScaleSet.load(), nullptr);
    {
        juce::SpinLock::ScopedLockType lock(nameLock);
        state.setProperty("patchName", juce::String(live.name), nullptr);
        state.setProperty("patchCategory", juce::String(live.category), nullptr);
    }
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PeekProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    program = (int)state.getProperty("program", 0);
    uiScale = (float)(double)state.getProperty("uiScale", 0.9);
    uiScaleSet = (bool)state.getProperty("uiScaleSet", false);
    {
        juce::SpinLock::ScopedLockType lock(nameLock);
        std::snprintf(live.name, NAME_LEN, "%s", state.getProperty("patchName", "Patch").toString().substring(0, NAME_LEN - 1).toRawUTF8());
        std::snprintf(live.category, sizeof live.category, "%s", state.getProperty("patchCategory", "None").toString().substring(0, 11).toRawUTF8());
    }
    apvts.replaceState(state);
    pendingSet.store(true);
}

juce::AudioProcessorEditor *PeekProcessor::createEditor() { return new PeekEditor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new PeekProcessor(); }
