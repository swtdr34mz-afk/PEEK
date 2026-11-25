/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "Engine.h"
#include <algorithm>
#include <vector>

namespace pk
{
namespace
{
enum { K_ARP = 1000 };
constexpr float VOICE_GAIN = 0.17f;            /* a voice's level: a four-note chord of the Init Patch about -15 dB RMS */

/* the Peak's times (its user guide): ATTACK to over 18 s, DECAY about 22 s, RELEASE over 24 s, DELAY 85 = 1 s,
 * 127 = 10 s, HOLD up to 500 ms, GLIDE 90 = about 1 s */
inline float attSec(float v) { return v <= 0.0f ? 0.0004f : 0.0008f * fexp2(clampf(v, 0.0f, 127.0f) * (14.5f / 127.0f)); }
inline float decSec(float v) { return 0.002f * fexp2(clampf(v, 0.0f, 127.0f) * (13.4f / 127.0f)); }
inline float relSec(float v) { return 0.002f * fexp2(clampf(v, 0.0f, 127.0f) * (13.55f / 127.0f)); }
inline float dlySec(float v) { return v <= 0.5f ? 0.0f : 0.01f * fexp2(clampf(v, 0.0f, 127.0f) * (9.97f / 127.0f)); }
inline float holdSec(float v)
{
    float x = clampf(v, 0.0f, 127.0f) / 127.0f;
    return 0.5f * x * std::sqrt(x);
}
inline float glideSec(float v) { return 0.001f * fexp2(clampf(v, 0.0f, 127.0f) * (10.0f / 90.0f)); }
inline float fadeSec(float v) { return v <= 0.5f ? 0.0f : 0.01f * fexp2(clampf(v, 0.0f, 127.0f) * (10.0f / 127.0f)); }
inline float animSec(float v) { return 0.002f + 0.001f * fexp2(clampf(v, 0.0f, 127.0f) * (13.3f / 127.0f)); }
/* the pitch knobs' curves: MOD ENV 2 DEPTH to eight octaves, LFO 2 DEPTH to five, both fine near the middle */
inline float env2Semis(float v)
{
    float x = std::fabs(v) / 127.0f;
    return (v < 0 ? -96.0f : 96.0f) * x * std::sqrt(x);
}
inline float lfo2Semis(float v)
{
    float x = std::fabs(v) / 127.0f;
    return (v < 0 ? -60.0f : 60.0f) * std::pow(x, 2.2f);
}
/* a matrix slot's depth (-64..63) as semitones (pitch: fine for vibrato, four octaves at the end) */
inline float depthSemis(float d)
{
    float x = d / 64.0f;
    return 48.0f * x * std::fabs(x);
}
inline float lfoShape(int t, float ph, float held)
{
    switch (t) {
    case LT_TRI: return ph < 0.25f ? 4.0f * ph : ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f;
    case LT_SAW: return 1.0f - 2.0f * ph;                 /* (falling) */
    case LT_SQUARE: return ph < 0.5f ? 1.0f : -1.0f;
    default: return held;
    }
}
/* the arp's 33 rhythms (PEEK's own): 16 steps, x a note, - a rest; plainer to busier */
const char *const RHYTHM[NRHYTHM] = {
    "xxxxxxxxxxxxxxxx", "x-x-x-x-x-x-x-x-", "xx-xxx-xxx-xxx-x", "x-xxx-xxx-xxx-xx", "xxx-xxx-xxx-xxx-",
    "x--xx--xx--xx--x", "x-xx-xx-x-xx-xx-", "xx-x-xx-xx-x-xx-", "x--x--x-x--x--x-", "x-x-xx-xx-x-xx-x",
    "xxx-x-x-xxx-x-x-", "x-x-x-xxx-x-x-xx", "x---x---x---x-xx", "xx--x-x-xx--x-x-", "x-xxx--xx-xxx--x",
    "x--x-x--x--x-x-x", "xxx-xx-x-xx-x-x-", "x-x--x-x-x--x-x-", "x-xx--x-x-xx--xx", "xx-xx-x-xx-xx-x-",
    "x---x-x---x-x-x-", "x-x-xxx-x-x-xxxx", "xx-x--x-xx-x--xx", "x--x-xx--x-xx-x-", "x-x-x--xx-x-x--x",
    "xxxx-x-xxxx-x-x-", "x-x--xxx-x--xx-x", "x--xxx--x-x-xxx-", "xx--xx-xx--xx-x-", "x-xx-x-xx-x-x-xx",
    "x---xx--x-x-xx-x", "xx-x-x-x-xx-x-xx", "x-x-xx-x--xx-xxx"};
}

/* ------------------------------------------------------------------- setup --- */
Engine::Engine()
{
    patchInit(pt);
    for (int i = 0; i < MAXV; i++) {
        Voice &v = V[i];
        v.seed = 0x1234567u + 0x9E3779B9u * (uint32_t)(i + 1);
        /* DIVERGE's and FILTER DIVERGE's fixed offsets: each voice its own, always the same */
        uint32_t s = 0xC0FFEEu + 7919u * (uint32_t)i;
        for (int k = 0; k < 3; k++)
            v.divO[k] = rndF(s);
        v.divF = rndF(s);
        for (int k = 0; k < 3; k++)
            v.o[k].reset(0.5f + 0.5f * rndF(s));
    }
    for (int i = 0; i < NKEYS; i++)
        arpPlaying[i] = -1;
}

void Engine::prepare(double sampleRate)
{
    sr = (float)sampleRate;
    wtInit();
    tab = blepTables();
    fx.prepare(sr);
    for (auto &v : V) {
        v.active = v.gate = false;
        v.h1.clear();
        v.h2.clear();
    }
    gpos = CH;
}

void Engine::setPatch(const Patch &p)
{
    pt = p;
}

void Engine::setParam(int p, int v)
{
    if (p < 0 || p >= NPARAM)
        return;
    pt.v[p] = (int16_t)v;
    if (p == P_ARP_ON && !v)
        arpAllOff();
}

void Engine::setVoices(int n)
{
    n = n > 8 ? 16 : 8;
    if (n == nVoices)
        return;
    nVoices = n;
    for (int i = n; i < MAXV; i++)
        V[i].active = V[i].gate = false;
}

/* ------------------------------------------------------------------- notes --- */
void Engine::noteOn(int note, float vel) { keyEvent(note, clampf(vel, 0.0f, 1.0f), true); }
void Engine::noteOff(int note) { keyEvent(note, 0.0f, false); }

void Engine::keyEvent(int note, float vel, bool down)
{
    if (note < 0 || note > 127)
        return;
    bool wasHeld = nKeys > 0;
    for (int i = 0; i < nKeys; i++)
        if (keys[i] == note) {
            for (int j = i; j < nKeys - 1; j++) {
                keys[j] = keys[j + 1];
                keyVel[j] = keyVel[j + 1];
            }
            nKeys--;
            break;
        }
    if (down && nKeys < NKEYS) {
        keys[nKeys] = (int8_t)note;
        keyVel[nKeys] = vel;
        nKeys++;
    }
    anyHeld = nKeys > 0;
    if (pt.v[P_ARP_ON]) {
        bool latch = pt.v[P_ARP_LATCH] != 0;
        if (down) {
            if (latch && !wasHeld) {                 /* (latched: a new chord after all were let go starts anew) */
                arpN = 0;
                if (pt.v[P_ARP_KEYSYNC]) {
                    arpI = -1;
                    arpRhythmStep = 0;
                    stepIdx = -1;
                }
            }
            bool have = false;
            for (int i = 0; i < arpN; i++)
                have |= arpNotes[i] == note;
            if (!have && arpN < NKEYS) {
                arpNotes[arpN] = (int8_t)note;
                arpVel[arpN] = vel;
                arpN++;
            }
            if (arpN == 1 && !latch) {
                arpI = -1;
                arpRhythmStep = 0;
            }
        } else if (!latch) {
            for (int i = 0; i < arpN; i++)
                if (arpNotes[i] == note) {
                    for (int j = i; j < arpN - 1; j++) {
                        arpNotes[j] = arpNotes[j + 1];
                        arpVel[j] = arpVel[j + 1];
                    }
                    arpN--;
                    break;
                }
            if (arpN == 0)
                arpAllOff();
        }
        return;
    }
    if (down) {
        playNote(note, note, vel, mono() && wasHeld);
    } else if (mono()) {
        if (note != monoNote)
            return;                                      /* (a key held under the one sounding: nothing to do) */
        if (nKeys > 0) {                                 /* the last note held sounds again (legato) */
            int n = keys[nKeys - 1];
            playNote(n, n, keyVel[nKeys - 1], true);
        } else
            stopNote(note);
    } else
        stopNote(note);
}

int Engine::findVoice(int note)
{
    (void)note;
    /* a free voice (the one longest free), else a released one (the quietest), else the oldest */
    int best = -1;
    uint32_t bestAge = 0xffffffffu;
    for (int i = 0; i < nVoices; i++)
        if (!V[i].active && !V[i].pend && V[i].age < bestAge) {
            bestAge = V[i].age;
            best = i;
        }
    if (best >= 0)
        return best;
    float q = 1e9f;
    for (int i = 0; i < nVoices; i++)
        if (!V[i].gate && !V[i].pend && V[i].e[0].v < q) {
            q = V[i].e[0].v;
            best = i;
        }
    if (best >= 0)
        return best;
    for (int i = 0; i < nVoices; i++)
        if (V[i].age < bestAge && !V[i].pend) {
            bestAge = V[i].age;
            best = i;
        }
    return best >= 0 ? best : 0;
}

void Engine::playNote(int key, int note, float vel, bool legato)
{
    const int N = std::min(unisonN(), nVoices);
    const int mode = pt.v[P_MODE];
    lastNote = note;
    if (mode == VM_MONO || mode == VM_MONOLG) {
        for (int u = 0; u < N; u++)
            startVoice(V[u], key, note, vel, u, legato);
        monoNote = note;
        return;
    }
    if (mode == VM_MONO2) {                              /* each note its own voices, in turn */
        for (int i = 0; i < nVoices; i++)
            if (V[i].gate)
                releaseVoice(V[i]);
        int base = (rr * N) % nVoices;
        rr++;
        for (int u = 0; u < N; u++) {
            Voice &v = V[(base + u) % nVoices];
            if (v.active && v.e[0].v > 0.002f) {
                v.pend = true;
                v.pNote = note;
                v.pKey = key;
                v.pVel = vel;
                v.pUni = u;
                if (v.fade >= 1.0f)
                    v.fade = 0.999f;
            } else
                startVoice(v, key, note, vel, u, false);
        }
        monoNote = note;
        return;
    }
    if (mode == VM_POLY2) {                              /* the same note: the voices it had */
        int got = 0;
        for (int i = 0; i < nVoices && got < N; i++)
            if (V[i].active && V[i].note == note && !V[i].pend) {
                startVoice(V[i], key, note, vel, V[i].uniIdx, false);
                got++;
            }
        if (got)
            return;
    }
    for (int u = 0; u < N; u++) {
        int i = findVoice(note);
        Voice &v = V[i];
        if (v.active && v.e[0].v > 0.002f) {             /* taken: out on a short curve first */
            v.pend = true;
            v.pNote = note;
            v.pKey = key;
            v.pVel = vel;
            v.pUni = u;
            if (v.fade >= 1.0f)
                v.fade = 0.999f;
            v.age = ++ageN;
        } else
            startVoice(v, key, note, vel, u, false);
    }
}

void Engine::stopNote(int key)
{
    for (int i = 0; i < MAXV; i++) {
        Voice &v = V[i];
        if (v.pend && v.pKey == key)
            v.pend = false, v.fade = maxf(v.fade, 0.0f);
        if (v.active && v.gate && v.key == key) {
            if (susOn)
                v.sus = true;
            else
                releaseVoice(v);
        }
    }
}

void Engine::envGate(Env &e, int base, const Voice &v, bool legato)
{
    const int16_t *P = pt.v + base;
    float vs = (float)P[ENV_VEL] / 64.0f;
    e.vs = vs >= 0.0f ? 1.0f - vs * (1.0f - v.vel) : 1.0f + vs * v.vel;
    if (legato && P[ENV_TRIG] == 0 && e.st != ES_IDLE && e.st != ES_REL)
        return;                                          /* (legato: it carries on) */
    e.st = P[ENV_DLY] > 0 ? ES_DELAY : ES_ATT;
    e.t = 0.0f;
    e.rep = 0;
}

void Engine::startVoice(Voice &v, int key, int note, float vel, int uni, bool legato)
{
    const int mode = pt.v[P_MODE];
    const bool monoMode = mode <= VM_MONO2;
    bool wasActive = v.active && v.e[0].st != ES_IDLE;
    bool legatoNow = legato && monoMode && wasActive && mode != VM_MONO2;
    v.active = true;
    v.gate = true;
    v.sus = false;
    v.key = key;
    v.note = note;
    v.vel = vel;
    v.press = 0.0f;
    v.uniIdx = uni;
    v.age = ++ageN;
    v.fade = 1.0f;
    v.pend = false;
    /* glide: MONO always, MONOLG legato only, the poly modes from the voice's last note; PRE-GLIDE first */
    float tgt = (float)note;
    bool glide = pt.v[P_GLIDE_ON] != 0;
    int pre = pt.v[P_PREGLIDE];
    if (glide && pre != 0)
        v.pitch = (mode != VM_MONOLG || legatoNow) ? tgt + (float)pre : tgt;
    else if (glide && mode == VM_MONOLG)
        v.pitch = legatoNow ? v.pitch : tgt;
    else if (glide && (mode == VM_MONO || mode == VM_MONO2))
        v.pitch = wasActive && mode == VM_MONO ? v.pitch : lastPitch;
    else if (!glide)
        v.pitch = tgt;
    /* (the poly modes glide from the voice's own last note) */
    v.target = tgt;
    /* unison: each voice its place in the detune and the spread */
    const int N = std::min(unisonN(), nVoices);
    if (N > 1) {
        float pos = (float)uni / (float)(N - 1) * 2.0f - 1.0f;   /* -1..1 */
        v.uniDet = pos;
    } else
        v.uniDet = 0.0f;
    /* the pan's place (SPREAD MODE) */
    {
        float p;
        int sm = pt.v[P_SPREADMODE];
        if (N > 1)
            p = v.uniDet;
        else if (sm == SP_NOTEVAL)
            p = clampf(((float)note - 60.0f) / 36.0f, -1.0f, 1.0f);
        else if (sm == SP_DIVERGE2) {
            static const float RR[8] = {-1.0f, 0.43f, -0.14f, 0.71f, -0.71f, 0.14f, -0.43f, 1.0f};
            p = RR[(d2++) & 7];
        } else {
            int idx = (int)(&v - V);
            float w = sm == SP_ALTERNATE ? 1.0f : 0.35f + 0.65f * (float)((idx >> 1) + 1) / (float)(nVoices / 2);
            p = (idx & 1) ? w : -w;
        }
        v.pan = p;
    }
    /* key sync: the oscillators from the start of their cycle */
    if (pt.v[P_KEYSYNC] && !legatoNow)
        for (auto &o : v.o)
            o.reset(0.0f);
    if (!wasActive) {
        v.smOk = false;
        for (auto &o : v.o)
            o.r.clear();
    }
    /* the envelopes */
    for (int k = 0; k < 3; k++)
        envGate(v.e[k], k == 0 ? P_AENV : k == 1 ? P_M1ENV : P_M2ENV, v, legatoNow);
    /* LFO 1 and 2: their phase (unless free), their fade */
    for (int k = 0; k < 2; k++) {
        const int16_t *Q = pt.v + (k ? P_LFO2 : P_LFO1);
        Lfo &l = v.l[k];
        bool retrig = !legatoNow || Q[LFO_TRIG] == 1;
        if (retrig && Q[LFO_PHASE] > 0) {
            l.ph = (float)(Q[LFO_PHASE] - 1) * (3.0f / 360.0f);
            l.cycles = 0;
            l.done = false;
            l.sh = rndF(v.seed);
        }
        if (!legatoNow || Q[LFO_FSYNC])
            l.fade = 0.0f;
        if (!legatoNow) {
            l.cycles = 0;
            l.done = false;
        }
    }
    lastPitch = tgt;
}

void Engine::releaseVoice(Voice &v)
{
    v.gate = false;
    v.sus = false;
    for (auto &e : v.e)
        if (e.st != ES_IDLE) {
            e.st = ES_REL;
            e.t = 0.0f;
        }
}

void Engine::sustain(bool on)
{
    susOn = on;
    if (!on)
        for (auto &v : V)
            if (v.sus && v.active)
                releaseVoice(v);
}

void Engine::bend(float v) { bendV = clampf(v, -1.0f, 1.0f); }
void Engine::modWheel(float v) { mwV = clampf(v, 0.0f, 1.0f); }
void Engine::pressure(float v) { atV = clampf(v, 0.0f, 1.0f); }
void Engine::polyPressure(int note, float val)
{
    for (auto &v : V)
        if (v.active && v.note == note)
            v.press = clampf(val, 0.0f, 1.0f);
}
void Engine::expression(float v) { exV = clampf(v, 0.0f, 1.0f); }
void Engine::breath(float v) { brV = clampf(v, 0.0f, 1.0f); }
void Engine::cv(float v) { cvV = clampf(v, -1.0f, 1.0f); }
void Engine::animate(int k, bool on)
{
    if (k < 0 || k > 1)
        return;
    anim[k] = on;
    if (on && animHold)
        animHeld[k] = true;
}
void Engine::animateHold(bool on)
{
    animHold = on;
    if (on) {
        for (int k = 0; k < 2; k++)
            animHeld[k] = anim[k];
    } else
        animHeld[0] = animHeld[1] = false;
}

void Engine::allOff()
{
    nKeys = 0;
    anyHeld = false;
    arpN = 0;
    arpAllOff();
    for (auto &v : V)
        if (v.active) {
            v.pend = false;
            releaseVoice(v);
        }
}

void Engine::panic()
{
    allOff();
    susOn = false;
    for (auto &v : V) {
        /* (all of it from new: whatever it held, but its seed and its fixed offsets) */
        uint32_t seed = v.seed;
        float dO[3] = {v.divO[0], v.divO[1], v.divO[2]}, dF = v.divF;
        v = Voice();
        v.seed = seed;
        for (int k = 0; k < 3; k++) {
            v.divO[k] = dO[k];
            v.o[k].clear();
            v.o[k].reset(0.5f + 0.5f * rndF(v.seed));
        }
        v.divF = dF;
    }
    for (int k = 0; k < 4; k++)
        gOut[k] = flushDn(std::isfinite(gOut[k]) ? gOut[k] : 0.0f);
    dcL = dcRr = dcxL = dcxR = 0.0f;
    fx.clear();
}

void Engine::host(bool playing, double ppq, double bpm, bool sync)
{
    hostSync = sync;
    hostBpm = bpm > 1.0 ? bpm : 120.0;
    if (sync && playing) {
        if (!hostPlaying || std::fabs(ppq - pos) > 0.05)
            pos = ppq;                                   /* (locked to the host's bar) */
    }
    hostPlaying = playing;
    hostPpq = ppq;
}

/* ------------------------------------------------------------------- the arp --- */
bool Engine::clockBeat() const { return pos - std::floor(pos) < 0.5; }

void Engine::arpAllOff()
{
    for (int i = 0; i < NKEYS; i++)
        if (arpPlaying[i] >= 0) {
            stopNote(K_ARP + i);
            arpPlaying[i] = -1;
        }
    arpNPlaying = 0;
    arpOffAt = -1.0;
}

void Engine::arpStep()
{
    if (arpN == 0)
        return;
    const int16_t *P = pt.v;
    /* the rhythm: this step a note or a rest */
    const char *rh = RHYTHM[clampi(P[P_ARP_RHYTHM], 1, NRHYTHM) - 1];
    bool play = rh[arpRhythmStep & 15] == 'x';
    arpRhythmStep = (arpRhythmStep + 1) & 15;
    if (!play)
        return;
    int type = P[P_ARP_TYPE];
    float chance = (float)P[P_ARP_CHANCE] / 127.0f;
    if (type != AT_CHORD2 && chance < 1.0f && (float)(rndU(seedN) & 0xffff) / 65536.0f >= chance)
        return;
    /* the notes, low to high (PLAYED: as played) */
    int8_t sorted[NKEYS];
    float sv[NKEYS];
    int n = arpN;
    for (int i = 0; i < n; i++) {
        sorted[i] = arpNotes[i];
        sv[i] = arpVel[i];
    }
    if (type != AT_PLAYED)
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && sorted[j] < sorted[j - 1]; j--) {
                std::swap(sorted[j], sorted[j - 1]);
                std::swap(sv[j], sv[j - 1]);
            }
    int octs = clampi(P[P_ARP_OCT], 1, 6);
    arpAllOff();
    if (type == AT_CHORD || type == AT_CHORD2) {
        arpOct = (arpOct + 1) % octs;
        int k = 0;
        for (int i = 0; i < n; i++) {
            if (type == AT_CHORD2 && chance < 1.0f && (float)(rndU(seedN) & 0xffff) / 65536.0f >= chance)
                continue;
            int note = sorted[i] + 12 * arpOct;
            if (note > 127)
                continue;
            playNote(K_ARP + k, note, sv[i], false);
            arpPlaying[k++] = note;
        }
        arpNPlaying = k;
    } else {
        int len = n * octs;
        int idx;
        switch (type) {
        case AT_DOWN: arpI = arpI <= 0 ? len - 1 : arpI - 1; idx = arpI; break;
        case AT_UPDOWN1:
        case AT_UPDOWN2: {
            /* UP-DOWN 1: the ends once; UP-DOWN 2: the ends twice */
            int cyc = type == AT_UPDOWN1 ? (len > 1 ? 2 * len - 2 : 1) : 2 * len;
            arpI = (arpI + 1) % cyc;
            if (type == AT_UPDOWN1)
                idx = arpI < len ? arpI : 2 * len - 2 - arpI;
            else
                idx = arpI < len ? arpI : 2 * len - 1 - arpI;
            break;
        }
        case AT_RANDOM: idx = (int)(rndU(seedN) % (uint32_t)len); break;
        default: arpI = (arpI + 1) % len; idx = arpI; break;
        }
        idx = clampi(idx, 0, len - 1);
        int note = sorted[idx % n] + 12 * (idx / n);
        if (note <= 127) {
            playNote(K_ARP, note, sv[idx % n], false);
            arpPlaying[0] = note;
            arpNPlaying = 1;
        }
    }
}

