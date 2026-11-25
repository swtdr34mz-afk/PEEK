/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK's building blocks: small helpers, the polyphase half-band of its oversampling (PROPHET 67's), and the
 * oscillator: a Peak-style "Oxford" oscillator, run at the voice's oversampled rate (2x or 4x) with its jumps smoothed
 * (a polynomial band-limited step a jump), so that a saw, a pulse, a synced or a folded wave stay clean while the
 * oscillators frequency-modulate each other sample by sample. */
#pragma once
#include "Wavetables.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace pk
{
constexpr float PI = 3.14159265358979f, TWOPI = 6.28318530717959f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
inline int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
/* 2^x, fast (a polynomial: 2e-7 off), for what runs per sample */
inline float fexp2(float x)
{
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    float fl = std::floor(x), f = x - fl;
    float p = 1.534580891e-04f;
    p = p * f + 1.339993158e-03f;
    p = p * f + 9.618488972e-03f;
    p = p * f + 5.550328776e-02f;
    p = p * f + 2.402264689e-01f;
    p = p * f + 6.931472057e-01f;
    p = p * f + 1.0f;
    return std::ldexp(p, (int)fl);
}
inline float hzOf(float midi) { return 440.0f * fexp2((midi - 69.0f) * (1.0f / 12.0f)); }
inline float flushDn(float x) { return std::fabs(x) < 1e-15f ? 0.0f : x; }
/* tanh, Padé (flat at +-3) */
inline float softT(float x)
{
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline uint32_t rndU(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
inline float rndF(uint32_t &s) { return (float)(int32_t)rndU(s) * (1.0f / 2147483648.0f); }   /* -1..1 */
/* sin(2 pi x) for any x, a parabola corrected (0.001 off): the oscillator's sine */
inline float sin2pi(float x)
{
    x -= std::floor(x + 0.5f);                            /* -0.5..0.5 */
    float y = 8.0f * x - 16.0f * x * std::fabs(x);
    return 0.225f * (y * std::fabs(y) - y) + y;
}
/* a triangle folder: x within +-1 as it is, beyond folded back (a Buchla-ish wavefolder's line) */
inline float foldTri(float x)
{
    float t = (x + 1.0f) * 0.25f;
    t -= std::floor(t);
    return 1.0f - 4.0f * std::fabs(t - 0.5f);
}

/* ----------------------------------------------------------- the half-band --- */
/* a polyphase IIR half-band (two paths of four first-order allpasses; de Soras' elliptic design: -91 dB from 24.7 kHz
 * at 88.2, flat to 19.4 kHz) */
static const float HB_A0[4] = {0.0472898531f, 0.3385179585f, 0.6552730688f, 0.8745126132f};
static const float HB_A1[4] = {0.1727806110f, 0.5069776792f, 0.7764899192f, 0.9589457184f};
inline float hbAp4(float x, float *t, const float *a)
{
    float y0 = a[0] * (x - t[1]) + t[0], y1 = a[1] * (y0 - t[2]) + t[1], y2 = a[2] * (y1 - t[3]) + t[2],
          y3 = a[3] * (y2 - t[4]) + t[3];
    t[0] = x;
    t[1] = y0;
    t[2] = y1;
    t[3] = y2;
    t[4] = y3;
    return y3;
}
/* a 2x stage down (two in, one out) through the half-band */
struct HalfBand {
    float d0[5] = {}, d1[5] = {}, dp = 0.0f;
    inline float down(float a, float b)
    {
        float y = 0.5f * (hbAp4(a, d0, HB_A0) + hbAp4(dp, d1, HB_A1));
        dp = b;
        return y;
    }
    void flush()
    {
        for (int i = 0; i < 5; i++) {
            d0[i] = flushDn(d0[i]);
            d1[i] = flushDn(d1[i]);
        }
        dp = flushDn(dp);
    }
    void clear() { *this = HalfBand(); }
};

/* ----------------------------------------------------------- the BLEP tables --- */
/* PROPHET 67's band-limited step: a Kaiser-windowed sinc (0.9 of Nyquist) over 16 samples, its step and its ramp's
 * residual, 1024 points a sample: -80 dB of aliasing, at the voice's oversampled rate */
constexpr int BL_H = 8, BL_OS = 1024, BL_N = 2 * BL_H * BL_OS + 2;
constexpr uint32_t ORING = 32u;
const float *blepTables();
/* A band-limited output line: corrections land in a ring around where they happen, the output is BL_H samples late */
struct BlRing {
    float ring[ORING];
    uint32_t rp;
    void clear() { std::memset(this, 0, sizeof *this); }
    /* a step of height J (ramp 0) or a change of slope J per sample (ramp 1), d samples before this one */
    inline void add(const float *tab, float J, float d, int ramp)
    {
        const float *tb = tab + (ramp ? BL_N : 0);
        float di = d * (float)BL_OS;
        int i0 = (int)di;
        if (i0 >= BL_OS)
            i0 = BL_OS - 1;
        if (i0 < 0)
            i0 = 0;
        float fr = di - (float)i0;
        for (int j = -BL_H; j < BL_H; j++) {
            const float *q = tb + (j + BL_H) * BL_OS + i0;
            float b = q[0] + fr * (q[1] - q[0]);
            if (!ramp && j >= 0)
                b -= 1.0f;
            ring[(rp + (uint32_t)j) & (ORING - 1u)] += J * b;
        }
    }
    /* this sample's naive value in, the corrected one (BL_H late) out */
    inline float out(float naive)
    {
        ring[rp & (ORING - 1u)] += naive;
        uint32_t r = (rp - (uint32_t)BL_H) & (ORING - 1u);
        float o = ring[r];
        ring[r] = 0.0f;
        rp++;
        return o;
    }
};

/* ---------------------------------------------------------------- the oscillator --- */
/* What an oscillator plays this chunk: its wave and SHAPE (-1..1), VSYNC (the virtual master's ratio, 1: none),
 * SAWDENSE's two copies (their level, their detune as a ratio) and the wavetable (its wave position 0..4, its mip) */
struct OscSet {
    int wave = 2;
    float shape = 0.0f, ratio = 1.0f, dense = 0.0f, ddet = 0.0f;
    int table = 0, mip = 0;
    float pos = 0.0f;
    const float *tab = nullptr;                            /* the BLEP tables */
};

struct Osc {
    float ph = 0.0f, ms = 0.0f;                            /* the phase, the virtual master's */
    float dp[2] = {0.0f, 0.0f};                            /* SAWDENSE's copies */
    BlRing r;                                              /* the output, band-limited (BL_H samples late) */
    void reset(float p)
    {
        ph = ms = p;
        dp[0] = p * 0.37f;
        dp[1] = p * 0.71f;
    }
    void clear()
    {
        ph = ms = 0.0f;
        dp[0] = dp[1] = 0.0f;
        r.clear();
    }

    /* the wave's value at a phase (0..1) */
    static inline float value(const OscSet &s, float p)
    {
        const float a = s.shape;
        switch (s.wave) {
        case 0: {                                           /* SINE: + folds it, - bends its phase (brighter) */
            if (a >= 0.0f)
                return a > 0.0f ? foldTri(sin2pi(p) * (1.0f + 3.5f * a)) : sin2pi(p);
            return sin2pi(p - 0.18f * a * sin2pi(p));
        }
        case 1: {                                           /* TRIANGLE: + folds it, - leans its peak toward a saw */
            if (a >= 0.0f) {
                float t = 1.0f - 4.0f * std::fabs(p - 0.5f);
                return a > 0.0f ? foldTri(t * (1.0f + 3.0f * a)) : t;
            }
            float pk = 0.5f - 0.48f * a;                   /* (a < 0: the peak moves right) */
            return p < pk ? -1.0f + 2.0f * p / pk : 1.0f - 2.0f * (p - pk) / (1.0f - pk);
        }
        case 2: {                                           /* SAWTOOTH (rising): + a second saw half a cycle on (an
                                                             * octave's brightness), - folded */
            float sw = 2.0f * p - 1.0f;
            if (a > 0.0f) {
                float q = p + 0.5f;
                q -= q >= 1.0f ? 1.0f : 0.0f;
                return (sw + a * (2.0f * q - 1.0f)) / (1.0f + 0.5f * a);
            }
            if (a < 0.0f)
                return foldTri(sw * (1.0f - 2.5f * a));
            return sw;
        }
        case 3: {                                           /* SQUARE / PULSE: the width */
            float pw = 0.5f + 0.47f * a;
            return p < pw ? 1.0f : -1.0f;
        }
        default: {                                          /* MORE: the wavetable, between its waves */
            float x = p * (float)WT_LEN;
            int i = (int)x;
            if (i >= WT_LEN)
                i = WT_LEN - 1;
            float f = x - (float)i;
            int w0 = (int)s.pos;
            if (w0 > 3)
                w0 = 3;
            float wf = s.pos - (float)w0;
            const float *A = wtWave(s.table, w0, s.mip), *B = wtWave(s.table, w0 + 1, s.mip);
            float va = A[i] + f * (A[i + 1] - A[i]), vb = B[i] + f * (B[i + 1] - B[i]);
            return va + wf * (vb - va);
        }
        }
    }
    /* a jump of J, d samples (0..1) before this sample, into the band-limited line; g: the level the output is
     * scaled by */
    float g = 1.0f;
    const float *tb = nullptr;
    inline void jump(float J, float d, float &)
    {
        if (J != 0.0f)
            r.add(tb, g * J, d, 0);
    }
    /* a corner (the slope changing by S a cycle), d samples before this sample */
    inline void corner(float S, float d, float inc) { r.add(tb, g * S * inc, d, 1); }
    /* the jumps inside a cycle passed from phase a to b (within one cycle), b reached 'ago' samples before now */
    inline void inner(const OscSet &s, float a, float b, float inc, float ago, float &cNow)
    {
        auto cross = [&](float c) {
            if (a < c && b >= c) {
                float d = clampf((b - c) / inc + ago, 0.0f, 0.9999f);
                jump(value(s, c + 1e-5f) - value(s, c - 1e-5f), d, cNow);
            }
        };
        if (s.wave == 3)
            cross(0.5f + 0.47f * s.shape);
        else if (s.wave == 2 && s.shape > 0.0f)
            cross(0.5f);
        else if (s.wave == 1 && s.shape <= 0.0f) {          /* the triangle's peak: a corner */
            float pk = 0.5f - 0.48f * s.shape;
            if (a < pk && b >= pk)
                corner(-2.0f / pk - 2.0f / (1.0f - pk), clampf((b - pk) / inc + ago, 0.0f, 0.9999f), inc);
        }
    }
    /* the cycle's own jump (its end to its start) */
    inline void wrap(const OscSet &s, float d, float &cNow, float inc)
    {
        jump(value(s, 1e-5f) - value(s, 0.99999f), d, cNow);
        if (s.wave == 1 && s.shape <= 0.0f) {              /* the triangle's foot */
            float pk = 0.5f - 0.48f * s.shape;
            corner(2.0f / pk + 2.0f / (1.0f - pk), d, inc);
        }
    }

    /* a step: inc (cycles a sample, FM's in it). The output is a sample late */
    inline float tick(const OscSet &s, float inc)
    {
        float cNow = 0.0f;
        inc = clampf(inc, 1e-7f, 0.45f);
        g = (s.wave == 2 && s.dense > 0.0f) ? 1.0f / (1.0f + 0.9f * s.dense) : 1.0f;
        tb = s.tab;
        float out;
        const float old = ph;
        if (s.ratio > 1.001f) {                             /* VSYNC: a virtual master at the note resets it */
            float sinc = minf(inc * s.ratio, 0.49f);
            ms += inc;
            float np = old + sinc;
            if (ms >= 1.0f) {
                ms -= std::floor(ms);
                float d = clampf(ms / inc, 0.0f, 0.9999f);   /* (the reset, d samples ago) */
                float pr = old + sinc * (1.0f - d);         /* where it got to */
                if (pr >= 1.0f) {
                    float over = pr - 1.0f, aw = clampf(d + over / sinc, 0.0f, 0.9999f);
                    inner(s, old, 1.0f, sinc, aw, cNow);
                    wrap(s, aw, cNow, sinc);
                    inner(s, -1.0f, over, sinc, d, cNow);
                    pr = over;
                } else
                    inner(s, old, pr, sinc, d, cNow);
                jump(value(s, 0.0f) - value(s, pr), d, cNow);
                np = sinc * d;
                inner(s, -1.0f, np, sinc, 0.0f, cNow);
            } else if (np >= 1.0f) {
                np -= 1.0f;
                float d = clampf(np / sinc, 0.0f, 0.9999f);
                inner(s, old, 1.0f, sinc, d, cNow);
                wrap(s, d, cNow, sinc);
                inner(s, -1.0f, np, sinc, 0.0f, cNow);
            } else
                inner(s, old, np, sinc, 0.0f, cNow);
            ph = np;
        } else {
            float np = old + inc;
            if (np >= 1.0f) {
                np -= 1.0f;
                float d = clampf(np / inc, 0.0f, 0.9999f);
                inner(s, old, 1.0f, inc, d, cNow);
                wrap(s, d, cNow, inc);
                inner(s, -1.0f, np, inc, 0.0f, cNow);
            } else
                inner(s, old, np, inc, 0.0f, cNow);
            ph = np;
            ms = ph;
        }
        out = value(s, ph);
        /* SAWDENSE: two more saws, a little detuned (the saw only) */
        if (g < 1.0f) {
            for (int k = 0; k < 2; k++) {
                float di = inc * (k ? 1.0f + s.ddet : 1.0f - s.ddet * 0.93f);
                float np = dp[k] + di;
                if (np >= 1.0f) {
                    np -= 1.0f;
                    jump(-2.0f * s.dense, clampf(np / di, 0.0f, 0.9999f), cNow);
                }
                dp[k] = np;
                out += s.dense * (2.0f * np - 1.0f);
            }
        }
        (void)cNow;
        return r.out(out * g);
    }
};
} // namespace pk
