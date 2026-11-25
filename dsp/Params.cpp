/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "Params.h"
#include <cstdio>
#include <cstring>

namespace pk
{
const char *const WAVE_NAME[NWAVE] = {"Sine", "Triangle", "Sawtooth", "Square", "More"};
const char *const LFOTYPE_NAME[NLFOTYPE] = {"Triangle", "Sawtooth", "Square", "S+H"};
static const char *const ONOFF[] = {"Off", "On"};
static const char *const RANGE[] = {"16'", "8'", "4'", "2'"};
static const char *const SHSRC[] = {"Manual", "Mod Env 1", "LFO 1"};
static const char *const FSHAPE[] = {"LP", "BP", "HP"};
static const char *const SLOPE[] = {"12 dB", "24 dB"};
static const char *const FENVSRC[] = {"Amp Env", "Mod Env 1"};
static const char *const VMODE[] = {"Mono", "MonoLG", "Mono2", "Poly", "Poly2"};
static const char *const UNISON[] = {"1", "2", "3", "4", "8"};
const int UNISON_VOICES[NUNISON] = {1, 2, 3, 4, 8};
static const char *const SPREAD[] = {"Diverge", "Alternate", "Diverge 2", "NoteVal"};
static const char *const TRIG[] = {"Legato", "Re-Trig"};
static const char *const MENVSEL[] = {"Mod Env 1", "Mod Env 2"};
static const char *const LRANGE[] = {"Low", "High", "Sync"};
static const char *const FMODE[] = {"FadeIn", "FadeOut", "GateIn", "GateOut"};
static const char *const ARPTYPE[] = {"Up", "Down", "Up-Down 1", "Up-Down 2", "Played", "Random", "Chord", "Chord 2"};
static const char *const CHTYPE[] = {"1", "2", "3"};
static const char *const CHMODE[] = {"Chorus", "Flanger", "Phlanger"};
static const char *const DTMODE[] = {"Normal", "Double", "Treble", "QuadLoFi", "HexVLoFi"};
static const char *const ROUTE[] = {"Parallel", "D->R->C", "D->C->R", "R->D->C", "R->C->D", "C->D->R", "C->R->D"};

const char *const LFOSYNC_NAME[NLFOSYNC] = {
    "64 beats", "48 beats", "42 beats", "36 beats", "32 beats", "30 beats", "28 beats", "24 beats", "21 + 1/3",
    "20 beats", "18 + 2/3", "18 beats", "16 beats", "13 + 1/3", "12 beats", "10 + 2/3", "8 beats",  "6 beats",
    "5 + 1/3",  "4 beats",  "3 beats",  "2 + 2/3",  "2nd",      "4th D",    "1 + 1/3",  "4th",      "8th D",
    "4th T",    "8th",      "16th D",   "8th T",    "16th",     "16th T",   "32nd",     "32nd T"};
const float LFOSYNC_BEATS[NLFOSYNC] = {
    64, 48, 42, 36, 32, 30, 28, 24, 64.0f / 3, 20, 56.0f / 3, 18, 16, 40.0f / 3, 12, 32.0f / 3, 8, 6, 16.0f / 3, 4, 3,
    8.0f / 3, 2, 1.5f, 4.0f / 3, 1, 0.75f, 2.0f / 3, 0.5f, 0.375f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f, 1.0f / 12};
static const char *const L34SYNC[NLFOSYNC + 1] = {
    "Off",      "64 beats", "48 beats", "42 beats", "36 beats", "32 beats", "30 beats", "28 beats", "24 beats",
    "21 + 1/3", "20 beats", "18 + 2/3", "18 beats", "16 beats", "13 + 1/3", "12 beats", "10 + 2/3", "8 beats",
    "6 beats",  "5 + 1/3",  "4 beats",  "3 beats",  "2 + 2/3",  "2nd",      "4th D",    "1 + 1/3",  "4th",
    "8th D",    "4th T",    "8th",      "16th D",   "8th T",    "16th",     "16th T",   "32nd",     "32nd T"};
const char *const DLYRATIO_NAME[NDLYRATIO] = {"1/1", "4/3", "3/4", "3/2", "2/3", "2/1", "1/2", "3/1", "1/3", "4/1", "1/4"};
const float DLYRATIO_L[NDLYRATIO] = {1, 1, 0.75f, 1, 2.0f / 3, 1, 0.5f, 1, 1.0f / 3, 1, 0.25f};
const float DLYRATIO_R[NDLYRATIO] = {1, 0.75f, 1, 2.0f / 3, 1, 0.5f, 1, 1.0f / 3, 1, 0.25f, 1};

/* PEEK's own 60 wavetables (made in dsp/Wavetables.cpp; not the Peak's), five waves each */
const char *const WAVETABLE_NAME[NWAVETABLE] = {
    "Pure",     "Bright",   "Hollow",   "Reedy",    "Glassy",   "Bellish",  "Organ 1",  "Organ 2",  "Drawbar",
    "Vowel A",  "Vowel O",  "Vowel I",  "Choir",    "Breathy",  "Metal",    "Steel",    "Formant",  "Sync Up",
    "Sync Saw", "PWM",      "SawFold",  "SineFold", "TriFold",  "HarmUp",   "HarmDown", "OddOnly",  "EvenOnly",
    "Comb",     "Notch",    "Pad",      "Strings",  "Brass",    "Clavy",    "E.Keys",   "Harp",     "Mallet",
    "Bass 1",   "Bass 2",   "Buzzy",    "Grit",     "Bits",     "Chip",     "Hash",     "Hum",      "Drone",
    "Spectra",  "Prism",    "Ladder",   "Stairs",   "Ripple",   "Wobbly",   "PhaseDst", "Casio",    "Squelch",
    "Nasal",    "Twang",    "Shimmer",  "Dusty",    "Moon",     "Peaks"};

const char *const MODSRC_NAME[NMODSRC] = {
    "Direct",  "ModWheel", "AftTouch", "ExprPED1", "BrthPED2", "Velocity", "Keyboard", "LFO1+",   "LFO1+/-",
    "LFO2+",   "LFO2+/-",  "AmpEnv",   "ModEnv1",  "ModEnv2",  "Animate1", "Animate2", "CV +/-",  "LFO3+",
    "LFO3+/-", "LFO4+",    "LFO4+/-",  "BndWhl+",  "BndWhl-",  "Noise"};
const char *const MODDST_NAME[NMODDST] = {
    "O123Ptch", "Osc1Ptch", "Osc2Ptch", "Osc3Ptch", "Osc1VSnc", "Osc2VSnc", "Osc3VSnc", "Osc1Shpe", "Osc2Shpe",
    "Osc3Shpe", "Osc1 Lev", "Osc2 Lev", "Osc3 Lev", "NoiseLev", "Ring Lev", "VcaLevel", "Filt Drv", "FiltDist",
    "FiltFreq", "Filt Res", "Lfo1Rate", "Lfo2Rate", "AmpEnv A", "AmpEnv D", "AmpEnv R", "ModEnv1A", "ModEnv1D",
    "ModEnv1R", "ModEnv2A", "ModEnv2D", "ModEnv2R", "FM O1>O2", "FM O2>O3", "FM O3>O1", "FM Ns>O1", "O3>FiltF",
    "Ns>FiltF", "Pan Posn", "O123Shpe", "AEnv Del", "AEnv Hld", "AEnv Sus", "MEnv1Del", "MEnv1Hld", "MEnv1Sus",
    "MEnv2Del", "MEnv2Hld", "MEnv2Sus"};
const char *const FXDST_NAME[NFXDST] = {
    "Dist Lev", "Chor Lev", "ChorRate", "Chor Dep", "Chor FB",  "Del Lev",  "Del Time", "Del FB",
    "Rev Lev",  "Rev Time", "RevLdamp", "RevHdamp", "Rev Size", "RevLpass", "RevHpass", "Del Slew",
    "DelLdamp", "DelHdamp", "DelWidth", "DelRatio", "Lfo3Rate", "Lfo4Rate", "NoiseLPF", "NoiseHPF"};

#define N(id, name, lo, hi, def) {id, name, lo, hi, def, K_NUM, nullptr}
#define S(id, name, lo, hi, def) {id, name, lo, hi, def, K_SIGNED, nullptr}
#define C(id, name, n, def, list) {id, name, 0, (n) - 1, def, K_CHOICE, list}
#define B(id, name, def) {id, name, 0, 1, def, K_ONOFF, ONOFF}
#define OSC(n)                                                                                                         \
    C("o" #n "_range", "Osc " #n " Range", 4, 1, RANGE), S("o" #n "_coarse", "Osc " #n " Coarse", -12, 12, 0),         \
        S("o" #n "_fine", "Osc " #n " Fine", -100, 100, 0), C("o" #n "_wave", "Osc " #n " Wave", NWAVE, W_SAW, WAVE_NAME), \
        C("o" #n "_more", "Osc " #n " WaveMore", NWAVETABLE, 0, WAVETABLE_NAME),                                      \
        S("o" #n "_env2", "Osc " #n " Mod Env 2 Depth", -128, 127, 0),                                                 \
        S("o" #n "_lfo2", "Osc " #n " LFO 2 Depth", -128, 127, 0),                                                     \
        C("o" #n "_shsrc", "Osc " #n " Shape Source", NSHSRC, SS_MANUAL, SHSRC),                                       \
        S("o" #n "_shape", "Osc " #n " Shape Amount", -64, 63, 0),                                                     \
        S("o" #n "_shenv", "Osc " #n " Shape Mod Env 1", -64, 63, 0),                                                  \
        S("o" #n "_shlfo", "Osc " #n " Shape LFO 1", -64, 63, 0), N("o" #n "_vsync", "Osc " #n " VSync", 0, 127, 0),   \
        N("o" #n "_dense", "Osc " #n " SawDense", 0, 127, 0), N("o" #n "_ddet", "Osc " #n " DenseDet", 0, 127, 64),   \
        {"o" #n "_fixnote", "Osc " #n " FixedNote", 0, 88, 0, K_NOTE, nullptr},                                        \
        S("o" #n "_bend", "Osc " #n " BendRange", -24, 24, 12)
#define ENV(p, name, a, d, s, r, trig)                                                                                 \
    N(p "_att", name " Attack", 0, 127, a), N(p "_dec", name " Decay", 0, 127, d),                                     \
        N(p "_sus", name " Sustain", 0, 127, s), N(p "_rel", name " Release", 0, 127, r),                              \
        N(p "_dly", name " Delay", 0, 127, 0), N(p "_hold", name " Hold", 0, 127, 0),                                  \
        S(p "_vel", name " Velocity", -64, 63, 0), C(p "_trig", name " MonoTrig", 2, trig, TRIG),                      \
        {p "_rep", name " Repeats", 0, 127, 0, K_REPEAT, nullptr}
#define LFO(n, rate)                                                                                                   \
    C("lfo" #n "_type", "LFO " #n " Type", NLFOTYPE, LT_TRI, LFOTYPE_NAME),                                            \
        C("lfo" #n "_range", "LFO " #n " Range", NLFORANGE, LR_LOW, LRANGE),                                           \
        N("lfo" #n "_rate", "LFO " #n " Rate", 0, 255, rate),                                                          \
        C("lfo" #n "_sync", "LFO " #n " RateSync", NLFOSYNC, 25, LFOSYNC_NAME),                                        \
        N("lfo" #n "_fade", "LFO " #n " Fade Time", 0, 127, 0),                                                        \
        C("lfo" #n "_fmode", "LFO " #n " FadeMode", NFADEMODE, FM_FADEIN, FMODE),                                      \
        B("lfo" #n "_fsync", "LFO " #n " FadeSync", 1), {"lfo" #n "_phase", "LFO " #n " Phase", 0, 120, 0, K_PHASE, nullptr}, \
        C("lfo" #n "_trig", "LFO " #n " MonoTrig", 2, 0, TRIG), N("lfo" #n "_slew", "LFO " #n " Slew", 0, 127, 0),     \
        {"lfo" #n "_rep", "LFO " #n " Repeats", 0, 127, 0, K_LREPEAT, nullptr},                                        \
        B("lfo" #n "_common", "LFO " #n " Common", 0)
#define L34(n)                                                                                                         \
    C("lfo" #n "_type", "LFO " #n " Waveform", NLFOTYPE, LT_TRI, LFOTYPE_NAME),                                        \
        N("lfo" #n "_rate", "LFO " #n " Rate", 0, 127, 64), C("lfo" #n "_sync", "LFO " #n " RateSync", NLFOSYNC + 1, 17, L34SYNC), \
        {"lfo" #n "_phase", "LFO " #n " Phase", 0, 120, 0, K_PHASE, nullptr},                                          \
        N("lfo" #n "_slew", "LFO " #n " Slew", 0, 127, 0), N("lfo" #n "_fade", "LFO " #n " Fade Time", 0, 127, 0)
#define MOD(n)                                                                                                         \
    C("mod" #n "_sa", "Mod " #n " Source A", NMODSRC, S_DIRECT, MODSRC_NAME),                                          \
        C("mod" #n "_sb", "Mod " #n " Source B", NMODSRC, S_DIRECT, MODSRC_NAME),                                      \
        C("mod" #n "_dst", "Mod " #n " Destination", NMODDST, D_O123PITCH, MODDST_NAME),                               \
        S("mod" #n "_depth", "Mod " #n " Depth", -64, 63, 0)
#define FXMOD(n)                                                                                                       \
    C("fxmod" #n "_sa", "FX Mod " #n " Source A", NMODSRC, S_DIRECT, MODSRC_NAME),                                     \
        C("fxmod" #n "_sb", "FX Mod " #n " Source B", NMODSRC, S_DIRECT, MODSRC_NAME),                                 \
        C("fxmod" #n "_dst", "FX Mod " #n " Destination", NFXDST, FD_DIST, FXDST_NAME),                                \
        S("fxmod" #n "_depth", "FX Mod " #n " Depth", -64, 63, 0)

const ParamDef PARAM[NPARAM] = {
    OSC(1), OSC(2), OSC(3),
    N("diverge", "Diverge", 0, 127, 0),
    N("drift", "Drift", 0, 127, 0),
    N("noise_lpf", "Noise LPF", 0, 127, 127),
    N("noise_hpf", "Noise HPF", 0, 127, 0),
    B("keysync", "Osc KeySync", 0),
    N("lvl1", "Osc 1 Level", 0, 255, 255),
    N("lvl2", "Osc 2 Level", 0, 255, 0),
    N("lvl3", "Osc 3 Level", 0, 255, 0),
    N("lvl_noise", "Noise Level", 0, 255, 0),
    N("lvl_ring", "Ring 1*2 Level", 0, 255, 0),
    N("vca_gain", "VCA Gain", 0, 127, 127),
    N("freq", "Filter Frequency", 0, 255, 255),
    N("reso", "Filter Resonance", 0, 255, 0),
    N("drive", "Filter Overdrive", 0, 255, 0),
    C("fshape", "Filter Shape", NFSHAPE, FS_LP, FSHAPE),
    C("slope", "Filter Slope", 2, 1, SLOPE),
    C("fenv_src", "Filter Env Source", 2, 1, FENVSRC),
    S("fenv1", "Filter Mod Env 1 Depth", -128, 127, 0),
    S("fenva", "Filter Amp Env Depth", -128, 127, 0),
    S("flfo1", "Filter LFO 1 Depth", -128, 127, 0),
    N("fosc3", "Osc 3 Filter Mod", 0, 255, 0),
    N("keytrk", "Filter Key Tracking", 0, 127, 127),
    N("postdrv", "Filter Post Drive", 0, 127, 0),
    N("fdiverge", "Filter Diverge", 0, 127, 0),
    B("glide_on", "Glide On", 0),
    N("glide", "Glide Time", 0, 127, 60),
    S("preglide", "PreGlide", -12, 12, 0),
    C("mode", "Voice Mode", NVMODE, VM_POLY, VMODE),
    C("unison", "Unison", NUNISON, 0, UNISON),
    N("unidet", "Unison Detune", 0, 127, 25),
    N("unispread", "Unison Spread", 0, 127, 0),
    C("spreadmode", "Spread Mode", NSPREAD, SP_DIVERGE, SPREAD),
    S("pan", "Pan Position", -64, 63, 0),
    N("patchlvl", "Patch Level", 0, 127, 64),
    ENV("aenv", "Amp Env", 2, 90, 127, 40, 0),
    ENV("m1env", "Mod Env 1", 2, 75, 35, 45, 1),
    ENV("m2env", "Mod Env 2", 2, 75, 35, 45, 1),
    C("menv_sel", "Mod Env Select", 2, 0, MENVSEL),
    N("an1_att", "Animate 1 Attack", 0, 127, 0),
    N("an1_rel", "Animate 1 Release", 0, 127, 0),
    N("an2_att", "Animate 2 Attack", 0, 127, 0),
    N("an2_rel", "Animate 2 Release", 0, 127, 0),
    LFO(1, 127), LFO(2, 128),
    L34(3), L34(4),
    B("arp_on", "Arp On", 0),
    B("arp_latch", "Arp Key Latch", 0),
    N("arp_gate", "Arp Gate", 0, 127, 64),
    C("arp_type", "Arp Type", NARPTYPE, AT_UP, ARPTYPE),
    N("arp_rhythm", "Arp Rhythm", 1, NRHYTHM, 1),
    N("arp_oct", "Arp Octaves", 1, 6, 1),
    N("arp_swing", "Arp Swing", 20, 80, 50),
    {"arp_rate", "Arp SyncRate", 0, NARPSYNC - 1, 15, K_CHOICE, LFOSYNC_NAME + ARPSYNC_AT},
    B("arp_keysync", "Arp KeySync", 0),
    N("arp_chance", "Arp Chance", 0, 127, 127),
    {"bpm", "ClockRate", 40, 240, 120, K_BPM, nullptr},
    N("dist", "Distortion Level", 0, 127, 0),
    C("ch_type", "Chorus Type", 3, 1, CHTYPE),
    N("ch_rate", "Chorus Rate", 0, 127, 20),
    N("ch_level", "Chorus Level", 0, 127, 0),
    N("ch_depth", "Chorus Depth", 0, 127, 64),
    S("ch_fb", "Chorus Feedback", -64, 63, 0),
    N("ch_lp", "Chorus LoPass", 0, 127, 90),
    N("ch_hp", "Chorus HiPass", 0, 127, 2),
    C("ch_mode", "Chorus Mode", NCHMODE, CM_CHORUS, CHMODE),
    N("dl_time", "Delay Time", 0, 127, 64),
    B("dl_sync", "Delay Sync", 0),
    {"dl_syncrate", "Delay SyncRate", 0, NDLYSYNC - 1, 8, K_CHOICE, LFOSYNC_NAME + DLYSYNC_AT},
    N("dl_fb", "Delay Feedback", 0, 127, 64),
    N("dl_level", "Delay Level", 0, 127, 0),
    N("dl_lpd", "Delay LP Damp", 0, 127, 85),
    N("dl_hpd", "Delay HP Damp", 0, 127, 0),
    C("dl_ratio", "Delay L/R Ratio", NDLYRATIO, 0, DLYRATIO_NAME),
    N("dl_slew", "Delay SlewRate", 0, 127, 32),
    N("dl_width", "Delay Width", 0, 127, 127),
    C("dl_tmode", "Delay TimeMode", NDTMODE, DT_NORMAL, DTMODE),
    N("rv_time", "Reverb Time", 0, 127, 90),
    N("rv_level", "Reverb Level", 0, 127, 0),
    N("rv_size", "Reverb Size", 0, 127, 64),
    N("rv_pre", "Reverb PreDelay", 0, 127, 40),
    N("rv_lpd", "Reverb LP Damp", 0, 127, 50),
    N("rv_hpd", "Reverb HP Damp", 0, 127, 1),
    N("rv_mdepth", "Reverb ModDepth", 0, 127, 64),
    N("rv_mrate", "Reverb ModRate", 0, 127, 4),
    N("rv_lp", "Reverb LoPass", 0, 127, 74),
    N("rv_hp", "Reverb HiPass", 0, 127, 0),
    N("fx_wet", "FX WetLevel", 0, 127, 127),
    N("fx_dry", "FX DryLevel", 0, 127, 127),
    C("fx_route", "FX Routing", NFXROUTE, FR_PARALLEL, ROUTE),
    B("fx_bypass", "FX Bypass", 0),
    MOD(1), MOD(2), MOD(3), MOD(4), MOD(5), MOD(6), MOD(7), MOD(8),
    MOD(9), MOD(10), MOD(11), MOD(12), MOD(13), MOD(14), MOD(15), MOD(16),
    FXMOD(1), FXMOD(2), FXMOD(3), FXMOD(4),
};

static const char *const NOTE12[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void paramText(int p, int v, char *out, int n)
{
    const ParamDef &d = PARAM[p];
    if (v < d.lo)
        v = d.lo;
    if (v > d.hi)
        v = d.hi;
    switch (d.kind) {
    case K_SIGNED: std::snprintf(out, (size_t)n, v > 0 ? "+%d" : "%d", v); break;
    case K_CHOICE:
    case K_ONOFF: std::snprintf(out, (size_t)n, "%s", d.choice[v - d.lo]); break;
    case K_NOTE:
        if (v == 0)
            std::snprintf(out, (size_t)n, "Off");
        else
            std::snprintf(out, (size_t)n, "%s%d", NOTE12[v % 12], v / 12 - 2);
        break;
    case K_PHASE:
        if (v == 0)
            std::snprintf(out, (size_t)n, "Free");
        else
            std::snprintf(out, (size_t)n, "%ddeg", (v - 1) * 3);
        break;
    case K_REPEAT:
        if (v == 0)
            std::snprintf(out, (size_t)n, "Off");
        else if (v >= 127)
            std::snprintf(out, (size_t)n, "On");
        else
            std::snprintf(out, (size_t)n, "%d", v);
        break;
    case K_LREPEAT:
        if (v == 0)
            std::snprintf(out, (size_t)n, "Off");
        else
            std::snprintf(out, (size_t)n, "%d", v);
        break;
    case K_BPM: std::snprintf(out, (size_t)n, "%d BPM", v); break;
    default: std::snprintf(out, (size_t)n, "%d", v);
    }
}

void patchInit(Patch &p)
{
    std::memset(&p, 0, sizeof p);
    std::snprintf(p.name, NAME_LEN, "Init Patch");
    std::snprintf(p.category, sizeof p.category, "None");
    for (int i = 0; i < NPARAM; i++)
        p.v[i] = (int16_t)PARAM[i].def;
}
} // namespace pk
