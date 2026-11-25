/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK's engine: a Novation Peak, its signal path as its block diagram draws it.
 *
 *   voices     8 or 16; POLY, POLY2, MONO, MONOLG, MONO2; UNISON 1..8 with its detune and spread (four spread modes)
 *   per voice  three oscillators (sine, triangle, sawtooth, square/pulse and 60 wavetables; SHAPE by hand, Mod Env 1
 *              and LFO 1; VSYNC; SAWDENSE; their pitch by Mod Env 2 and LFO 2; DIVERGE and DRIFT), linear FM between
 *              them (O1>O2, O2>O3, O3>O1, noise>O1), noise (its low- and high-pass), RING 1*2, the mixer;
 *              OVERDRIVE, the state-variable filter (LP / BP / HP, 12 or 24 dB, its integrators saturating as an OTA's
 *              do: it sings on its own), Osc 3 at audio rate into its frequency, KEY TRACKING, FILTER DIVERGE; the
 *              POST DRIVE; the VCA (Amp Env, VCA GAIN); its pan. The oscillators, the mixer, both drives and the
 *              filter run at 4x (HIGH) or 2x (ECO) the host's rate
 *   modulation three DAHDSR envelopes (REPEATS, velocity, mono re-trigger or legato), LFO 1 and 2 per voice (fade in
 *              or out, phase, slew, repeats, common), LFO 3 and 4 global, ANIMATE 1 and 2 (their own attack and
 *              release), a 16-slot matrix of two sources each, a 4-slot FX matrix
 *   the rest   glide and pre-glide, the arp (its 33 rhythms, octaves, swing, gate, chance, latch), the clock (its own
 *              or the host's), the effects (dsp/Fx.h)
 *
 * The engine renders in chunks of 16 samples (the modulation's rate, 0.36 ms); notes land on the next chunk. All
 * calls from the audio thread. */
#pragma once
#include "Dsp.h"
#include "Fx.h"
#include "Params.h"

namespace pk
{
constexpr int MAXV = 16, CH = 16, NKEYS = 32;

class Engine
{
public:
    Engine();
    void prepare(double sampleRate);
    void setPatch(const Patch &p);
    const Patch &patch() const { return pt; }
    void setParam(int p, int v);
    void setVoices(int n);                       /* 8 or 16 */
    void setQuality(int q) { quality = q ? 1 : 0; }   /* 1 HIGH: the voices at 4x; 0 ECO: at 2x */
    int voiceCount() const { return nVoices; }

    void noteOn(int note, float vel);            /* vel 0..1 */
    void noteOff(int note);
    void sustain(bool on);
    void bend(float v);                          /* -1..1 */
    void modWheel(float v);                      /* 0..1 */
    void pressure(float v);                      /* channel aftertouch */
    void polyPressure(int note, float v);
    void expression(float v);                    /* ExprPED1 */
    void breath(float v);                        /* BrthPED2 */
    void cv(float v);                            /* the CV input: -1..1 */
    void animate(int k, bool on);                /* the ANIMATE buttons (k 0, 1) */
    void animateHold(bool on);
    bool animateOn(int k) const { return anim[k] || animHeld[k]; }
    bool animateHolding() const { return animHold; }
    void allOff();
    void panic();
    /* the host's transport: playing, its position (quarters), its tempo; sync: follow it (else the patch's clock) */
    void host(bool playing, double ppq, double bpm, bool sync);

    void render(float *L, float *R, int n);

    /* for the editor */
    int voicesSounding() const;
    float voiceLevel(int v) const { return v >= 0 && v < MAXV && V[v].active ? V[v].e[0].v : 0.0f; }
    float lfoValue(int k) const { return k >= 0 && k < 4 ? lfoShow[k] : 0.0f; }
    float bpmNow() const { return bpmCur; }
    bool clockBeat() const;

private:
    struct Env {
        float v = 0.0f, t = 0.0f;                /* its level, the time in its stage */
        uint8_t st = 0;                          /* ES_* */
        int rep = 0;
        float vs = 1.0f;                         /* velocity's scale */
    };
    enum { ES_IDLE, ES_DELAY, ES_ATT, ES_HOLD, ES_DEC, ES_SUS, ES_REL };
    struct Lfo {
        float ph = 0.0f, sh = 0.0f, out = 0.0f, fade = 0.0f;
        int cycles = 0;
        bool done = false;
    };

    struct Voice {
        bool active = false, gate = false, sus = false;
        int note = -1, key = -1;                 /* key: what plays it (a key, the arp's) */
        float vel = 0.0f, press = 0.0f;
        float pitch = 60.0f, target = 60.0f, glideK = 0.0f, pre = 0.0f;
        float uniDet = 0.0f, pan = 0.0f;
        int uniIdx = 0, slot = 0;
        Osc o[3];
        float last[3] = {};                      /* the oscillators' last outputs (the FM's) */
        float nLp = 0.0f, nHp = 0.0f, nX = 0.0f;
        float divO[3] = {}, divF = 0.0f, drift[3] = {}, driftT[3] = {};
        int driftN = 0;
        Env e[3];
        Lfo l[2];
        float s1[3] = {}, s2[3] = {};            /* the filter's two stages' states (and their last band) */
        float emph = 0.0f, deemph = 0.0f;
        HalfBand h1, h2;
        float sm[12] = {};
        bool smOk = false;
        float fade = 1.0f;                       /* taken: out quickly, then the pending note */
        bool pend = false;
        int pNote = -1, pKey = -1, pUni = 0;
        float pVel = 0.0f;
        uint32_t seed = 1, age = 0;
        float dst[NMODDST] = {};
        float srcNoise = 0.0f;
    };

    Patch pt;
    Fx fx;
    FxSet fxs;
    Voice V[MAXV];
    int nVoices = 16, quality = 1;
    float sr = 44100.0f;
    const float *tab = nullptr;
    uint32_t ageN = 0, seedN = 0x2545F491u;

    /* the controllers, and gliding (5 ms) */
    float bendV = 0, mwV = 0, atV = 0, exV = 0, brV = 0, cvV = 0;
    float ctl[6] = {};
    bool susOn = false;
    bool anim[2] = {}, animHeld[2] = {}, animHold = false;
    float animV[2] = {};
    /* the global LFOs (3, 4; and 1, 2's common phase) */
    float gPh[4] = {}, gSh[4] = {}, gOut[4] = {}, lfoShow[4] = {};
    uint32_t gSeed = 0x9E3779B9u;
    /* keys held (in order) */
    int8_t keys[NKEYS];
    float keyVel[NKEYS];
    int nKeys = 0;
    int lastNote = 60, monoNote = -1, rr = 0, d2 = 0;
    bool anyHeld = false;
    float lastPitch = 60.0f;
    /* the clock and the arp */
    bool hostPlaying = false, hostSync = true;
    double hostPpq = 0.0, hostBpm = 120.0, pos = 0.0;
    float bpmCur = 120.0f;
    long stepIdx = -1;
    double stepStart = 0.0;
    int8_t arpNotes[NKEYS];
    float arpVel[NKEYS];
    int arpN = 0, arpI = -1, arpDir = 1, arpOct = 0, arpPlaying[NKEYS], arpNPlaying = 0, arpRhythmStep = 0;
    bool arpLatched = false, arpWasOn = false;
    double arpOffAt = -1.0;
    float dcL = 0, dcRr = 0, dcxL = 0, dcxR = 0;
    float gNoise = 0.0f;
    float fxdL3 = 0.0f, fxdL4 = 0.0f, fxdNL = 0.0f, fxdNH = 0.0f;   /* the FX matrix's LFO 3 / 4 rate and noise filters */
    float outL[CH] = {}, outR[CH] = {};
    int gpos = CH;

    void keyEvent(int note, float vel, bool down);
    void playNote(int key, int note, float vel, bool legato);
    void stopNote(int key);
    void startVoice(Voice &v, int key, int note, float vel, int uni, bool legato);
    int findVoice(int note);
    void releaseVoice(Voice &v);
    void chunk();
    void clockChunk();
    void arpStep();
    void arpRebuild();
    void arpAllOff();
    void voiceChunk(Voice &v, float *L, float *R);
    void modulate(Voice &v);
    float srcValue(const Voice *v, int s) const;
    float lfoHz(int k, float extra) const;
    void envGate(Env &e, int base, const Voice &v, bool legato);
    int unisonN() const { return UNISON_VOICES[pt.v[P_UNISON]]; }
    bool mono() const { return pt.v[P_MODE] <= VM_MONO2; }
};
} // namespace pk