void Engine::clockChunk()
{
    const float dt = (float)CH / sr;
    bpmCur = hostSync ? (float)hostBpm : (float)pt.v[P_BPM];
    double was = pos;
    pos += (double)dt * bpmCur / 60.0;
    (void)was;
    /* the arp's steps: its rate, swing (the odd steps longer) */
    bool on = pt.v[P_ARP_ON] != 0;
    if (!on) {
        if (arpWasOn) {
            arpAllOff();
            arpN = 0;
            arpWasOn = false;
        }
        return;
    }
    arpWasOn = true;
    if (arpN == 0) {
        stepIdx = -1;
        return;
    }
    double T = LFOSYNC_BEATS[ARPSYNC_AT + clampi(pt.v[P_ARP_RATE], 0, NARPSYNC - 1)];
    double sw = (double)pt.v[P_ARP_SWING] / 100.0;
    double pair = std::floor(pos / (2.0 * T)), in = pos - pair * 2.0 * T;
    long idx = (long)pair * 2 + (in >= 2.0 * T * sw ? 1 : 0);
    double stepLen = (idx & 1) ? 2.0 * T * (1.0 - sw) : 2.0 * T * sw;
    if (idx != stepIdx) {
        stepIdx = idx;
        stepStart = (idx & 1) ? pair * 2.0 * T + 2.0 * T * sw : pair * 2.0 * T;
        arpStep();
        float gate = (float)pt.v[P_ARP_GATE] / 127.0f;
        arpOffAt = gate >= 0.999f ? -1.0 : stepStart + stepLen * (0.02 + 0.98 * gate);
    }
    if (arpOffAt >= 0.0 && pos >= arpOffAt) {
        arpAllOff();
    }
}

