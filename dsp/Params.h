/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK's parameters, laid out as a Novation Peak patch (its user guide 1.2 and the 2.0 firmware's additions): three
 * oscillators, the common oscillator page, the mixer, the filter, glide and the voice menu, three envelopes, the
 * animate envelopes, four LFOs, the arp and clock, the effects, a 16-slot modulation matrix and a 4-slot FX matrix.
 * Every value is an integer in the Peak's own range (FREQUENCY 0..255, SHAPE -64..+63, the matrix depth -64..+63, ...),
 * so what the display shows is what the Peak shows. */
#pragma once
#include <cstdint>

namespace pk
{
/* ------------------------------------------------------------------ choices --- */
enum Wave { W_SINE, W_TRI, W_SAW, W_SQUARE, W_MORE, NWAVE };
enum ShapeSrc { SS_MANUAL, SS_ENV1, SS_LFO1, NSHSRC };
enum FShape { FS_LP, FS_BP, FS_HP, NFSHAPE };
enum VoiceMode { VM_MONO, VM_MONOLG, VM_MONO2, VM_POLY, VM_POLY2, NVMODE };
enum SpreadMode { SP_DIVERGE, SP_ALTERNATE, SP_DIVERGE2, SP_NOTEVAL, NSPREAD };
enum LfoType { LT_TRI, LT_SAW, LT_SQUARE, LT_SH, NLFOTYPE };
enum LfoRange { LR_LOW, LR_HIGH, LR_SYNC, NLFORANGE };
enum FadeMode { FM_FADEIN, FM_FADEOUT, FM_GATEIN, FM_GATEOUT, NFADEMODE };
enum ArpType { AT_UP, AT_DOWN, AT_UPDOWN1, AT_UPDOWN2, AT_PLAYED, AT_RANDOM, AT_CHORD, AT_CHORD2, NARPTYPE };
enum ChorusMode { CM_CHORUS, CM_FLANGER, CM_PHLANGER, NCHMODE };
enum DelayTimeMode { DT_NORMAL, DT_DOUBLE, DT_TREBLE, DT_QUAD, DT_HEX, NDTMODE };
enum FxRoute { FR_PARALLEL, FR_DRC, FR_DCR, FR_RDC, FR_RCD, FR_CDR, FR_CRD, NFXROUTE };
constexpr int NWAVETABLE = 60, NUNISON = 5, NLFOSYNC = 35, NARPSYNC = 19, NDLYSYNC = 16, NDLYRATIO = 11;
constexpr int NRHYTHM = 33;

/* the modulation sources (the 2.0 firmware's list, its order) */
enum ModSrc {
    S_DIRECT, S_MODWHEEL, S_AFTERTOUCH, S_EXPR, S_BREATH, S_VELOCITY, S_KEYBOARD, S_LFO1P, S_LFO1B, S_LFO2P, S_LFO2B,
    S_AMPENV, S_MENV1, S_MENV2, S_ANIM1, S_ANIM2, S_CV, S_LFO3P, S_LFO3B, S_LFO4P, S_LFO4B, S_BENDUP, S_BENDDN, S_NOISE,
    NMODSRC
};
/* the destinations */
enum ModDst {
    D_O123PITCH, D_O1PITCH, D_O2PITCH, D_O3PITCH, D_O1VSYNC, D_O2VSYNC, D_O3VSYNC, D_O1SHAPE, D_O2SHAPE, D_O3SHAPE,
    D_O1LEV, D_O2LEV, D_O3LEV, D_NOISELEV, D_RINGLEV, D_VCALEVEL, D_FILTDRV, D_FILTDIST, D_FILTFREQ, D_FILTRES,
    D_LFO1RATE, D_LFO2RATE, D_AENV_A, D_AENV_D, D_AENV_R, D_M1ENV_A, D_M1ENV_D, D_M1ENV_R, D_M2ENV_A, D_M2ENV_D,
    D_M2ENV_R, D_FM12, D_FM23, D_FM31, D_FMN1, D_O3FILT, D_NFILT, D_PAN, D_O123SHAPE, D_AENV_DLY, D_AENV_HLD,
    D_AENV_SUS, D_M1ENV_DLY, D_M1ENV_HLD, D_M1ENV_SUS, D_M2ENV_DLY, D_M2ENV_HLD, D_M2ENV_SUS, NMODDST
};
/* the FX matrix's destinations */
enum FxDst {
    FD_DIST, FD_CH_LEV, FD_CH_RATE, FD_CH_DEPTH, FD_CH_FB, FD_DL_LEV, FD_DL_TIME, FD_DL_FB, FD_RV_LEV, FD_RV_TIME,
    FD_RV_LDAMP, FD_RV_HDAMP, FD_RV_SIZE, FD_RV_LPASS, FD_RV_HPASS, FD_DL_SLEW, FD_DL_LDAMP, FD_DL_HDAMP, FD_DL_WIDTH,
    FD_DL_RATIO, FD_LFO3RATE, FD_LFO4RATE, FD_NOISELPF, FD_NOISEHPF, NFXDST
};

/* ---------------------------------------------------------------- the patch --- */
enum Param {
    /* OSCILLATOR 1..3: OSC_N each */
    P_OSC1, OSC_RANGE = 0, OSC_COARSE, OSC_FINE, OSC_WAVE, OSC_MORE, OSC_ENV2, OSC_LFO2, OSC_SHSRC, OSC_SHMAN, OSC_SHENV,
    OSC_SHLFO, OSC_VSYNC, OSC_DENSE, OSC_DDET, OSC_FIXNOTE, OSC_BEND, OSC_N,
    P_OSC_END = P_OSC1 + 3 * OSC_N,
    /* the common oscillator pages */
    P_DIVERGE = P_OSC_END, P_DRIFT, P_NOISE_LPF, P_NOISE_HPF, P_KEYSYNC,
    /* MIXER */
    P_LVL1, P_LVL2, P_LVL3, P_LVL_NOISE, P_LVL_RING, P_VCA_GAIN,
    /* FILTER (and the voice menu's two) */
    P_FREQ, P_RESO, P_DRIVE, P_FSHAPE, P_SLOPE, P_FENV_SRC, P_FENV1, P_FENVA, P_FLFO1, P_FOSC3, P_KEYTRK, P_POSTDRV,
    P_FDIVERGE,
    /* GLIDE, VOICE */
    P_GLIDE_ON, P_GLIDE, P_PREGLIDE, P_MODE, P_UNISON, P_UNIDET, P_UNISPREAD, P_SPREADMODE, P_PAN, P_PATCHLVL,
    /* AMP ENVELOPE, MOD ENVELOPE 1, 2: ENV_N each */
    P_AENV, ENV_ATT = 0, ENV_DEC, ENV_SUS, ENV_REL, ENV_DLY, ENV_HOLD, ENV_VEL, ENV_TRIG, ENV_REP, ENV_N,
    P_M1ENV = P_AENV + ENV_N, P_M2ENV = P_M1ENV + ENV_N,
    P_MENV_SEL = P_M2ENV + ENV_N,
    /* ANIMATE's envelopes */
    P_AN1_ATT, P_AN1_REL, P_AN2_ATT, P_AN2_REL,
    /* LFO 1, 2: LFO_N each */
    P_LFO1, LFO_TYPE = 0, LFO_RANGE, LFO_RATE, LFO_SYNC, LFO_FADE, LFO_FMODE, LFO_FSYNC, LFO_PHASE, LFO_TRIG, LFO_SLEW,
    LFO_REP, LFO_COMMON, LFO_N,
    P_LFO2 = P_LFO1 + LFO_N,
    /* LFO 3, 4 (global): L34_N each */
    P_LFO3 = P_LFO2 + LFO_N, L34_WAVE = 0, L34_RATE, L34_SYNC, L34_PHASE, L34_SLEW, L34_FADE, L34_N,
    P_LFO4 = P_LFO3 + L34_N,
    /* ARP, CLOCK */
    P_ARP_ON = P_LFO4 + L34_N, P_ARP_LATCH, P_ARP_GATE, P_ARP_TYPE, P_ARP_RHYTHM, P_ARP_OCT, P_ARP_SWING, P_ARP_RATE,
    P_ARP_KEYSYNC, P_ARP_CHANCE, P_BPM,
    /* DISTORTION, CHORUS, DELAY, REVERB, the FX's global page */
    P_DIST, P_CH_TYPE, P_CH_RATE, P_CH_LEVEL, P_CH_DEPTH, P_CH_FB, P_CH_LP, P_CH_HP, P_CH_MODE,
    P_DL_TIME, P_DL_SYNC, P_DL_SYNCRATE, P_DL_FB, P_DL_LEVEL, P_DL_LPD, P_DL_HPD, P_DL_RATIO, P_DL_SLEW, P_DL_WIDTH,
    P_DL_TMODE,
    P_RV_TIME, P_RV_LEVEL, P_RV_SIZE, P_RV_PRE, P_RV_LPD, P_RV_HPD, P_RV_MDEPTH, P_RV_MRATE, P_RV_LP, P_RV_HP,
    P_FX_WET, P_FX_DRY, P_FX_ROUTE, P_FX_BYPASS,
    /* the modulation matrix: 16 slots of MOD_N */
    P_MOD1, MOD_SA = 0, MOD_SB, MOD_DST, MOD_DEPTH, MOD_N,
    P_MOD_END = P_MOD1 + 16 * MOD_N,
    /* the FX matrix: 4 slots of MOD_N */
    P_FXMOD1 = P_MOD_END,
    P_FXMOD_END = P_FXMOD1 + 4 * MOD_N,
    NPARAM = P_FXMOD_END
};

enum Kind : uint8_t {
    K_NUM,      /* a number as it is */
    K_SIGNED,   /* with its sign (+12, -3) */
    K_CHOICE,   /* a name from the list */
    K_ONOFF,
    K_NOTE,     /* FIXED NOTE: Off, then a MIDI note's name */
    K_PHASE,    /* Free, 0deg .. 357deg */
    K_REPEAT,   /* an envelope's REPEATS: Off, 1..126, On */
    K_LREPEAT,  /* an LFO's: Off, 1..127 */
    K_BPM
};

struct ParamDef {
    const char *id;           /* the host's id (stable: saved projects) */
    const char *name;         /* the host's and the display's name */
    int lo, hi, def;
    Kind kind;
    const char *const *choice;
};

extern const ParamDef PARAM[NPARAM];
extern const char *const MODSRC_NAME[NMODSRC];
extern const char *const MODDST_NAME[NMODDST];
extern const char *const FXDST_NAME[NFXDST];
extern const char *const WAVETABLE_NAME[NWAVETABLE];
extern const char *const LFOSYNC_NAME[NLFOSYNC];
extern const float LFOSYNC_BEATS[NLFOSYNC];
/* the arp's rates are the LFO table's from 8 beats on, the delay's from 4 beats on */
constexpr int ARPSYNC_AT = 16, DLYSYNC_AT = 19;
static_assert(ARPSYNC_AT + NARPSYNC == NLFOSYNC && DLYSYNC_AT + NDLYSYNC == NLFOSYNC, "the sync tables");
extern const char *const DLYRATIO_NAME[NDLYRATIO];
extern const float DLYRATIO_L[NDLYRATIO], DLYRATIO_R[NDLYRATIO];
extern const int UNISON_VOICES[NUNISON];
extern const char *const WAVE_NAME[NWAVE];
extern const char *const LFOTYPE_NAME[NLFOTYPE];

/* the value as the display shows it */
void paramText(int p, int v, char *out, int n);
/* the oscillator block a parameter belongs to (-1 none), the LFO's */
inline int oscOf(int p) { return p >= P_OSC1 && p < P_OSC_END ? (p - P_OSC1) / OSC_N : -1; }

/* a whole patch */
constexpr int NAME_LEN = 20;
struct Patch {
    char name[NAME_LEN];
    char category[12];
    int16_t v[NPARAM];
};
void patchInit(Patch &p);                              /* the Init Patch (the Peak's) */

} // namespace pk
