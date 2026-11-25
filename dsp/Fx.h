/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK's effects, the Peak's chain: the analogue DISTORTION after the VCAs on the sum of the voices, then the three
 * time effects (CHORUS, DELAY, REVERB) in parallel or in series in any order, mixed with DryLevel and WetLevel.
 *
 *   distortion  an analogue overdrive (asymmetric, so it fattens before it fizzes), its edge rounded off as it rises
 *   chorus      three programs (two taps, four taps, an ensemble of three on two LFOs), as a chorus, a flanger or a
 *               "phlanger" (short delays, close to a phaser); feedback either way; LoPass and HiPass on its output
 *   delay       up to 1.4 s (and the slower, darker lo-fi modes beyond), L/R ratios for ping-pong, damping inside the
 *               loop, its time slewed (the tape-like pitch bend when it moves), the width
 *   reverb      an 8-line feedback delay network (Hadamard), its input diffused through four allpasses, every line's
 *               length slowly modulated (no metallic ringing), damping inside the loop, early reflections that grow
 *               with the size, a pre-delay up to half a second, LoPass and HiPass on its output
 * All per sample at the host's rate; their settings ramp across a 16-sample chunk. */
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <algorithm>
#include <vector>

namespace pk
{
/* a one-pole low-pass / high-pass coefficient for a frequency */
inline float onePole(float hz, float sr) { return 1.0f - std::exp(-TWOPI * clampf(hz, 1.0f, 0.45f * sr) / sr); }

/* a delay line with a cubic read */
struct Line {
    std::vector<float> b;
    int mask = 0, w = 0;
    void init(int minLen)
    {
        int n = 1;
        while (n < minLen + 4)
            n <<= 1;
        b.assign((size_t)n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear()
    {
        std::fill(b.begin(), b.end(), 0.0f);
        w = 0;
    }
    inline void push(float x)
    {
        b[(size_t)w] = x;
        w = (w + 1) & mask;
    }
    /* d samples back from the newest (d >= 1) */
    inline float tap(int d) const { return b[(size_t)((w - d) & mask)]; }
    inline float read(float d) const
    {
        int i = (int)d;
        float f = d - (float)i;
        float y0 = tap(i - 1 < 1 ? 1 : i - 1), y1 = tap(i < 1 ? 1 : i), y2 = tap(i + 1), y3 = tap(i + 2);
        float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

/* the effects' settings (their natural units), set every chunk */
struct FxSet {
    float dist = 0.0f;                                   /* 0..1 */
    int chType = 1, chMode = 0;
    float chRate = 0.5f, chLevel = 0.0f, chDepth = 0.5f, chFb = 0.0f, chLp = 0.7f, chHp = 0.02f;
    float dlTime = 0.35f, dlFb = 0.5f, dlLevel = 0.0f, dlLp = 0.67f, dlHp = 0.0f, dlSlew = 0.25f, dlWidth = 1.0f;
    float dlRatioL = 1.0f, dlRatioR = 1.0f;
    int dlMode = 0;
    float rvTime = 0.7f, rvLevel = 0.0f, rvSize = 0.5f, rvPre = 0.3f, rvLpd = 0.4f, rvHpd = 0.0f, rvMDepth = 0.5f,
          rvMRate = 0.03f, rvLp = 0.58f, rvHp = 0.0f;
    float wet = 1.0f, dry = 1.0f;
    int route = 0;
    bool bypass = false;
};

class Fx
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        const float k = sr / 44100.0f;
        chL.init((int)(0.06f * sr));
        chR.init((int)(0.06f * sr));
        dlL.init((int)(8.6f * sr));
        dlR.init((int)(8.6f * sr));
        pre.init((int)(0.55f * sr));
        er.init((int)(0.2f * sr));
        static const int AP[4] = {142, 107, 379, 277};
        for (int i = 0; i < 4; i++) {
            apLen[i] = (int)(AP[i] * k * 1.35f);
            ap[i].init(apLen[i] + 2);
        }
        static const float LEN[8] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.5f, 67.3f, 73.1f};   /* ms at size 1 */
        for (int i = 0; i < 8; i++) {
            baseLen[i] = LEN[i] * 0.001f * sr;
            fdn[i].init((int)(baseLen[i] * 1.75f + 64.0f * k + 8));
        }
        clear();
    }
    void clear()
    {
        chL.clear();
        chR.clear();
        dlL.clear();
        dlR.clear();
        pre.clear();
        er.clear();
        for (auto &a : ap)
            a.clear();
        for (auto &l : fdn)
            l.clear();
        for (auto &v : lp)
            v = 0.0f;
        for (auto &v : hp)
            v = 0.0f;
        std::memset(st, 0, sizeof st);
        dlTimeNow = -1.0f;
        lofiHold[0] = lofiHold[1] = 0.0f;
        lofiCount = 0;
        chFbL = chFbR = chLpL = chLpR = chHpL = chHpR = 0.0f;
        for (int c = 0; c < 2; c++) {
            dlLpS[c] = dlHpS[c] = dlOut[c] = rvOutLp[c] = rvOutHp[c] = 0.0f;
        }
        for (auto &f : fdnOut)
            f = 0.0f;
        rvTail = 0.0f;
        distWas = 0.0f;
    }

    void set(const FxSet &s) { want = s; }

    /* the chain over n samples (in place). L, R: the voices' sum */
    void process(float *L, float *R, int n)
    {
        const FxSet &s = want;
        /* the distortion first (not bypassed by BYPASS: it is analogue on the Peak) */
        float dist = clampf(s.dist, 0.0f, 1.0f);
        if (dist > 0.0005f || distWas > 0.0005f) {
            float dg = 1.0f + 22.0f * dist * dist, bias = 0.22f * dist;
            float comp = 1.25f * std::pow(dg, -0.62f);       /* (louder as it drives, but not by much) */
            float tone = onePole(15000.0f - 8500.0f * dist, sr);
            float db = softT(bias);
            for (int i = 0; i < n; i++) {
                for (int c = 0; c < 2; c++) {
                    float *x = c ? R : L;
                    float u = x[i] * dg;
                    float y = (softT(u + bias) - db) * comp;
                    float &z = st[c][0];
                    z += tone * (y - z);
                    /* (its DC, the asymmetry's, let go) */
                    float &dx = st[c][1], &dy = st[c][2];
                    float o = z - dx + 0.9995f * dy;
                    dx = z;
                    dy = o;
                    float mixd = clampf(dist * 40.0f, 0.0f, 1.0f);    /* (from off to on without a step) */
                    x[i] = x[i] + mixd * (o - x[i]);
                }
            }
        }
        distWas = dist;
        if (s.bypass && !bypassWas) {
            /* (into bypass: the time effects stopped; out of it they start empty) */
        }
        if (s.bypass) {
            bypassWas = true;
            return;
        }
        if (bypassWas) {
            bypassWas = false;
            float keep = dlTimeNow;
            clear();
            dlTimeNow = keep;
        }
        float wet = s.wet, dry = s.dry;
        for (int i = 0; i < n; i++) {
            float inL = L[i], inR = R[i];
            float xl = inL, xr = inR, accL = 0.0f, accR = 0.0f;
            auto run = [&](int which, float il, float ir, float &ol, float &orr) {
                if (which == 0)
                    chorus(il, ir, ol, orr);
                else if (which == 1)
                    delay(il, ir, ol, orr);
                else
                    reverb(il, ir, ol, orr);
            };
            if (s.route == FR_PARALLEL) {
                for (int k = 0; k < 3; k++) {
                    float ol, orr;
                    run(k, inL, inR, ol, orr);
                    accL += ol;
                    accR += orr;
                }
            } else {
                static const int ORDER[NFXROUTE][3] = {{0, 1, 2}, {1, 2, 0}, {1, 0, 2}, {2, 1, 0}, {2, 0, 1}, {0, 1, 2}, {0, 2, 1}};
                for (int k = 0; k < 3; k++) {
                    float ol, orr;
                    run(ORDER[s.route][k], xl, xr, ol, orr);
                    xl += ol;
                    xr += orr;
                }
                accL = xl - inL;
                accR = xr - inR;
            }
            L[i] = dry * inL + wet * accL;
            R[i] = dry * inR + wet * accR;
        }
        for (auto &v : lp)
            v = flushDn(v);
        for (auto &v : hp)
            v = flushDn(v);
    }

private:
    float sr = 44100.0f;
    FxSet want;
    float distWas = 0.0f;
    bool bypassWas = false;
    float st[2][4] = {};

    /* ------------------------------------------------------------------ chorus --- */
    Line chL, chR;
    float chPh = 0.0f, chPh2 = 0.0f, chFbL = 0.0f, chFbR = 0.0f;
    float chLpL = 0, chLpR = 0, chHpL = 0, chHpR = 0;
    void chorus(float il, float ir, float &ol, float &orr)
    {
        const FxSet &s = want;
        if (s.chLevel <= 0.0001f && std::fabs(chFbL) + std::fabs(chFbR) < 1e-7f) {
            chL.push(il);
            chR.push(ir);
            ol = orr = 0.0f;
            return;
        }
        float hz = 0.03f * fexp2(s.chRate * 8.5f);          /* 0.03 .. 11 Hz */
        chPh += hz / sr;
        chPh -= std::floor(chPh);
        chPh2 += hz * 6.6f / sr;
        chPh2 -= std::floor(chPh2);
        float base, exc;
        if (s.chMode == CM_FLANGER) {
            base = 0.0012f;
            exc = 0.0038f * s.chDepth;
        } else if (s.chMode == CM_PHLANGER) {
            base = 0.00025f;
            exc = 0.0011f * s.chDepth;
        } else {
            base = s.chType == 2 ? 0.0105f : 0.0072f;
            exc = 0.0055f * s.chDepth;
        }
        auto lfo = [&](float off) {                         /* a sine, and in the ensemble a quick shimmer over it */
            float v = 0.5f + 0.5f * sin2pi(chPh + off);
            if (s.chType == 2)
                v = 0.82f * v + 0.18f * (0.5f + 0.5f * sin2pi(chPh2 + off * 1.7f));
            return v;
        };
        float fb = s.chFb * (s.chMode == CM_CHORUS ? 0.7f : 0.93f);
        chL.push(il + fb * chFbL);
        chR.push(ir + fb * chFbR);
        float wl = 0.0f, wr = 0.0f;
        int taps = s.chType == 0 ? 1 : s.chType == 1 ? 2 : 3;
        for (int t = 0; t < taps; t++) {
            float o = (float)t / (float)taps;
            float dl = (base + exc * lfo(o)) * sr, dr = (base + exc * lfo(o + 0.5f / (float)taps + 0.25f)) * sr;
            wl += chL.read(maxf(dl, 1.5f));
            wr += chR.read(maxf(dr, 1.5f));
        }
        wl *= 1.0f / (float)taps;
        wr *= 1.0f / (float)taps;
        chFbL = flushDn(wl);
        chFbR = flushDn(wr);
        if (s.chMode == CM_PHLANGER) {                      /* (against the dry: the notches of a phaser) */
            wl = wl - il * 0.7f;
            wr = wr - ir * 0.7f;
        }
        float klp = onePole(400.0f * fexp2(s.chLp * 5.7f), sr), khp = onePole(15.0f * fexp2(s.chHp * 7.0f), sr);
        chLpL += klp * (wl - chLpL);
        chLpR += klp * (wr - chLpR);
        chHpL += khp * (chLpL - chHpL);
        chHpR += khp * (chLpR - chHpR);
        float g = s.chLevel * 1.4f;
        ol = (chLpL - chHpL) * g;
        orr = (chLpR - chHpR) * g;
    }

    /* ------------------------------------------------------------------- delay --- */
    Line dlL, dlR;
    float dlTimeNow = -1.0f, dlLpS[2] = {}, dlHpS[2] = {}, dlOut[2] = {};
    float lofiHold[2] = {};
    int lofiCount = 0;
    void delay(float il, float ir, float &ol, float &orr)
    {
        const FxSet &s = want;
        static const int DIV[NDTMODE] = {1, 2, 3, 4, 6};
        int dv = DIV[s.dlMode];
        float target = clampf(s.dlTime, 0.002f, 1.4f) * (float)dv;
        if (dlTimeNow < 0.0f)
            dlTimeNow = target;
        float tau = 0.0008f + 1.6f * (1.0f - s.dlSlew) * (1.0f - s.dlSlew);
        dlTimeNow += (target - dlTimeNow) * (1.0f - std::exp(-1.0f / (tau * sr)));
        if (s.dlLevel <= 0.0001f && std::fabs(dlOut[0]) + std::fabs(dlOut[1]) < 1e-6f) {
            dlL.push(0.0f);
            dlR.push(0.0f);
            ol = orr = 0.0f;
            return;
        }
        float in = 0.5f * (il + ir);
        bool pp = s.dlRatioL != s.dlRatioR;
        float fb = s.dlFb;
        /* the loop's damping */
        float klp = onePole(350.0f * fexp2(s.dlLp * 6.0f), sr), khp = onePole(8.0f * fexp2(s.dlHp * 7.5f), sr);
        float y[2];
        for (int c = 0; c < 2; c++) {
            float v = dlOut[c];
            dlLpS[c] += klp * (v - dlLpS[c]);
            dlHpS[c] += khp * (dlLpS[c] - dlHpS[c]);
            y[c] = dlLpS[c] - dlHpS[c];
        }
        /* ping-pong: the echoes cross over when the sides differ */
        float wl = (pp ? in : 0.5f * (il + in)) + fb * (pp ? y[1] : y[0]);
        float wr = (pp ? 0.0f : 0.5f * (ir + in)) + fb * (pp ? y[0] : y[1]);
        wl = softT(wl * 0.8f) * 1.25f;                      /* (a gentle limit: a full loop never runs away) */
        wr = softT(wr * 0.8f) * 1.25f;
        if (dv > 1) {                                       /* the slower modes: a lower rate, fewer bits */
            if (++lofiCount >= dv) {
                lofiCount = 0;
                float q = s.dlMode >= DT_QUAD ? (s.dlMode == DT_HEX ? 48.0f : 160.0f) : 4000.0f;
                lofiHold[0] = std::round(wl * q) / q;
                lofiHold[1] = std::round(wr * q) / q;
            }
            wl = lofiHold[0];
            wr = lofiHold[1];
        }
        dlL.push(wl);
        dlR.push(wr);
        float tl = dlTimeNow * s.dlRatioL * sr, tr = dlTimeNow * s.dlRatioR * sr;
        dlOut[0] = flushDn(dlL.read(maxf(tl, 2.0f)));
        dlOut[1] = flushDn(dlR.read(maxf(tr, 2.0f)));
        float a = dlOut[0], b = dlOut[1];
        if (dv > 1) {                                       /* (the stepped rate rounded off) */
            float k = onePole(0.42f * sr / (float)dv, sr);
            st[0][3] += k * (a - st[0][3]);
            st[1][3] += k * (b - st[1][3]);
            a = st[0][3];
            b = st[1][3];
        }
        float mid = 0.5f * (a + b), side = 0.5f * (a - b) * s.dlWidth;
        float g = s.dlLevel;
        ol = (mid + side) * g;
        orr = (mid - side) * g;
    }

    /* ------------------------------------------------------------------ reverb --- */
    Line pre, er, ap[4], fdn[8];
    int apLen[4] = {};
    float baseLen[8] = {};
    float lp[8] = {}, hp[8] = {}, fdnOut[8] = {}, rvPh[8] = {};
    float rvOutLp[2] = {}, rvOutHp[2] = {}, rvTail = 0.0f;
    void reverb(float il, float ir, float &ol, float &orr)
    {
        const FxSet &s = want;
        float in = 0.5f * (il + ir);
        pre.push(in);
        if (s.rvLevel <= 0.0001f && rvTail < 1e-6f) {
            ol = orr = 0.0f;
            return;
        }
        float x = pre.read(maxf(s.rvPre * 0.5f * sr, 1.0f));
        er.push(x);
        /* the input diffused */
        static const float G[4] = {0.72f, 0.72f, 0.64f, 0.64f};
        for (int i = 0; i < 4; i++) {
            float d = ap[i].tap(apLen[i]);
            float v = x + G[i] * d;
            ap[i].push(v);
            x = d - G[i] * v;
        }
        /* the early reflections: more and stronger as the room grows */
        float sz = 0.32f + 1.4f * s.rvSize;
        static const float ET[8] = {0.0043f, 0.0097f, 0.0151f, 0.0213f, 0.0289f, 0.0347f, 0.0431f, 0.0517f};
        float erL = 0.0f, erR = 0.0f;
        for (int k = 0; k < 8; k++) {
            float t = er.tap(maxf(1.0f, ET[k] * sz * sr));
            float gk = (0.55f - 0.05f * (float)k) * (0.25f + 0.75f * s.rvSize);
            if (k & 1)
                erR += t * gk;
            else
                erL += t * gk;
        }
        /* the network */
        float rt = s.rvTime;                                /* seconds */
        float klp = onePole(700.0f * fexp2(s.rvLpd * 4.8f), sr), khp = onePole(10.0f * fexp2(s.rvHpd * 6.5f), sr);
        float mhz = 0.07f + 3.2f * s.rvMRate, mdep = s.rvMDepth * 14.0f * (sr / 44100.0f);
        float v[8];
        for (int k = 0; k < 8; k++) {
            rvPh[k] += mhz * (0.71f + 0.083f * (float)k) / sr;
            rvPh[k] -= std::floor(rvPh[k]);
            float len = baseLen[k] * sz + mdep * (1.0f + sin2pi(rvPh[k])) + 2.0f;
            float y = fdn[k].read(len);
            float gk = fexp2(-3.0f * 3.3219f * len / (rt * sr));   /* (each line: -60 dB in the time) */
            lp[k] += klp * (y - lp[k]);
            hp[k] += khp * (lp[k] - hp[k]);
            v[k] = (lp[k] - hp[k]) * gk;
            fdnOut[k] = y;
        }
        /* Hadamard 8 */
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; j++) {
                    float a = v[j], b = v[j + h];
                    v[j] = a + b;
                    v[j + h] = a - b;
                }
        const float nrm = 0.35355339f;
        for (auto &x8 : v)                                 /* (a full, endless tail fed for ever: it rounds off) */
            x8 = 2.5f * softT(x8 * 0.4f);
        static const float INS[8] = {1, -1, 1, -1, 1, 1, -1, -1};
        for (int k = 0; k < 8; k++)
            fdn[k].push(v[k] * nrm + x * INS[k] * 0.5f + (k & 1 ? erR : erL) * 0.15f);
        float yl = 0.5f * (fdnOut[0] - fdnOut[2] + fdnOut[4] - fdnOut[6]) + 0.25f * (fdnOut[1] + fdnOut[7]);
        float yr = 0.5f * (fdnOut[1] - fdnOut[3] + fdnOut[5] - fdnOut[7]) + 0.25f * (fdnOut[2] + fdnOut[6]);
        yl += erL * 0.6f;
        yr += erR * 0.6f;
        rvTail = rvTail * 0.9995f + std::fabs(yl) + std::fabs(yr);
        /* the output's LoPass and HiPass */
        float olp = onePole(500.0f * fexp2(s.rvLp * 5.5f), sr), ohp = onePole(15.0f * fexp2(s.rvHp * 7.0f), sr);
        rvOutLp[0] += olp * (yl - rvOutLp[0]);
        rvOutLp[1] += olp * (yr - rvOutLp[1]);
        rvOutHp[0] += ohp * (rvOutLp[0] - rvOutHp[0]);
        rvOutHp[1] += ohp * (rvOutLp[1] - rvOutHp[1]);
        float g = s.rvLevel * 0.9f;
        ol = (rvOutLp[0] - rvOutHp[0]) * g;
        orr = (rvOutLp[1] - rvOutHp[1]) * g;
        for (auto &f : fdnOut)
            f = flushDn(f);
    }
};
} // namespace pk
