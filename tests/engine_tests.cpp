/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK's engine tests, without JUCE: every factory patch at 44.1, 48 and 96 kHz (no NaN, no runaway, sound where
 * there should be, silence after), the oscillators' aliasing, the filter's self-oscillation, every voice mode and
 * unison, the arp, every FX routing, the matrix's every destination at full depth, any buffer size the same, 8 and 16
 * voices, the CPU. A failure exits non-zero. */
#include "Engine.h"
#include "Presets.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace pk;
static int fails = 0;
#define CHECK(c, ...)                                                                                                   \
    do {                                                                                                                \
        if (!(c)) {                                                                                                     \
            std::printf("FAIL: " __VA_ARGS__);                                                                          \
            std::printf("\n");                                                                                          \
            fails++;                                                                                                    \
        }                                                                                                               \
    } while (0)

struct Stats {
    float peak = 0, rms = 0;
    bool nan = false;
};
static Stats run(Engine &e, int frames, int block = 256, std::vector<float> *keep = nullptr)
{
    std::vector<float> L(block), R(block);
    Stats s;
    double sum = 0;
    int done = 0;
    while (done < frames) {
        int n = std::min(block, frames - done);
        e.render(L.data(), R.data(), n);
        for (int i = 0; i < n; i++) {
            if (!std::isfinite(L[i]) || !std::isfinite(R[i]))
                s.nan = true;
            s.peak = std::max(s.peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
            sum += L[i] * L[i] + R[i] * R[i];
            if (keep)
                keep->push_back(L[i]);
        }
        done += n;
    }
    s.rms = (float)std::sqrt(sum / (2.0 * frames));
    return s;
}
static void chord(Engine &e, bool on)
{
    const int notes[4] = {48, 55, 60, 64};
    for (int n : notes)
        on ? e.noteOn(n, 0.8f) : e.noteOff(n);
}
/* the energy away from a note's harmonics (dB under the harmonics'), a DFT over the harmonics' bins */
static double nonHarmonicDb(const std::vector<float> &x, double sr, double f0)
{
    const int N = 8192;
    std::vector<double> w(N);
    for (int i = 0; i < N; i++)
        w[i] = x[x.size() - N + i] * (0.42 - 0.5 * std::cos(2 * M_PI * i / N) + 0.08 * std::cos(4 * M_PI * i / N));
    double harm = 0, non = 0;
    for (int k = 1; k < N / 2 && k * sr / N < 20000.0; k++) {
        double re = 0, im = 0;
        for (int i = 0; i < N; i++) {
            re += w[i] * std::cos(2 * M_PI * k * i / N);
            im += w[i] * std::sin(2 * M_PI * k * i / N);
        }
        double f = k * sr / N, p = re * re + im * im, h = std::fmod(f, f0);
        if (f < 30.0)
            continue;
        if (std::min(h, f0 - h) < 3.5 * sr / N)
            harm += p;
        else
            non += p;
    }
    return 10.0 * std::log10(non / harm + 1e-30);
}

int main()
{
    static Engine e;
    /* every factory patch, three rates */
    const double rates[3] = {44100.0, 48000.0, 96000.0};
    for (double sr : rates) {
        e.prepare(sr);
        for (int k = 0; k < NFACTORY; k++) {
            e.panic();
            e.setPatch(FACTORY[k]);
            chord(e, true);
            Stats on = run(e, (int)(sr * 1.5));
            chord(e, false);
            e.setParam(P_ARP_ON, 0);
            run(e, (int)(sr * 20.0));
            Stats tail = run(e, (int)(sr * 0.5));
            CHECK(!on.nan && !tail.nan, "%s at %.0f: NaN", FACTORY[k].name, sr);
            CHECK(on.peak < 2.0f, "%s at %.0f: peak %.2f", FACTORY[k].name, sr, on.peak);
            CHECK(on.rms > 0.003f, "%s at %.0f: silent (rms %.4f)", FACTORY[k].name, sr, on.rms);
            CHECK(tail.peak < 0.02f, "%s at %.0f: still sounding after 20 s (%.4f)", FACTORY[k].name, sr, tail.peak);
        }
    }
    std::printf("factory patches: done\n");

    /* the oscillators: saw, pulse, synced saw, at C6 (1 kHz), HIGH: their aliasing far down */
    e.prepare(44100.0);
    {
        struct T {
            const char *name;
            int wave, vsync;
            double limit;
        } tests[] = {{"saw", W_SAW, 0, -55.0}, {"pulse", W_SQUARE, 0, -55.0}, {"vsync saw", W_SAW, 40, -50.0}};
        for (auto &t : tests) {
            Patch p;
            patchInit(p);
            p.v[P_OSC1 + OSC_WAVE] = (int16_t)t.wave;
            p.v[P_OSC1 + OSC_VSYNC] = (int16_t)t.vsync;
            p.v[P_KEYTRK] = 0;
            e.panic();
            e.setPatch(p);
            e.setQuality(1);
            e.noteOn(84, 0.8f);
            std::vector<float> x;
            run(e, 22050, 256, &x);
            double db = nonHarmonicDb(x, 44100.0, 440.0 * std::pow(2.0, (84 - 69) / 12.0));
            std::printf("  %-10s aliasing %.1f dB\n", t.name, db);
            CHECK(db < t.limit, "%s: aliasing %.1f dB", t.name, db);
            e.noteOff(84);
        }
    }
    /* the filter sings by itself at full resonance, in tune */
    {
        Patch p;
        patchInit(p);
        p.v[P_LVL1] = 0;
        p.v[P_RESO] = 255;
        p.v[P_FREQ] = 150;
        e.panic();
        e.setPatch(p);
        e.noteOn(60, 0.8f);
        std::vector<float> x;
        Stats s = run(e, 44100, 256, &x);
        CHECK(s.rms > 0.02f && s.peak < 1.0f, "self-oscillation: rms %.3f peak %.3f", s.rms, s.peak);
        int zc = 0;
        for (size_t i = 22050; i + 1 < x.size(); i++)
            zc += (x[i] < 0.0f) != (x[i + 1] < 0.0f);
        double hz = zc / 2.0 / 0.5, want = 20.0 * std::pow(2.0, 150.0 * 10.2 / 255.0);
        std::printf("  self-oscillation at %.0f Hz (wanted %.0f)\n", hz, want);
        CHECK(std::fabs(std::log2(hz / want)) < 0.08, "self-oscillation at %.0f Hz, not %.0f", hz, want);
        e.noteOff(60);
    }
    /* every voice mode, unison sizes, 8 and 16 voices */
    for (int voices : {8, 16})
        for (int mode = 0; mode < NVMODE; mode++)
            for (int uni = 0; uni < NUNISON; uni++) {
                Patch p = FACTORY[0];
                p.v[P_MODE] = (int16_t)mode;
                p.v[P_UNISON] = (int16_t)uni;
                p.v[P_GLIDE_ON] = 1;
                e.panic();
                e.setVoices(voices);
                e.setPatch(p);
                for (int n = 40; n < 80; n += 3) {
                    e.noteOn(n, 0.7f);
                    run(e, 900);
                    if (n % 2)
                        e.noteOff(n - 3);
                }
                Stats s = run(e, 22050);
                e.allOff();
                CHECK(!s.nan && s.peak < 2.5f && s.rms > 0.001f, "mode %d unison %d, %d voices: peak %.2f rms %.4f", mode, uni, voices, s.peak, s.rms);
                CHECK(e.voicesSounding() <= voices, "mode %d: %d voices sounding", mode, e.voicesSounding());
            }
    e.setVoices(8);
    std::printf("voice modes: done\n");
    /* the arp, every type */
    for (int t = 0; t < NARPTYPE; t++) {
        Patch p = FACTORY[0];
        p.v[P_ARP_ON] = 1;
        p.v[P_ARP_TYPE] = (int16_t)t;
        p.v[P_ARP_OCT] = 3;
        p.v[P_ARP_RHYTHM] = (int16_t)(1 + t * 4);
        e.panic();
        e.setPatch(p);
        e.host(false, 0.0, 120.0, false);
        chord(e, true);
        Stats s = run(e, 88200);
        chord(e, false);
        CHECK(!s.nan && s.rms > 0.002f, "arp type %d: rms %.4f", t, s.rms);
    }
    /* every FX routing, every chorus and delay mode, at full levels */
    for (int r = 0; r < NFXROUTE; r++) {
        Patch p = FACTORY[0];
        p.v[P_FX_ROUTE] = (int16_t)r;
        p.v[P_CH_LEVEL] = p.v[P_DL_LEVEL] = p.v[P_RV_LEVEL] = p.v[P_DIST] = 127;
        p.v[P_DL_FB] = 127;
        p.v[P_RV_TIME] = 127;
        p.v[P_CH_MODE] = (int16_t)(r % NCHMODE);
        p.v[P_CH_TYPE] = (int16_t)(r % 3);
        p.v[P_DL_TMODE] = (int16_t)(r % NDTMODE);
        e.panic();
        e.setPatch(p);
        chord(e, true);
        Stats s = run(e, 88200);
        chord(e, false);
        Stats t = run(e, 88200);
        CHECK(!s.nan && !t.nan && s.peak < 4.0f && t.peak < 4.0f, "routing %d: peak %.2f / %.2f", r, s.peak, t.peak);
    }
    std::printf("effects: done\n");
    /* the matrix: every destination at full depth, both ways, from an LFO */
    for (int d = 0; d < NMODDST; d++)
        for (int depth : {63, -64}) {
            Patch p = FACTORY[0];
            p.v[P_MOD1 + MOD_SA] = S_LFO1B;
            p.v[P_MOD1 + MOD_DST] = (int16_t)d;
            p.v[P_MOD1 + MOD_DEPTH] = (int16_t)depth;
            p.v[P_MOD1 + MOD_N + MOD_DST] = (int16_t)d;
            p.v[P_MOD1 + MOD_N + MOD_DEPTH] = (int16_t)depth;
            p.v[P_LFO1 + LFO_RATE] = 200;
            e.panic();
            e.setPatch(p);
            chord(e, true);
            Stats s = run(e, 44100);
            chord(e, false);
            run(e, 44100);
            CHECK(!s.nan && s.peak < 4.0f, "destination %s depth %d: peak %.2f", MODDST_NAME[d], depth, s.peak);
        }
    std::printf("matrix: done\n");
    /* any buffer size: the same sound */
    {
        std::vector<float> a, b;
        static Engine e2[2];
        for (int pass = 0; pass < 2; pass++) {
            Engine &f = e2[pass];
            f.prepare(44100.0);
            f.setPatch(FACTORY[4]);
            f.noteOn(60, 0.8f);
            f.noteOn(67, 0.8f);
            run(f, 30000, pass ? 37 : 512, pass ? &b : &a);
        }
        double diff = 0;
        for (size_t i = 0; i < a.size(); i++)
            diff = std::max(diff, (double)std::fabs(a[i] - b[i]));
        CHECK(diff < 1e-5, "buffer sizes differ by %g", diff);
    }
    /* the CPU: 16 voices, HIGH, the heaviest patch */
    {
        e.prepare(48000.0);
        e.setVoices(16);
        e.setQuality(1);
        Patch p = FACTORY[0];
        e.panic();
        e.setPatch(p);
        for (int n = 40; n < 88; n += 3)
            e.noteOn(n, 0.8f);
        auto t0 = std::chrono::steady_clock::now();
        run(e, 48000 * 4);
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("CPU, 16 voices HIGH: %.1f %% of a core\n", t / 4.0 * 100.0);
    }
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
