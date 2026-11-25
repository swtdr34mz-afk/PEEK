// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 swtdr34mz (PEEK)
//
// PEEK for the MPC: the same engine as the VST3 and AU (dsp/: Engine, Params, Presets, Wavetables, Blep), hosted by
// the MPC Plugin SDK's framework (mpc_plugin.h). mpc/build.py copies the engine's files beside this one before the SDK
// builds it.
//
// The sound: a Peak-style poly of 8 or 16 voices, three oscillators with shape, VSync, saw density and 60 wavetables,
// linear FM, the state-variable filter (LP / BP / HP, 12 / 24 dB) between its overdrive and its post drive, three
// envelopes, four LFOs, ANIMATE, a 16-slot matrix and a 4-slot FX matrix, the arp, distortion, chorus, delay, reverb;
// on the MPC's tempo, locked to its bar while it plays.
//
// Parameters (their ids never change; new ones only at the end):
//   0              VOLUME (the host nudges parameter 0 on every prepare: harmless here, it ramps)
//   1..275         the patch's 275 parameters, in dsp/Params.h's order
//   276            VOICES (8 or 16)
//   277            QUALITY (ECO: the voices at 2x; HIGH: at 4x)
//   278, 279       ANIMATE 1, ANIMATE 2
//   280            ANIMATE HOLD
//
// MIDI in: notes and velocity, pitch bend, channel and poly pressure, CC 1 mod wheel, CC 2 breath (pedal 2),
// CC 3 the CV input (64 the middle), CC 4 pedal 2, CC 11 expression (pedal 1), CC 64 sustain, CC 80 / 81 ANIMATE 1 / 2,
// CC 120 all sound off, CC 121 reset controllers, CC 123 all notes off.

#include "mpc_plugin.h"
#include "Engine.h"
#include "Presets.h"
#include "peek_names.h"

#include <cmath>
#include <cstring>

namespace {
using namespace pk;

enum {
    kVolume = 0,
    kP = 1,
    kVoices = kP + NPARAM,
    kQuality,
    kAnim1,
    kAnim2,
    kHold,
    kNumParams
};
static_assert(kNumParams == 281, "the parameters' order is permanent");
static_assert(sizeof(SHORT) / sizeof(SHORT[0]) == NPARAM, "a short name for each parameter");

class PeekMpc : public mpc::Plugin
{
public:
    PeekMpc() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("vol", "Volume", 0.0f, 1.0f, 0.7f, "", 2));
        for (int p = 0; p < NPARAM; p++) {
            const ParamDef &d = PARAM[p];
            if (d.kind == K_CHOICE || d.kind == K_ONOFF) {
                std::vector<std::string> ch;
                for (int k = d.lo; k <= d.hi; k++)
                    ch.push_back(d.choice[k - d.lo]);
                addParam(mpc::choiceParam(d.id, SHORT[p], ch, d.def - d.lo));
            } else {
                addParam(mpc::intParam(d.id, SHORT[p], d.lo, d.hi, d.def));
            }
        }
        addParam(mpc::choiceParam("voices", "Voices", {"8 Voice", "16 Voice"}, 0));
#if defined(__arm__)                     /* (the 32-bit MPCs: ECO to start with) */
        addParam(mpc::choiceParam("quality", "Quality", {"ECO", "HIGH"}, 0));
#else
        addParam(mpc::choiceParam("quality", "Quality", {"ECO", "HIGH"}, 0));
#endif
        addParam(mpc::boolParam("anim1", "Animate 1", false));
        addParam(mpc::boolParam("anim2", "Animate 2", false));
        addParam(mpc::boolParam("hold", "Anim Hold", false));

