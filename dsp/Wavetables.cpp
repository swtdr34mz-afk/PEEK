/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "Wavetables.h"
#include "Params.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace pk
{
namespace
{
constexpr double TAU = 6.283185307179586;
constexpr int NH = 128, TD = 2048;          /* the harmonics kept, the points a recipe is drawn at */

double frac(double x) { return x - std::floor(x); }
double fold(double x) { return 1.0 - 4.0 * std::fabs(frac((x + 1.0) * 0.25) - 0.5); }   /* a triangle folder */
double saw(double p) { return 2.0 * frac(p) - 1.0; }
double tri(double p) { return 1.0 - 4.0 * std::fabs(frac(p) - 0.5); }
double pulse(double p, double w) { return frac(p) < w ? 1.0 : -1.0; }
uint32_t hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
double rnd(uint32_t a, uint32_t b) { return (double)(hash(a * 7919u + b * 104729u + 17u) & 0xffffffu) / 16777216.0; }   /* 0..1 */
double gauss(double x, double c, double w) { return std::exp(-(x - c) * (x - c) / (2.0 * w * w)); }

/* a recipe given as harmonics (amplitudes; phases sine): amp(h) for h = 1..NH */
using HarmFn = double (*)(int t, double m, int h);
/* the recipes drawn in time: one cycle, x 0..1 */
double drawn(int t, double m, double x)
{
    switch (t) {
    case 17: {                                          /* Sync Up: a sine synced at 1..7 times */
        double r = 1.0 + 6.0 * m;
        return std::sin(TAU * frac(x * r)) * (1.0 - 0.15 * m);
    }
    case 18: return saw(x * (1.0 + 5.0 * m));           /* Sync Saw */
    case 19: return pulse(x, 0.5 - 0.44 * m);           /* PWM */
    case 20: return fold(saw(x) * (1.0 + 4.0 * m));     /* SawFold */
    case 21: return fold(std::sin(TAU * x) * (1.0 + 5.0 * m));
    case 22: return fold(tri(x + 0.25) * (1.0 + 4.0 * m));
    case 32: return 0.6 * pulse(x, 0.12 + 0.1 * (1.0 - m)) + 0.4 * saw(2.0 * x) * m;   /* Clavy */
    case 39: {                                          /* Grit: a saw in fewer steps */
        double n = std::round(16.0 - 13.0 * m);
        return std::floor(frac(x) * n) / (n - 1.0) * 2.0 - 1.0;
    }
    case 40: {                                          /* Bits: a sine in fewer bits */
        double n = std::pow(2.0, 5.0 - 4.0 * m);
        return std::round(std::sin(TAU * x) * n) / n;
    }
    case 41: {                                          /* Chip: the old consoles' pulses and stepped triangle */
        if (m < 0.75) {
            const double w[4] = {0.5, 0.25, 0.125, 0.0625};
            int k = (int)(m * 4.0 / 0.75 + 0.5);
            return pulse(x, w[k > 3 ? 3 : k]);
        }
        return std::round(tri(x) * 7.5) / 7.5;
    }
    case 42: {                                          /* Hash: a seeded wave of steps, smoothed a little by m */
        int n = 8 + (int)(m * 24.0);
        double xi = frac(x) * n;
        int i0 = (int)xi;
        double f = xi - i0, a = rnd(42, (uint32_t)i0) * 2 - 1, b = rnd(42, (uint32_t)((i0 + 1) % n)) * 2 - 1;
        double s = f * f * (3 - 2 * f);
        return a + (b - a) * s;
    }
    case 47: {                                          /* Ladder: a staircase of 2..9 steps */
        double n = 2.0 + std::round(7.0 * m);
        return std::floor(frac(x) * n) / (n - 1.0) * 2.0 - 1.0;
    }
    case 48: return std::round(std::sin(TAU * x) * (2.0 + 6.0 * (1.0 - m))) / (2.0 + 6.0 * (1.0 - m));   /* Stairs */
    case 50: return std::sin(TAU * x + (0.5 + 2.5 * m) * std::sin(TAU * 2.0 * x));   /* Wobbly */
    case 51: {                                          /* PhaseDst: a phase-distorted cosine (the CZ's saw) */
        double d = 0.5 - 0.47 * m, p = frac(x);
        double q = p < d ? 0.5 * p / d : 0.5 + 0.5 * (p - d) / (1.0 - d);
        return -std::cos(TAU * q);
    }
    case 52: {                                          /* Casio: a windowed resonance (the CZ's) */
        double p = frac(x), r = 1.0 + 9.0 * m;
        return (1.0 - p) * std::cos(TAU * r * p) * 1.2 - 0.2;
    }
    case 53: {                                          /* Squelch: a saw ringing at a moving resonance */
        double p = frac(x), r = 2.0 + 10.0 * m;
        return 0.5 * saw(x) + 0.7 * (1.0 - p) * std::sin(TAU * r * p);
    }
    default: return 0.0;
    }
}
bool isDrawn(int t)
{
    switch (t) {
    case 17: case 18: case 19: case 20: case 21: case 22: case 32: case 39: case 40: case 41: case 42: case 47: case 48:
    case 50: case 51: case 52: case 53: return true;
    default: return false;
    }
}
/* the others, as harmonics: h 1..NH at morph m (0..1) */
double harm(int t, double m, int h)
{
    const double H = h;
    switch (t) {
    case 0: return h == 1 ? 1.0 : h <= 4 ? 0.25 * m / H : 0.0;                         /* Pure */
    case 1: return 1.0 / std::pow(H, 2.2 - 1.4 * m);                                     /* Bright */
    case 2: return (h & 1) ? 1.0 / std::pow(H, 2.4 - 1.5 * m) : 0.0;                     /* Hollow */
    case 3: return std::fabs(std::sin(3.14159 * H * (0.5 - 0.38 * m))) / std::pow(H, 0.85);   /* Reedy */
    case 4: return h == 1 ? 1.0 : (h == 7 || h == 9 || h == 13 || h == 17) ? 0.12 + 0.5 * m : 0.0;   /* Glassy */
    case 5: {                                                                          /* Bellish */
        const int P[6] = {1, 3, 7, 11, 16, 24};
        for (int k = 0; k < 6; k++)
            if (h == P[k])
                return k == 0 ? 1.0 : 0.6 * std::exp(-std::fabs((double)k / 5.0 - m) * 3.0);
        return 0.0;
    }
    case 6: case 7: case 8: {                                                          /* the organs */
        const int D[9] = {1, 2, 3, 4, 6, 8, 10, 12, 16};
        static const double A[3][2][9] = {
            {{1, .8, .6, .2, 0, 0, 0, 0, 0}, {1, .8, .8, .6, .4, .3, .2, .2, .3}},
            {{1, 0, .7, 0, 0, 0, 0, 0, 0}, {1, .2, .8, .1, .4, .1, .3, 0, .5}},
            {{1, 0, 0, 0, 0, 0, 0, 0, 0}, {.8, .8, .8, .8, .8, .8, .8, .8, .8}}};
        for (int k = 0; k < 9; k++)
            if (h == D[k]) {
                double a = A[t - 6][0][k], b = A[t - 6][1][k];
                if (t == 8)
                    return k == 0 ? 1.0 : (m * 8.0 >= k ? 0.8 : 0.0);
                return a + (b - a) * m;
            }
        return 0.0;
    }
    case 9: case 10: case 11: case 12: case 13: case 16: case 30: case 54: {          /* formants */
        static const double F[5][3] = {{7, 12, 25}, {4, 8, 24}, {3, 22, 30}, {5, 9, 26}, {6, 11, 28}};
        double f1, f2, f3, w = 1.4;
        if (t == 16 || t == 54) {
            f1 = (t == 16 ? 2.0 + 30.0 * m : 12.0 + 20.0 * m);
            f2 = f1 * 1.6;
            f3 = f1 * 2.6;
            w = 1.8;
        } else {
            int v = t == 9 ? 0 : t == 10 ? 1 : t == 11 ? 2 : t == 12 ? 3 : 4;
            int v2 = (v + 1) % 5;
            f1 = F[v][0] + (F[v2][0] - F[v][0]) * m;
            f2 = F[v][1] + (F[v2][1] - F[v][1]) * m;
            f3 = F[v][2] + (F[v2][2] - F[v][2]) * m;
            if (t == 12)
                w = 2.2;
            if (t == 30) {
                f1 = 3.0 + 4.0 * m;
                f2 = 9.0 + 6.0 * m;
                f3 = 20.0;
                w = 3.0;
            }
        }
        double env = gauss(H, f1, w) + 0.7 * gauss(H, f2, w * 1.4) + 0.35 * gauss(H, f3, w * 2.0);
        double base = 1.0 / std::pow(H, t == 13 ? 0.4 : 0.9);
        return base * (0.08 + env) * (t == 13 ? (0.6 + 0.4 * rnd(13, (uint32_t)h)) : 1.0);
    }
    case 14: {                                                                         /* Metal */
        bool prime = h > 1;
        for (int k = 2; k * k <= h; k++)
            if (h % k == 0)
                prime = false;
        return h == 1 ? 1.0 : prime ? (0.25 + 0.5 * m) / std::pow(H, 0.35) : 0.03 / H;
    }
    case 15: return (1.0 / H) * (1.0 + 4.0 * m * (0.5 + 0.5 * std::cos(H * 0.9)));     /* Steel */
    case 23: return h == 1 ? 1.0 - 0.6 * m : (h == 1 + (int)std::round(15.0 * m) ? 0.9 : 0.0);   /* HarmUp */
    case 24: return h >= 1 + (int)(m * 12.0) ? 1.0 / H : 0.0;                           /* HarmDown */
    case 25: return (h & 1) ? 1.0 / std::pow(H, 1.0 - 0.4 * m) * (h % 3 == 0 ? m : 1.0) : 0.0;   /* OddOnly */
    case 26: return h == 1 ? 1.0 : (h & 1) ? 0.0 : (0.3 + 0.6 * m) / std::pow(H, 0.8);   /* EvenOnly */
    case 27: return (1.0 / H) * (0.5 + 0.5 * std::cos(TAU * H / (3.0 + 9.0 * m)));     /* Comb */
    case 28: return (1.0 / H) * (1.0 - 0.97 * gauss(H, 2.0 + 30.0 * m, 1.5));          /* Notch */
    case 29: return 1.0 / std::pow(H, 1.5 - 0.4 * m);                                    /* Pad */
    case 31: return (1.0 / std::pow(H, 1.6 - 1.1 * m)) * (1.0 + gauss(H, 4.0, 3.0));    /* Brass */
    case 33: return h == 1 ? 1.0 : h == 2 ? 0.4 : h == 7 ? 0.5 * (1.0 - m) : h == 3 ? 0.2 * m : 0.0;   /* E.Keys */
    case 34: return (h & 1 ? 1.0 : 0.4) / (H * H) + (h >= 5 && h <= 12 ? 0.05 * m : 0.0);   /* Harp */
    case 35: return h == 1 ? 1.0 : h == 4 ? 0.5 : h == 10 ? 0.35 * (1.0 - 0.5 * m) : h == 20 ? 0.25 * m : 0.0;   /* Mallet */
    case 36: return h == 1 ? 1.5 : 1.0 / std::pow(H, 1.3 - 0.5 * m);                     /* Bass 1 */
    case 37: return (h & 1) ? 1.0 / std::pow(H, 1.4 - 0.5 * m) : 0.25 * m / H;           /* Bass 2 */
    case 38: return 1.0 / std::pow(H, 0.6 - 0.3 * m);                                    /* Buzzy */
    case 43: return h == 1 ? 1.0 : (h == 3 || h == 5) ? 0.3 * m + 0.05 : h == 2 ? 0.2 : 0.0;   /* Hum */
    case 44: return (1.0 / std::pow(H, 1.1)) * (0.5 + rnd(44 + (uint32_t)(m * 4.0), (uint32_t)h));   /* Drone */
    case 45: return h > 48 ? 0.0 : rnd(45 + (uint32_t)std::lround(m * 4.0), (uint32_t)h) / std::pow(H, 0.7);   /* Spectra */
    case 46: return ((h - 1) / 3) % 2 == (int)(m * 1.99) ? 1.0 / std::pow(H, 0.8) : 0.08 / H;   /* Prism */
    case 49: return h == 1 ? 1.0 : (h >= 8 + (int)(m * 16) && h <= 10 + (int)(m * 16)) ? 0.4 : 0.0;   /* Ripple */
    case 55: return (h & 1) ? 1.0 / H : (1.0 - m) / H;                                  /* Twang */
    case 56: return (h == 1 || h == 2 || h == 4 || h == 8 || h == 16 || h == 32) ? (h == 1 ? 1.0 : 0.25 + 0.6 * m) / std::sqrt(H) : 0.0;   /* Shimmer */
    case 57: return (1.0 / H) * std::exp(-H * (0.25 - 0.2 * m)) + 0.02 * rnd(57, (uint32_t)h);   /* Dusty */
    case 58: return h == 1 ? 1.0 : h == 2 ? 0.5 * m + 0.1 : h == 3 ? 0.6 : h == 4 ? 0.2 * m : 0.0;   /* Moon */
    case 59: return (1.0 / H) * (0.1 + gauss(H, 3 + 6 * m, 1) + gauss(H, 12 + 14 * m, 2) + gauss(H, 30 + 20 * m, 3));   /* Peaks */
    default: return h == 1 ? 1.0 : 0.0;
    }
}

struct Store {
    std::vector<float> d;                    /* [table][wave][mip][WT_LEN + 1] */
    Store() : d((size_t)NWAVETABLE * WT_WAVES * WT_MIPS * (WT_LEN + 1))
    {
        std::vector<double> sinT(WT_LEN), cosT(WT_LEN), am(NH + 1), ph(NH + 1), x(TD), cs(TD), sn(TD);
        for (int i = 0; i < WT_LEN; i++) {
            sinT[(size_t)i] = std::sin(TAU * i / WT_LEN);
            cosT[(size_t)i] = std::cos(TAU * i / WT_LEN);
        }
        for (int i = 0; i < TD; i++) {
            cs[(size_t)i] = std::cos(TAU * i / TD);
            sn[(size_t)i] = std::sin(TAU * i / TD);
        }
        std::vector<double> cph(NH + 1), sph(NH + 1);
        for (int t = 0; t < NWAVETABLE; t++)
            for (int w = 0; w < WT_WAVES; w++) {
                double m = w / 4.0;
                if (isDrawn(t)) {                     /* drawn: its harmonics found (a DFT of 2048 points) */
                    for (int i = 0; i < TD; i++)
                        x[(size_t)i] = drawn(t, m, (double)i / TD);
                    for (int h = 1; h <= NH; h++) {
                        double re = 0, im = 0;
                        for (int i = 0; i < TD; i++) {
                            size_t k = (size_t)((h * i) & (TD - 1));
                            re += x[(size_t)i] * cs[k];
                            im += x[(size_t)i] * sn[k];
                        }
                        am[(size_t)h] = 2.0 * std::sqrt(re * re + im * im) / TD;
                        ph[(size_t)h] = std::atan2(re, im);
                    }
                } else {
                    for (int h = 1; h <= NH; h++) {
                        am[(size_t)h] = harm(t, m, h);
                        ph[(size_t)h] = 0.0;
                    }
                }
                /* each mip: the harmonics up to its limit, summed; all of a wave's mips by the full one's peak */
                for (int h = 1; h <= NH; h++) {
                    cph[(size_t)h] = std::cos(ph[(size_t)h]);
                    sph[(size_t)h] = std::sin(ph[(size_t)h]);
                }
                double peak = 0.0;
                std::vector<double> full(WT_LEN);
                for (int mip = 0; mip < WT_MIPS; mip++) {
                    int lim = NH >> mip;
                    float *o = &d[(((size_t)t * WT_WAVES + (size_t)w) * WT_MIPS + (size_t)mip) * (WT_LEN + 1)];
                    for (int i = 0; i < WT_LEN; i++) {
                        double s = 0;
                        for (int h = 1; h <= lim; h++) {
                            if (am[(size_t)h] == 0.0)
                                continue;
                            /* (a gentle roll-off over the top octave of each mip: no sudden change between them) */
                            double g = h > lim / 2 ? 0.5 + 0.5 * std::cos(3.14159265 * (h - lim / 2) / (lim / 2 + 1)) : 1.0;
                            size_t k = (size_t)((h * i) & (WT_LEN - 1));
                            s += g * am[(size_t)h] * (sinT[k] * cph[(size_t)h] + cosT[k] * sph[(size_t)h]);
                        }
                        if (mip == 0) {
                            full[(size_t)i] = s;
                            peak = std::fmax(peak, std::fabs(s));
                        }
                        o[i] = (float)s;
                    }
                }
                double k = peak > 1e-9 ? 0.95 / peak : 1.0;
                for (int mip = 0; mip < WT_MIPS; mip++) {
                    float *o = &d[(((size_t)t * WT_WAVES + (size_t)w) * WT_MIPS + (size_t)mip) * (WT_LEN + 1)];
                    for (int i = 0; i < WT_LEN; i++)
                        o[i] = (float)(o[i] * k);
                    o[WT_LEN] = o[0];
                }
            }
    }
};
const Store &store()
{
    static const Store s;
    return s;
}
} // namespace

void wtInit() { (void)store(); }

const float *wtWave(int table, int wave, int mip)
{
    if (table < 0 || table >= NWAVETABLE)
        table = 0;
    if (wave < 0)
        wave = 0;
    if (wave >= WT_WAVES)
        wave = WT_WAVES - 1;
    if (mip < 0)
        mip = 0;
    if (mip >= WT_MIPS)
        mip = WT_MIPS - 1;
    return &store().d[(((size_t)table * WT_WAVES + (size_t)wave) * WT_MIPS + (size_t)mip) * (WT_LEN + 1)];
}
} // namespace pk