/* --------------------------------------------------------------- modulation --- */
float Engine::lfoHz(int k, float extra) const
{
    const int16_t *Q = pt.v + (k ? P_LFO2 : P_LFO1);
    if (Q[LFO_RANGE] == LR_SYNC)
        return bpmCur / 60.0f / LFOSYNC_BEATS[clampi(Q[LFO_SYNC], 0, NLFOSYNC - 1)];
    float r = clampf((float)Q[LFO_RATE] + extra, 0.0f, 255.0f);
    float hz = 200.0f * fexp2((r - 255.0f) * (14.3f / 255.0f));   /* LOW: to 200 Hz */
    return Q[LFO_RANGE] == LR_HIGH ? hz * 8.0f : hz;            /* HIGH: to 1.6 kHz */
}

float Engine::srcValue(const Voice *v, int s) const
{
    switch (s) {
    case S_DIRECT: return 1.0f;
    case S_MODWHEEL: return ctl[1];
    case S_AFTERTOUCH: return v ? maxf(ctl[2], v->press) : ctl[2];
    case S_EXPR: return ctl[3];
    case S_BREATH: return ctl[4];
    case S_VELOCITY: return v ? v->vel : 0.0f;
    case S_KEYBOARD: return v ? clampf(((float)v->note - 60.0f) / 60.0f, -1.0f, 1.0f) : 0.0f;
    case S_LFO1P: return v ? 0.5f + 0.5f * v->l[0].out : 0.5f + 0.5f * lfoShow[0];
    case S_LFO1B: return v ? v->l[0].out : lfoShow[0];
    case S_LFO2P: return v ? 0.5f + 0.5f * v->l[1].out : 0.5f + 0.5f * lfoShow[1];
    case S_LFO2B: return v ? v->l[1].out : lfoShow[1];
    case S_AMPENV: return v ? v->e[0].v : 0.0f;
    case S_MENV1: return v ? v->e[1].v : 0.0f;
    case S_MENV2: return v ? v->e[2].v : 0.0f;
    case S_ANIM1: return animV[0];
    case S_ANIM2: return animV[1];
    case S_CV: return ctl[5];
    case S_LFO3P: return 0.5f + 0.5f * gOut[2];
    case S_LFO3B: return gOut[2];
    case S_LFO4P: return 0.5f + 0.5f * gOut[3];
    case S_LFO4B: return gOut[3];
    case S_BENDUP: return ctl[0];
    case S_BENDDN: return -ctl[0];
    case S_NOISE: return v ? v->srcNoise : gNoise;
    default: return 0.0f;
    }
}