        /* the factory patches: VOLUME and the sound; VOICES, QUALITY and ANIMATE they leave as they are */
        for (int k = 0; k < NFACTORY; k++) {
            std::vector<float> v;
            v.push_back(0.7f);
            for (int p = 0; p < NPARAM; p++) {
                int val = FACTORY[k].v[p];
                if (PARAM[p].kind == K_CHOICE || PARAM[p].kind == K_ONOFF)
                    val -= PARAM[p].lo;
                v.push_back((float)val);
            }
            addProgram(FACTORY[k].name, v);
        }
        engine.prepare(44100.0);         // (the MPC's rate: made here, on the UI thread, never in process)
        rate = 44100.0;
    }

    void prepare(double sampleRate, int) override
    {
        if (sampleRate != rate) {        // (only if the rate really changes: the MPC's is fixed at 44.1 kHz)
            engine.prepare(sampleRate);
            rate = sampleRate;
        }
        forceSend();
    }

    /* resume, or a program / state loaded (on the audio thread): the voices let go */
    void reset() override
    {
        engine.panic();
        forceSend();
    }

    void process(mpc::ProcessContext &ctx) override
    {
        const int n = ctx.numFrames;
        float *L = ctx.outputs[0], *R = ctx.numOutputs > 1 ? ctx.outputs[1] : ctx.outputs[0];
        sync();
        double bpm = ctx.transport.valid && ctx.transport.tempoValid ? ctx.transport.bpm : 120.0;
        bool playing = ctx.transport.valid && ctx.transport.playing && ctx.transport.ppqValid;
        engine.host(playing, playing ? ctx.transport.ppqPos : 0.0, bpm, true);
        int done = 0;
        for (int i = 0; i < ctx.numMidiEvents; i++) {
            const mpc::MidiEvent &e = ctx.midi[i];
            int at = mpc::clamp(e.sampleOffset, 0, n);
            if (at > done) {
                engine.render(L + done, R + done, at - done);
                done = at;
            }
            midi(e);
        }
        if (n > done)
            engine.render(L + done, R + done, n - done);
        float v = (float)get(kVolume), g = v * v * 2.0f;
        if (lastGain < 0.0f)
            lastGain = g;
        float step = (g - lastGain) / (float)(n > 0 ? n : 1), gg = lastGain;
        for (int s = 0; s < n; s++) {
            gg += step;
            L[s] *= gg;
            if (R != L)
                R[s] *= gg;
        }
        lastGain = g;
    }

    bool formatValue(int index, float plain, std::string &out) override
    {
        int v = (int)std::lround(plain);
        char t[40];
        if (index == kVolume) {
            if (plain <= 0.0001f)
                out = "-inf dB";
            else {
                std::snprintf(t, sizeof t, "%.1f dB", 20.0 * std::log10(plain * plain * 2.0));
                out = t;
            }
            return true;
        }
        if (index >= kP && index < kVoices) {
            int p = index - kP;
            const ParamDef &d = PARAM[p];
            if (d.kind == K_CHOICE || d.kind == K_ONOFF)
                v += d.lo;               /* (a choice's plain value is its index from 0) */
            paramText(p, mpc::clamp(v, d.lo, d.hi), t, sizeof t);
            out = t;
            if (out.size() > 12)
                out.resize(12);
            return true;
        }
        return false;
    }

private:
    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "PEEK";
        i.vendor = "swtdr34mz";
        i.uniqueId = MPC_FOURCC('P', 'e', 'e', 'k');
        i.version = 1000;
        i.isInstrument = true;
        i.numInputs = 0;
        i.numOutputs = 2;
        i.tailSamples = 44100 * 12;
        return i;
    }

    void forceSend() { lastGain = -1.0f, all = true; }

    int value(int p) const
    {
        const ParamDef &d = PARAM[p];
        int v = getInt(kP + p);
        if (d.kind == K_CHOICE || d.kind == K_ONOFF)
            v += d.lo;
        return mpc::clamp(v, d.lo, d.hi);
    }

    /* the parameters to the engine, when they change (atomics: safe here) */
    void sync()
    {
        for (int p = 0; p < NPARAM; p++) {
            int v = value(p);
            if (all || v != sent[p]) {
                engine.setParam(p, v);
                sent[p] = (int16_t)v;
            }
        }
        all = false;
        engine.setVoices(getInt(kVoices) ? 16 : 8);
        engine.setQuality(getInt(kQuality));
        bool a1 = getBool(kAnim1), a2 = getBool(kAnim2), h = getBool(kHold);
        if (h != holdSent) {
            engine.animateHold(h);
            holdSent = h;
        }
        if (a1 != anim1Sent) {
            engine.animate(0, a1);
            anim1Sent = a1;
        }
        if (a2 != anim2Sent) {
            engine.animate(1, a2);
            anim2Sent = a2;
        }
    }

    void midi(const mpc::MidiEvent &e)
    {
        const uint8_t st = e.status();
        if (e.isNoteOn())
            engine.noteOn(e.note(), (float)e.velocity() / 127.0f);
        else if (e.isNoteOff())
            engine.noteOff(e.note());
        else if (e.isPitchBend())
            engine.bend((float)e.pitchBend() / 8192.0f);
        else if (st == 0xA0)
            engine.polyPressure(e.data[1], (float)e.data[2] / 127.0f);
        else if (e.isAftertouch())
            engine.pressure((float)e.data[1] / 127.0f);
        else if (e.isController()) {
            int cc = e.controller(), raw = e.controllerValue();
            float v = (float)raw / 127.0f;
            switch (cc) {
            case 1: engine.modWheel(v); break;
            case 2:
            case 4: engine.breath(v); break;
            case 3: engine.cv(mpc::clamp(((float)raw - 64.0f) / 63.0f, -1.0f, 1.0f)); break;
            case 11: engine.expression(v); break;
            case 64: engine.sustain(raw >= 64); break;
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
        }
    }

    Engine engine;
    double rate = 0.0;
    int16_t sent[NPARAM] = {};
    bool all = true, holdSent = false, anim1Sent = false, anim2Sent = false;
    float lastGain = -1.0f;
};

} // namespace

MPC_PLUGIN_ENTRY(PeekMpc)