void Engine::modulate(Voice &v)
{
    float *d = v.dst;
    std::memset(d, 0, sizeof v.dst);
    v.srcNoise = rndF(v.seed);
    for (int k = 0; k < 16; k++) {
        const int16_t *M = pt.v + P_MOD1 + k * MOD_N;
        int depth = M[MOD_DEPTH];
        if (depth == 0)
            continue;
        float a = srcValue(&v, M[MOD_SA]) * srcValue(&v, M[MOD_SB]);
        if (a == 0.0f)
            continue;
        int dst = M[MOD_DST];
        float x = (float)depth / 64.0f;
        switch (dst) {
        case D_O123PITCH: case D_O1PITCH: case D_O2PITCH: case D_O3PITCH: d[dst] += depthSemis((float)depth) * a; break;
        case D_FM12: case D_FM23: case D_FM31: case D_FMN1: d[dst] += 3.0f * x * std::fabs(x) * a; break;
        case D_O1VSYNC: case D_O2VSYNC: case D_O3VSYNC: d[dst] += x * a * 127.0f; break;
        case D_O1LEV: case D_O2LEV: case D_O3LEV: case D_NOISELEV: case D_RINGLEV: case D_FILTDRV: case D_FILTFREQ:
        case D_FILTRES: case D_O3FILT: case D_LFO1RATE: case D_LFO2RATE: d[dst] += x * a * 255.0f; break;
        case D_VCALEVEL: d[dst] += x * a; break;
        case D_NFILT: d[dst] += x * a * 4.0f; break;     /* (octaves) */
        default: d[dst] += x * a * 127.0f; break;        /* shapes (64 a side), times, sustains, pan, post drive */
        }
    }
}

/* the envelopes' one step (a sample) */
static inline void envStep(float &v, float &t, uint8_t &st, int &rep, float dly, float ka, float hold, float kd, float sus,
                           float kr, float dt, int reps)
{
    switch (st) {
    case 1:                                              /* DELAY */
        t += dt;
        if (t >= dly) {
            st = 2;
            t = 0.0f;
        }
        break;
    case 2:                                              /* ATTACK: an RC charging toward 1.3, cut off at 1 */
        v += (1.3f - v) * ka;
        if (v >= 1.0f) {
            v = 1.0f;
            st = hold > 0.0f ? 3 : 4;
            t = 0.0f;
        }
        break;
    case 3:
        t += dt;
        if (t >= hold) {
            st = 4;
            t = 0.0f;
        }
        break;
    case 4:                                              /* DECAY (and the sustain: it settles there) */
        v += (sus - v) * kd;
        t += dt;
        if (reps > 0 && (rep < reps || reps >= 127) && (v - sus) < 0.004f + 0.01f * (1.0f - sus)) {
            rep++;
            st = 2;
            t = 0.0f;
        }
        break;
    case 6:                                              /* RELEASE */
        v += (0.0f - v) * kr;
        if (v < 1e-4f) {
            v = 0.0f;
            st = 0;
        }
        break;
    default: break;
    }
}

/* --------------------------------------------------------------- the voice --- */
void Engine::voiceChunk(Voice &v, float *Lo, float *Ro)
{
    const int16_t *P = pt.v;
    const float dt = 1.0f / sr, cdt = (float)CH / sr;
    modulate(v);
    const float *d = v.dst;

    /* the envelopes' rates this chunk */
    float ka[3], kd[3], kr[3], sus[3], dly[3], hold[3];
    int reps[3];
    for (int k = 0; k < 3; k++) {
        const int16_t *E = P + (k == 0 ? P_AENV : k == 1 ? P_M1ENV : P_M2ENV);
        int offs = k == 0 ? 0 : k == 1 ? 3 : 6;
        float mA = d[(k == 0 ? D_AENV_A : k == 1 ? D_M1ENV_A : D_M2ENV_A)];
        float mD = d[(k == 0 ? D_AENV_D : k == 1 ? D_M1ENV_D : D_M2ENV_D)];
        float mR = d[(k == 0 ? D_AENV_R : k == 1 ? D_M1ENV_R : D_M2ENV_R)];
        float mDl = d[D_AENV_DLY + offs], mH = d[D_AENV_HLD + offs], mS = d[D_AENV_SUS + offs];
        ka[k] = 1.0f - std::exp(-dt * 1.466f / attSec((float)E[ENV_ATT] + mA));
        kd[k] = 1.0f - std::exp(-dt * 6.9f / decSec((float)E[ENV_DEC] + mD));
        kr[k] = 1.0f - std::exp(-dt * 6.9f / relSec((float)E[ENV_REL] + mR));
        sus[k] = clampf(((float)E[ENV_SUS] + mS) / 127.0f, 0.0f, 1.0f);
        dly[k] = dlySec((float)E[ENV_DLY] + mDl);
        hold[k] = holdSec((float)E[ENV_HOLD] + mH);
        reps[k] = E[ENV_REP];
    }
    /* LFO 1 and 2: rate, fade, slew */
    float lhz[2], lfadeT[2], lslew[2];
    for (int k = 0; k < 2; k++) {
        const int16_t *Q = P + (k ? P_LFO2 : P_LFO1);
        lhz[k] = lfoHz(k, d[k ? D_LFO2RATE : D_LFO1RATE]);
        lfadeT[k] = fadeSec((float)Q[LFO_FADE]);
        float s = (float)Q[LFO_SLEW] / 127.0f;
        lslew[k] = s <= 0.0f ? 1.0f : 1.0f - std::exp(-dt / (s * s * 0.35f / maxf(lhz[k], 0.05f) + 1e-5f));
    }
    /* the pitch: what doesn't move within the chunk */
    float common = (float)P[P_DIVERGE] / 127.0f * 0.25f;   /* (DIVERGE: up to a quarter tone) */
    /* DRIFT: each oscillator's slow wander */
    float drift = (float)P[P_DRIFT] / 127.0f;
    if (drift > 0.0f) {
        if (--v.driftN <= 0) {
            for (int k = 0; k < 3; k++)
                v.driftT[k] = rndF(v.seed) * 0.3f * drift;
            v.driftN = (int)((0.3f + 0.9f * (float)(rndU(v.seed) & 1023u) / 1023.0f) / cdt);
        }
        float kdr = 1.0f - std::exp(-cdt / 0.4f);
        for (int k = 0; k < 3; k++)
            v.drift[k] += (v.driftT[k] - v.drift[k]) * kdr;
    } else
        v.drift[0] = v.drift[1] = v.drift[2] = 0.0f;
    const int N = std::min(unisonN(), nVoices);
    float uni = N > 1 ? v.uniDet * (float)P[P_UNIDET] / 127.0f * 0.5f : 0.0f;   /* (semitones) */
    float fixed[3], base[3], e2s[3], l2s[3], bendS[3];
    for (int k = 0; k < 3; k++) {
        const int16_t *O = P + P_OSC1 + k * OSC_N;
        fixed[k] = (float)O[OSC_FIXNOTE];
        base[k] = (float)(12 * (O[OSC_RANGE] - 1) + O[OSC_COARSE]) + (float)O[OSC_FINE] * 0.01f + d[D_O123PITCH] +
                  d[D_O1PITCH + k] + v.divO[k] * common + v.drift[k] + uni;
        e2s[k] = env2Semis((float)O[OSC_ENV2]);
        l2s[k] = lfo2Semis((float)O[OSC_LFO2]);
        bendS[k] = ctl[0] * (float)O[OSC_BEND];
    }
    /* glide */
    float gk = (P[P_GLIDE_ON] && v.pitch != v.target) ? 1.0f - std::exp(-dt * 4.0f / glideSec((float)P[P_GLIDE])) : 1.0f;
    /* the oversampling: HIGH 4x, ECO 2x (halved from 88.2 kHz up) */
    int os = quality ? 4 : 2;
    if (sr > 80000.0f)
        os /= 2;
    if (sr > 160000.0f)
        os = 1;
    const float fos = sr * (float)os, ifos = 1.0f / fos;
    /* the oscillators' sets */
    OscSet set[3];
    float shBase[3], shEnv[3], shLfo[3];
    for (int k = 0; k < 3; k++) {
        const int16_t *O = P + P_OSC1 + k * OSC_N;
        OscSet &s = set[k];
        s.wave = O[OSC_WAVE];
        s.tab = tab;
        s.table = O[OSC_MORE];
        float vs = clampf((float)O[OSC_VSYNC] + d[D_O1VSYNC + k], 0.0f, 127.0f);
        s.ratio = 1.0f + vs / 16.0f;
        float de = (float)O[OSC_DENSE] / 127.0f;
        s.dense = de * 0.85f;
        float dd = (float)O[OSC_DDET] / 127.0f;
        s.ddet = 0.0005f + 0.03f * dd * dd;
        shBase[k] = ((float)O[OSC_SHMAN] + d[D_O1SHAPE + k] + d[D_O123SHAPE]) / 64.0f;
        shEnv[k] = (float)O[OSC_SHENV] / 64.0f;
        shLfo[k] = (float)O[OSC_SHLFO] / 64.0f;
    }
    /* the mixer */
    auto lvl = [](float x) { x = clampf(x, 0.0f, 1.0f); return x * std::sqrt(x); };
    float tgt[12];
    tgt[0] = lvl(((float)P[P_LVL1] + d[D_O1LEV]) / 255.0f);
    tgt[1] = lvl(((float)P[P_LVL2] + d[D_O2LEV]) / 255.0f);
    tgt[2] = lvl(((float)P[P_LVL3] + d[D_O3LEV]) / 255.0f);
    tgt[3] = lvl(((float)P[P_LVL_NOISE] + d[D_NOISELEV]) / 255.0f) * 0.9f;
    tgt[4] = lvl(((float)P[P_LVL_RING] + d[D_RINGLEV]) / 255.0f) * 1.2f;
    tgt[5] = clampf(((float)P[P_DRIVE] + d[D_FILTDRV]) / 255.0f, 0.0f, 1.0f);
    tgt[6] = clampf(((float)P[P_RESO] + d[D_FILTRES]) / 255.0f, 0.0f, 1.0f);
    tgt[7] = clampf(((float)P[P_POSTDRV] + d[D_FILTDIST]) / 127.0f, 0.0f, 1.0f);
    tgt[8] = clampf(((float)P[P_FOSC3] + d[D_O3FILT]) / 255.0f, 0.0f, 1.0f) * 5.0f;   /* octaves */
    tgt[9] = clampf((float)P[P_VCA_GAIN] / 127.0f, 0.0f, 1.0f);
    float panPos = clampf(v.pan * (float)P[P_UNISPREAD] / 127.0f + ((float)P[P_PAN] + d[D_PAN]) / 64.0f, -1.0f, 1.0f);
    tgt[10] = panPos;
    tgt[11] = clampf(d[D_VCALEVEL], -1.0f, 1.0f);
    if (!v.smOk) {
        for (int i = 0; i < 12; i++)
            v.sm[i] = tgt[i];
        v.smOk = true;
    }
    float cur[12], stp[12];
    for (int i = 0; i < 12; i++) {
        cur[i] = v.sm[i];
        stp[i] = (tgt[i] - v.sm[i]) * (1.0f / (float)CH);
        v.sm[i] = tgt[i];
    }
    /* the FM's depths (cycles per cycle) */
    float fm12 = maxf(d[D_FM12], 0.0f), fm23 = maxf(d[D_FM23], 0.0f), fm31 = maxf(d[D_FM31], 0.0f),
          fmn1 = maxf(d[D_FMN1], 0.0f);
    /* the oscillators heard or used (by the ring, the FM, Osc 3's filter mod): the others rest */
    bool need[3];
    need[0] = cur[0] > 0.0f || tgt[0] > 0.0f || tgt[4] > 0.0f || fm12 > 0.0f;
    need[1] = cur[1] > 0.0f || tgt[1] > 0.0f || tgt[4] > 0.0f || fm23 > 0.0f;
    need[2] = cur[2] > 0.0f || tgt[2] > 0.0f || fm31 > 0.0f || tgt[8] > 0.0f || cur[8] > 0.0f;
    /* the filter: what doesn't move within the chunk */
    const int fshape = P[P_FSHAPE];
    const bool f24 = P[P_SLOPE] != 0;
    float cutBase = (float)P[P_FREQ] + d[D_FILTFREQ];          /* 0..255: ten octaves and a bit from 20 Hz */
    float keyOct = (float)P[P_KEYTRK] / 127.0f * (v.pitch - 60.0f) / 12.0f;
    float fdiv = v.divF * (float)P[P_FDIVERGE] / 127.0f * 0.4f;
    float fenv1 = (float)P[P_FENV1] / 127.0f * 9.0f, fenva = (float)P[P_FENVA] / 127.0f * 9.0f;
    float flfo = (float)P[P_FLFO1] / 127.0f * 8.0f;
    float nflt = d[D_NFILT];
    const float hzMax = minf(0.36f * fos, 30000.0f);
    /* the noise's filters */
    float nlpv = clampf((float)P[P_NOISE_LPF] + fxdNL, 0.0f, 127.0f), nhpv = clampf((float)P[P_NOISE_HPF] + fxdNH, 0.0f, 127.0f);
    float nlp = onePole(180.0f * fexp2(nlpv / 127.0f * 7.0f), fos);
    float nhp = nhpv > 0.5f ? onePole(10.0f * fexp2(nhpv / 127.0f * 9.0f), fos) : 0.0f;
    /* the post drive's emphasis */
    const float kEm = onePole(900.0f, fos);
    /* the voice's level */
    float lvlPatch = VOICE_GAIN * fexp2(((float)P[P_PATCHLVL] - 64.0f) / 64.0f);
    if (N > 1)
        lvlPatch *= 1.0f / std::sqrt((float)N);
    const float steal = 1.0f / (0.004f * sr);
    const bool monoMode = mono();

    for (int s = 0; s < CH; s++) {
        for (int i = 0; i < 12; i++)
            cur[i] += stp[i];
        /* the envelopes */
        for (int k = 0; k < 3; k++) {
            Env &e = v.e[k];
            envStep(e.v, e.t, e.st, e.rep, dly[k], ka[k], hold[k], kd[k], sus[k], kr[k], dt, reps[k]);
        }
        float ae = v.e[0].v * v.e[0].vs, m1 = v.e[1].v * v.e[1].vs, m2 = v.e[2].v * v.e[2].vs;
        /* LFO 1, 2 */
        float lf[2];
        for (int k = 0; k < 2; k++) {
            const int16_t *Q = P + (k ? P_LFO2 : P_LFO1);
            Lfo &l = v.l[k];
            float raw;
            if (Q[LFO_COMMON] && !monoMode) {
                raw = gOut[k];
            } else {
                float ph = l.ph + lhz[k] * dt;
                if (ph >= 1.0f) {
                    ph -= std::floor(ph);
                    l.sh = rndF(v.seed);
                    if (Q[LFO_REP] > 0 && ++l.cycles >= Q[LFO_REP])
                        l.done = true;
                }
                l.ph = ph;
                raw = l.done ? 0.0f : lfoShape(Q[LFO_TYPE], ph, l.sh);
            }
            /* the fade */
            float f = 1.0f;
            if (lfadeT[k] > 0.0f) {
                l.fade += dt;
                float x = l.fade / lfadeT[k];
                switch (Q[LFO_FMODE]) {
                case FM_FADEIN: f = minf(x, 1.0f); break;
                case FM_FADEOUT: f = maxf(1.0f - x, 0.0f); break;
                case FM_GATEIN: f = x < 1.0f ? 0.0f : 1.0f; break;
                default: f = x < 1.0f ? 1.0f : 0.0f; break;
                }
            }
            l.out += (raw * f - l.out) * lslew[k];
            lf[k] = l.out;
        }
        /* glide */
        v.pitch += (v.target - v.pitch) * gk;
        /* the oscillators' increments and shapes this sample */
        float inc[3];
        for (int k = 0; k < 3; k++) {
            float note = fixed[k] > 0.0f ? fixed[k] : v.pitch;
            float semis = note + base[k] + bendS[k] + e2s[k] * m2 + l2s[k] * lf[1];
            inc[k] = hzOf(semis) * ifos;
            OscSet &st = set[k];
            st.shape = clampf(shBase[k] + shEnv[k] * m1 + shLfo[k] * lf[0], -1.0f, 1.0f);
            if (st.wave == W_MORE) {
                st.pos = (st.shape + 1.0f) * 2.0f;
                float h = 0.5f / maxf(inc[k] * (st.ratio > 1.0f ? st.ratio : 1.0f), 1e-6f);   /* harmonics that fit */
                int mip = 0;
                while (mip < WT_MIPS - 1 && (float)(WT_LEN / 2 >> mip) > h)
                    mip++;
                st.mip = mip;
            }
        }
        /* the filter's frequency this sample (before the audio-rate part) */
        float oct0 = cutBase * (10.2f / 255.0f) + keyOct + fenv1 * m1 + fenva * ae +
                     flfo * lf[0] + fdiv;
        float res = clampf(cur[6], 0.0f, 1.0f);
        float k1 = 1.6f - 1.85f * std::pow(res, 0.9f);   /* (from about 85 % it sings on its own) */
        float k2 = fshape == FS_LP ? 1.6f - 0.65f * res : 1.6f - 0.9f * res;
        float drv = clampf(cur[5], 0.0f, 1.0f), pdr = clampf(cur[7], 0.0f, 1.0f);
        float dGain = 1.0f + 8.0f * drv * drv + 2.0f * drv, dBias = 0.25f * drv, dOff = softT(dBias),
              dComp = 2.0f / (1.0f + 2.5f * drv);
        float pGain = 1.0f + 14.0f * pdr, pComp = 1.0f / (1.0f + 1.6f * pdr);
        float y4[4];
        /* (the cutoff moved at audio rate only by Osc 3 or the noise: else once a sample) */
        const bool audioCut = cur[8] > 0.0f || stp[8] != 0.0f || nflt != 0.0f;
        auto tanOf = [&](float oct) {
            float hz = minf(20.0f * fexp2(clampf(oct, -1.0f, 11.0f)), hzMax);
            float w = PI * hz * ifos;
            return w * (1.0f + w * w * (0.33333f + w * w * (0.13333f + w * w * 0.05397f)));
        };
        float gFixed = audioCut ? 0.0f : tanOf(oct0);
        for (int q = 0; q < os; q++) {
            /* the oscillators: each modulating the next (FM), on the last sample's outputs */
            float n0 = rndF(v.seed);
            float i1 = inc[0] * (1.0f + fm31 * v.last[2] + fmn1 * n0);
            float i2 = inc[1] * (1.0f + fm12 * v.last[0]);
            float i3 = inc[2] * (1.0f + fm23 * v.last[1]);
            float o1 = need[0] ? v.o[0].tick(set[0], i1 < 0.0f ? -i1 : i1) : 0.0f;
            float o2 = need[1] ? v.o[1].tick(set[1], i2 < 0.0f ? -i2 : i2) : 0.0f;
            float o3 = need[2] ? v.o[2].tick(set[2], i3 < 0.0f ? -i3 : i3) : 0.0f;
            v.last[0] = o1;
            v.last[1] = o2;
            v.last[2] = o3;
            /* the noise, its filters */
            v.nLp += nlp * (n0 - v.nLp);
            float nz = v.nLp;
            if (nhp > 0.0f) {
                v.nHp += nhp * (nz - v.nHp);
                nz -= v.nHp;
            }
            float x = cur[0] * o1 + cur[1] * o2 + cur[2] * o3 + cur[3] * nz * 1.6f + cur[4] * o1 * o2;
            x += 1e-5f * n0;                             /* (the circuit's own hiss: what starts the filter singing) */
            /* OVERDRIVE: before the filter, fattening */
            x = (softT(x * dGain * 0.5f + dBias) - dOff) * dComp;
            /* the filter */
            float g = audioCut ? tanOf(oct0 + cur[8] * o3 + nflt * nz) : gFixed;   /* tan(pi f / fs) */
            /* a state-variable stage (zero-delay); its damping grows with the band's level, as an OTA's
             * transconductance falls when driven: the resonance rounds off, and past about 85 % it sings by itself
             * at a steady level whatever its pitch */
            auto svf = [&](float in, float kk, float *st, float &lp, float &bp, float &hp) {
                kk += 0.3f * st[2] * st[2];
                float a1 = 1.0f / (1.0f + g * (g + kk)), a2 = g * a1, a3 = g * a2;
                float v3 = in - st[1], v1 = a1 * st[0] + a2 * v3, v2 = st[1] + a2 * st[0] + a3 * v3;
                st[0] = 2.0f * v1 - st[0];
                st[1] = 2.0f * v2 - st[1];
                st[0] = 3.0f * softT(st[0] * (1.0f / 3.0f));   /* (and a hard limit far up: never runs away) */
                st[2] = v1;
                lp = v2;
                bp = v1;
                hp = in - kk * v1 - v2;
            };
            float lp, bp, hp, y;
            svf(x, k1, v.s1, lp, bp, hp);
            if (fshape == FS_LP)
                y = lp;
            else if (fshape == FS_BP)
                y = bp * (0.35f + 0.65f * maxf(k1, 0.03f)) * 1.6f;
            else
                y = hp;
            if (f24) {
                float lp2, bp2, hp2;
                svf(y, k2, v.s2, lp2, bp2, hp2);
                y = fshape == FS_LP ? lp2 : fshape == FS_BP ? bp2 * (0.5f + 0.5f * k2) * 1.4f : hp2;
                if (fshape == FS_LP)
                    y *= 1.0f + 0.55f * res;
            } else if (fshape == FS_LP)
                y *= 1.0f + 0.3f * res;
            /* POST DRIVE: after the filter, before the VCA: bright, gritty */
            if (pdr > 0.0005f) {
                v.emph += kEm * (y - v.emph);
                float e = y + 1.6f * (y - v.emph);       /* (the highs lifted into the clipper) */
                float z = softT(e * pGain * 0.6f) * 1.4f;
                v.deemph += kEm * (z - v.deemph);
                z = z - 0.45f * (z - v.deemph);          /* (and partly let down again: it stays brighter) */
                float m = minf(pdr * 6.0f, 1.0f);
                y = y + m * (z * pComp * 1.5f - y);
            }
            y4[q] = y;
        }
        float y;
        if (os == 4) {
            float c0 = v.h2.down(y4[0], y4[1]);         /* (in this order: an argument list has none) */
            float c1 = v.h2.down(y4[2], y4[3]);
            y = v.h1.down(c0, c1);
        }
        else if (os == 2)
            y = v.h1.down(y4[0], y4[1]);
        else
            y = y4[0];
        /* the VCA */
        float amp = clampf(ae * cur[9] + cur[11], 0.0f, 1.5f);
        if (v.fade < 1.0f) {                             /* taken: out on a smooth curve, then the note it was taken for */
            v.fade -= steal;
            if (v.fade <= 0.0f) {
                v.fade = 1.0f;
                for (auto &e : v.e) {
                    e.v = 0.0f;
                    e.st = ES_IDLE;
                }
                if (v.pend) {
                    v.pend = false;
                    v.active = false;
                    startVoice(v, v.pKey, v.pNote, v.pVel, v.pUni, false);
                } else
                    v.active = false;
                return;
            }
            float f = v.fade;
            amp *= f * f * (3.0f - 2.0f * f);
        }
        float out = y * amp * lvlPatch;
        float p = clampf(cur[10], -1.0f, 1.0f);
        float gl = std::sqrt(0.5f * (1.0f - p)) * 1.41421f, gr = std::sqrt(0.5f * (1.0f + p)) * 1.41421f;
        Lo[s] += out * gl;
        Ro[s] += out * gr;
    }
    /* finished: its amp envelope at its end */
    if (v.e[0].st == ES_IDLE && !v.pend && !v.gate)
        v.active = false;
    for (int k = 0; k < 3; k++) {
        v.s1[k] = flushDn(v.s1[k]);
        v.s2[k] = flushDn(v.s2[k]);
    }
    v.nLp = flushDn(v.nLp);
    v.nHp = flushDn(v.nHp);
    v.emph = flushDn(v.emph);
    v.deemph = flushDn(v.deemph);
    v.h1.flush();
    v.h2.flush();
}

/* ------------------------------------------------------------------ render --- */
void Engine::chunk()
{
    const float dt = (float)CH / sr;
    /* the wheels, pressure, pedals and CV glide (5 ms): no steps in the pitch, no crackle in a filter */
    {
        const float in[6] = {bendV, mwV, atV, exV, brV, cvV}, kc = 1.0f - std::exp(-dt / 0.005f);
        for (int i = 0; i < 6; i++)
            ctl[i] = flushDn(ctl[i] + (in[i] - ctl[i]) * kc);
    }
    /* ANIMATE: up and down at their own speeds */
    for (int k = 0; k < 2; k++) {
        float tg = (anim[k] || animHeld[k]) ? 1.0f : 0.0f;
        float T = animSec((float)pt.v[tg > animV[k] ? (k ? P_AN2_ATT : P_AN1_ATT) : (k ? P_AN2_REL : P_AN1_REL)]);
        animV[k] += (tg - animV[k]) * (1.0f - std::exp(-dt / T));
        animV[k] = flushDn(animV[k]);
    }
    clockChunk();
    gNoise = rndF(gSeed);
    /* the global LFOs: 1 and 2's common phase, 3 and 4 */
    for (int k = 0; k < 4; k++) {
        float hz;
        int type;
        float slew = 0.0f;
        if (k < 2) {
            const int16_t *Q = pt.v + (k ? P_LFO2 : P_LFO1);
            hz = lfoHz(k, 0.0f);
            type = Q[LFO_TYPE];
            slew = (float)Q[LFO_SLEW] / 127.0f;
        } else {
            const int16_t *Q = pt.v + (k == 2 ? P_LFO3 : P_LFO4);
            type = Q[L34_WAVE];
            slew = (float)Q[L34_SLEW] / 127.0f;
            if (Q[L34_SYNC] > 0) {
                float beats = LFOSYNC_BEATS[clampi(Q[L34_SYNC] - 1, 0, NLFOSYNC - 1)];
                hz = bpmCur / 60.0f / beats;
                /* (locked to the clock: its phase from the position) */
                double ph = pos / (double)beats + (Q[L34_PHASE] > 0 ? (double)(Q[L34_PHASE] - 1) / 120.0 : 0.0);
                float nph = (float)(ph - std::floor(ph));
                if (nph < gPh[k])
                    gSh[k] = rndF(gSeed);
                gPh[k] = nph;
                hz = 0.0f;
            } else
                hz = 0.01f * fexp2(clampf((float)Q[L34_RATE] + (k == 2 ? fxdL3 : fxdL4), 0.0f, 127.0f) * (14.0f / 127.0f));
        }
        if (hz > 0.0f) {
            gPh[k] += hz * dt;
            if (gPh[k] >= 1.0f) {
                gPh[k] -= std::floor(gPh[k]);
                gSh[k] = rndF(gSeed);
            }
        }
        float raw = lfoShape(type, gPh[k], gSh[k]);
        float ks = slew <= 0.0f ? 1.0f : 1.0f - std::exp(-dt / (slew * slew * 0.35f / maxf(hz, 0.05f) + 1e-5f));
        gOut[k] += (raw - gOut[k]) * ks;
    }
    /* the voices */
    std::memset(outL, 0, sizeof outL);
    std::memset(outR, 0, sizeof outR);
    int newest = -1;
    uint32_t na = 0;
    for (int i = 0; i < MAXV; i++) {
        Voice &v = V[i];
        if (!v.active)
            continue;
        voiceChunk(v, outL, outR);
        if (v.active && v.age >= na) {
            na = v.age;
            newest = i;
        }
    }
    /* the LFOs as the panel shows them: the newest voice's */
    for (int k = 0; k < 2; k++)
        lfoShow[k] = newest >= 0 ? V[newest].l[k].out : gOut[k];
    lfoShow[2] = gOut[2];
    lfoShow[3] = gOut[3];
    /* the effects: their settings, then the FX matrix (sources from the newest voice) */
    {
        const int16_t *P = pt.v;
        float fd[NFXDST] = {};
        const Voice *nv = newest >= 0 ? &V[newest] : nullptr;
        for (int k = 0; k < 4; k++) {
            const int16_t *M = P + P_FXMOD1 + k * MOD_N;
            if (M[MOD_DEPTH] == 0)
                continue;
            float a = srcValue(nv, M[MOD_SA]) * srcValue(nv, M[MOD_SB]);
            fd[clampi(M[MOD_DST], 0, NFXDST - 1)] += (float)M[MOD_DEPTH] / 64.0f * a * 127.0f;
        }
        auto u = [](float x) { return clampf(x / 127.0f, 0.0f, 1.0f); };
        FxSet &s = fxs;
        s.dist = u((float)P[P_DIST] + fd[FD_DIST]);
        s.chType = P[P_CH_TYPE];
        s.chMode = P[P_CH_MODE];
        s.chRate = u((float)P[P_CH_RATE] + fd[FD_CH_RATE]);
        float cl = u((float)P[P_CH_LEVEL] + fd[FD_CH_LEV]);
        s.chLevel = cl * cl;
        s.chDepth = u((float)P[P_CH_DEPTH] + fd[FD_CH_DEPTH]);
        s.chFb = clampf(((float)P[P_CH_FB] + fd[FD_CH_FB] * 0.5f) / 64.0f, -1.0f, 1.0f);
        s.chLp = u((float)P[P_CH_LP]);
        s.chHp = u((float)P[P_CH_HP]);
        /* the delay's time: free (127 about 1.4 s) or in step with the clock (shortened to fit 1.4 s) */
        float t;
        if (P[P_DL_SYNC]) {
            int i = clampi(P[P_DL_SYNCRATE], 0, NDLYSYNC - 1);
            t = LFOSYNC_BEATS[DLYSYNC_AT + i] * 60.0f / maxf(bpmCur, 20.0f);
            while (t > 1.4f && i < NDLYSYNC - 1)
                t = LFOSYNC_BEATS[DLYSYNC_AT + ++i] * 60.0f / maxf(bpmCur, 20.0f);
            t += fd[FD_DL_TIME] / 127.0f * 0.25f * t;
        } else {
            float x = u((float)P[P_DL_TIME] + fd[FD_DL_TIME]);
            t = 0.004f + 1.396f * x * x;
        }
        s.dlTime = clampf(t, 0.002f, 1.4f);
        s.dlFb = u((float)P[P_DL_FB] + fd[FD_DL_FB]) * 0.997f;
        float dlv = u((float)P[P_DL_LEVEL] + fd[FD_DL_LEV]);
        s.dlLevel = dlv * dlv;
        s.dlLp = u((float)P[P_DL_LPD] + fd[FD_DL_LDAMP]);
        s.dlHp = u((float)P[P_DL_HPD] + fd[FD_DL_HDAMP]);
        s.dlSlew = u((float)P[P_DL_SLEW] + fd[FD_DL_SLEW]);
        s.dlWidth = u((float)P[P_DL_WIDTH] + fd[FD_DL_WIDTH]);
        int ri = clampi((int)std::lround((float)P[P_DL_RATIO] + fd[FD_DL_RATIO] * (10.0f / 127.0f)), 0, NDLYRATIO - 1);
        s.dlRatioL = DLYRATIO_L[ri];
        s.dlRatioR = DLYRATIO_R[ri];
        s.dlMode = P[P_DL_TMODE];
        /* the reverb: TIME 0.3 .. 14 s, and toward an endless tail at the top */
        float rt = u((float)P[P_RV_TIME] + fd[FD_RV_TIME]);
        s.rvTime = 0.3f * fexp2(rt * 5.5f) + (rt > 0.94f ? (rt - 0.94f) * 900.0f : 0.0f);
        float rl = u((float)P[P_RV_LEVEL] + fd[FD_RV_LEV]);
        s.rvLevel = rl * rl * 1.3f;
        s.rvSize = u((float)P[P_RV_SIZE] + fd[FD_RV_SIZE]);
        s.rvPre = u((float)P[P_RV_PRE]);
        s.rvLpd = u((float)P[P_RV_LPD] + fd[FD_RV_LDAMP]);
        s.rvHpd = u((float)P[P_RV_HPD] + fd[FD_RV_HDAMP]);
        s.rvMDepth = u((float)P[P_RV_MDEPTH]);
        s.rvMRate = u((float)P[P_RV_MRATE]);
        s.rvLp = u((float)P[P_RV_LP] + fd[FD_RV_LPASS]);
        s.rvHp = u((float)P[P_RV_HP] + fd[FD_RV_HPASS]);
        float wv = u((float)P[P_FX_WET]), dv = u((float)P[P_FX_DRY]);
        s.wet = wv * wv;
        s.dry = dv * dv;
        s.route = P[P_FX_ROUTE];
        s.bypass = P[P_FX_BYPASS] != 0;
        fx.set(s);
        /* (the FX matrix's LFO and noise destinations act on the patch's own: LFO 3 / 4 rate, noise filters) */
        fxdL3 = fd[FD_LFO3RATE];
        fxdL4 = fd[FD_LFO4RATE];
        fxdNL = fd[FD_NOISELPF];
        fxdNH = fd[FD_NOISEHPF];
    }
    /* the voices' DC let go (8 Hz), then the effects */
    const float kdc = 1.0f - TWOPI * 8.0f / sr;
    for (int i = 0; i < CH; i++) {
        float xl = outL[i], xr = outR[i];
        dcL = xl - dcxL + kdc * dcL;
        dcRr = xr - dcxR + kdc * dcRr;
        dcxL = xl;
        dcxR = xr;
        outL[i] = dcL;
        outR[i] = dcRr;
    }
    dcL = flushDn(dcL);
    dcRr = flushDn(dcRr);
    fx.process(outL, outR, CH);
}

void Engine::render(float *L, float *R, int n)
{
    while (n > 0) {
        if (gpos >= CH) {
            chunk();
            gpos = 0;
        }
        int k = std::min(CH - gpos, n);
        std::memcpy(L, outL + gpos, sizeof(float) * (size_t)k);
        std::memcpy(R, outR + gpos, sizeof(float) * (size_t)k);
        gpos += k;
        L += k;
        R += k;
        n -= k;
    }
}

int Engine::voicesSounding() const
{
    int n = 0;
    for (const auto &v : V)
        n += v.active;
    return n;
}
} // namespace pk
