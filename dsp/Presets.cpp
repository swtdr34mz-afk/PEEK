/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace pk
{
namespace
{
/* a patch made from the Init Patch, by the parameters' ids */
struct B {
    Patch p;
    B(const char *name, const char *cat)
    {
        patchInit(p);
        std::snprintf(p.name, NAME_LEN, "%s", name);
        std::snprintf(p.category, sizeof p.category, "%s", cat);
    }
    B &operator()(const char *id, int v)
    {
        for (int i = 0; i < NPARAM; i++)
            if (std::strcmp(PARAM[i].id, id) == 0) {
                p.v[i] = (int16_t)(v < PARAM[i].lo ? PARAM[i].lo : v > PARAM[i].hi ? PARAM[i].hi : v);
                return *this;
            }
        return *this;
    }
    /* a matrix slot: sources, destination, depth */
    B &mod(int slot, int sa, int sb, int dst, int depth)
    {
        int b = P_MOD1 + (slot - 1) * MOD_N;
        p.v[b + MOD_SA] = (int16_t)sa;
        p.v[b + MOD_SB] = (int16_t)sb;
        p.v[b + MOD_DST] = (int16_t)dst;
        p.v[b + MOD_DEPTH] = (int16_t)depth;
        return *this;
    }
    B &fxmod(int slot, int sa, int sb, int dst, int depth)
    {
        int b = P_FXMOD1 + (slot - 1) * MOD_N;
        p.v[b + MOD_SA] = (int16_t)sa;
        p.v[b + MOD_SB] = (int16_t)sb;
        p.v[b + MOD_DST] = (int16_t)dst;
        p.v[b + MOD_DEPTH] = (int16_t)depth;
        return *this;
    }
    /* the mod wheel's vibrato (LFO 2 on all three pitches) */
    B &vib(int slot = 1) { return mod(slot, S_MODWHEEL, S_LFO2B, D_O123PITCH, 8)("lfo2_rate", 150); }
};

std::vector<Patch> build()
{
    std::vector<B> b;
    /* 1 */
    b.push_back(B("Oxford Pad", "Pad")("o1_dense", 70)("o1_ddet", 58)("o2_wave", W_SAW)("o2_fine", 7)("o2_dense", 60)
                    ("o2_ddet", 70)("lvl2", 210)("o3_wave", W_TRI)("o3_range", 0)("lvl3", 150)("lvl1", 210)("freq", 132)
                    ("reso", 40)("fenv1", 30)("m1env_att", 70)("m1env_dec", 95)("m1env_sus", 60)("m1env_rel", 80)
                    ("aenv_att", 62)("aenv_dec", 90)("aenv_sus", 120)("aenv_rel", 84)("slope", 0)("drift", 30)
                    ("diverge", 40)("ch_level", 70)("ch_type", 2)("rv_level", 92)("rv_time", 100)("rv_size", 110)
                    ("lfo1_rate", 70)("flfo1", 6)("unispread", 70)("spreadmode", SP_ALTERNATE).vib());
    b.push_back(B("Brass Section", "Brass")("o2_wave", W_SAW)("o2_fine", -9)("lvl2", 230)("lvl1", 230)("freq", 95)
                    ("reso", 30)("drive", 70)("fenv1", 62)("m1env_att", 34)("m1env_dec", 70)("m1env_sus", 70)
                    ("m1env_rel", 50)("aenv_att", 22)("aenv_dec", 80)("aenv_sus", 115)("aenv_rel", 48)
                    ("aenv_vel", 30)("m1env_vel", 30)("ch_level", 40)("rv_level", 60).vib());
    b.push_back(B("Acid Spitter", "Bass")("mode", VM_MONOLG)("glide_on", 1)("glide", 44)("o1_range", 0)("freq", 70)
                    ("reso", 215)("fenv1", 70)("m1env_att", 0)("m1env_dec", 58)("m1env_sus", 0)("m1env_rel", 40)
                    ("m1env_vel", 40)("aenv_att", 0)("aenv_dec", 60)("aenv_sus", 90)("aenv_rel", 30)("postdrv", 70)
                    ("drive", 60)("keytrk", 64)("dl_level", 50)("dl_sync", 1)("dl_syncrate", 10)("dl_fb", 50)
                    ("o1_bend", 2));
    b.push_back(B("Glass FM Bells", "Bell")("o1_wave", W_SINE)("o2_wave", W_SINE)("o2_range", 2)("o2_coarse", 7)
                    ("lvl1", 230)("lvl2", 0)("o3_wave", W_SINE)("o3_range", 3)("lvl3", 0)("freq", 255)("keytrk", 0)
                    ("aenv_att", 0)("aenv_dec", 100)("aenv_sus", 0)("aenv_rel", 96)("aenv_vel", 35)
                    ("m2env_att", 0)("m2env_dec", 88)("m2env_sus", 0)("m2env_rel", 80)
                    .mod(1, S_MENV2, S_VELOCITY, D_FM31, 40).mod(2, S_DIRECT, S_DIRECT, D_FM23, 22)
                    ("lvl3", 0)("rv_level", 90)("rv_time", 96)("dl_level", 30)("dl_time", 70)("dl_ratio", 5));
    b.push_back(B("Wave Motion", "Pad")("o1_wave", W_MORE)("o1_more", 9)("o1_shsrc", SS_LFO1)("o1_shlfo", 63)
                    ("o2_wave", W_MORE)("o2_more", 29)("o2_fine", 8)("lvl2", 200)("o2_shsrc", SS_LFO1)("o2_shlfo", -50)
                    ("lfo1_rate", 52)("lfo1_type", LT_TRI)("freq", 170)("reso", 50)("aenv_att", 60)("aenv_rel", 86)
                    ("ch_level", 60)("rv_level", 100)("rv_time", 108)("rv_size", 127).vib());
    b.push_back(B("VSync Lead", "Lead")("mode", VM_MONO)("glide_on", 1)("glide", 30)("o1_vsync", 20)
                    ("o2_wave", W_SAW)("o2_fine", 6)("lvl2", 160)("freq", 190)("reso", 40)("drive", 90)
                    .mod(1, S_MENV2, S_DIRECT, D_O1VSYNC, 50)("m2env_att", 0)("m2env_dec", 80)("m2env_sus", 20)
                    .mod(2, S_MODWHEEL, S_LFO2B, D_O123PITCH, 9)("lfo2_rate", 155)("lfo2_fade", 60)
                    .mod(3, S_AFTERTOUCH, S_DIRECT, D_O1VSYNC, 40)("dl_level", 70)("dl_sync", 1)("dl_syncrate", 7)
                    ("dl_fb", 60)("rv_level", 50));
    b.push_back(B("Fat Mono Bass", "Bass")("mode", VM_MONO)("o1_range", 0)("o2_range", 0)("o2_wave", W_SQUARE)
                    ("o2_fine", -6)("lvl2", 200)("o3_wave", W_SINE)("o3_range", 0)("o3_coarse", -12)("lvl3", 180)
                    ("freq", 85)("reso", 60)("fenv1", 50)("m1env_att", 0)("m1env_dec", 64)("m1env_sus", 30)
                    ("aenv_att", 0)("aenv_dec", 70)("aenv_sus", 110)("aenv_rel", 28)("drive", 110)("keytrk", 70));
    b.push_back(B("Sub Thump", "Bass")("o1_wave", W_SINE)("o1_range", 0)("o2_wave", W_TRI)("o2_range", 0)
                    ("o2_coarse", 12)("lvl2", 110)("freq", 120)("drive", 150)("aenv_att", 0)("aenv_dec", 75)
                    ("aenv_sus", 100)("aenv_rel", 30)("m2env_att", 0)("m2env_dec", 40)("m2env_sus", 0)("o1_env2", 22)
                    ("o2_env2", 22)("mode", VM_MONO));
    b.push_back(B("Dark Strings", "Strings")("o1_dense", 90)("o1_ddet", 66)("o2_dense", 90)("o2_ddet", 80)
                    ("o2_fine", -5)("lvl2", 220)("lvl1", 220)("freq", 118)("reso", 20)("slope", 0)("aenv_att", 70)
                    ("aenv_sus", 127)("aenv_rel", 88)("fenv1", 20)("m1env_att", 76)("m1env_sus", 80)
                    ("ch_level", 95)("ch_type", 2)("ch_rate", 14)("rv_level", 85)("rv_time", 104).vib());
    b.push_back(B("Ring Garden", "Bell")("o1_wave", W_TRI)("o2_wave", W_SINE)("o2_range", 2)("o2_coarse", 5)
                    ("o2_fine", 17)("lvl_ring", 230)("lvl1", 90)("freq", 210)("aenv_att", 0)("aenv_dec", 92)
                    ("aenv_sus", 0)("aenv_rel", 90)("rv_level", 105)("rv_time", 110)("dl_level", 45)("dl_ratio", 6));
    b.push_back(B("FM Pluck", "Pluck")("o1_wave", W_TRI)("o3_wave", W_SINE)("o3_range", 3)("lvl3", 0)
                    .mod(1, S_MENV2, S_DIRECT, D_FM31, 42)("m2env_att", 0)("m2env_dec", 52)("m2env_sus", 0)
                    ("freq", 175)("fenv1", 40)("m1env_dec", 60)("m1env_sus", 0)("aenv_att", 0)("aenv_dec", 78)
                    ("aenv_sus", 0)("aenv_rel", 62)("rv_level", 70)("dl_level", 40)("dl_sync", 1)("dl_syncrate", 9));
    b.push_back(B("Noise Sweep", "FX")("lvl1", 0)("lvl_noise", 240)("fshape", FS_BP)("freq", 140)("reso", 190)
                    ("flfo1", 60)("lfo1_rate", 60)("lfo1_type", LT_TRI)("aenv_att", 50)("aenv_rel", 90)
                    ("rv_level", 100)("rv_time", 112)("dl_level", 50));
    b.push_back(B("Drive Stab", "Keys")("o2_wave", W_SAW)("o2_coarse", 7)("lvl2", 170)("freq", 120)("reso", 90)
                    ("fenv1", 55)("m1env_att", 0)("m1env_dec", 48)("m1env_sus", 10)("aenv_att", 0)("aenv_dec", 60)
                    ("aenv_sus", 0)("aenv_rel", 50)("postdrv", 80)("dist", 40)("rv_level", 70)("dl_level", 50)
                    ("dl_sync", 1)("dl_syncrate", 7));
    b.push_back(B("PWM Keys", "Keys")("o1_wave", W_SQUARE)("o1_shsrc", SS_LFO1)("o1_shlfo", 40)("o2_wave", W_SQUARE)
                    ("o2_shlfo", -40)("o2_fine", 6)("lvl2", 210)("lfo1_rate", 80)("freq", 150)("fenv1", 30)
                    ("m1env_dec", 80)("m1env_sus", 40)("aenv_att", 2)("aenv_dec", 90)("aenv_sus", 100)("aenv_rel", 64)
                    ("ch_level", 90)("ch_type", 0)("rv_level", 60).vib());
    b.push_back(B("Animate Me", "Pad")("o1_dense", 60)("o2_wave", W_MORE)("o2_more", 17)("lvl2", 190)("freq", 100)
                    ("reso", 80)("aenv_att", 40)("aenv_rel", 80).mod(1, S_ANIM1, S_DIRECT, D_FILTFREQ, 40)
                    .mod(2, S_ANIM2, S_LFO1B, D_O2SHAPE, 50).mod(3, S_ANIM2, S_DIRECT, D_O1VSYNC, 40)
                    ("an1_att", 60)("an1_rel", 70)("an2_att", 30)("an2_rel", 60)("lfo1_rate", 100)("rv_level", 90)
                    ("ch_level", 60));
    b.push_back(B("Arp Machine", "Arp")("arp_on", 1)("arp_rhythm", 7)("arp_type", AT_UPDOWN1)("arp_oct", 2)
                    ("arp_gate", 50)("o2_wave", W_SQUARE)("o2_range", 2)("lvl2", 140)("freq", 110)("reso", 110)
                    ("fenv1", 45)("m1env_att", 0)("m1env_dec", 44)("m1env_sus", 0)("aenv_att", 0)("aenv_dec", 60)
                    ("aenv_sus", 60)("aenv_rel", 40).mod(1, S_LFO3B, S_DIRECT, D_FILTFREQ, 20)("lfo3_sync", 13)
                    ("dl_level", 70)("dl_sync", 1)("dl_syncrate", 7)("dl_fb", 70)("dl_ratio", 5)("rv_level", 60));
    b.push_back(B("Hollow Organ", "Organ")("o1_wave", W_MORE)("o1_more", 6)("o1_shape", 10)("o2_wave", W_SINE)
                    ("o2_range", 2)("lvl2", 140)("o3_wave", W_SINE)("o3_range", 3)("o3_coarse", 7)("lvl3", 90)
                    ("aenv_att", 0)("aenv_dec", 60)("aenv_sus", 127)("aenv_rel", 30)("dist", 30)("ch_level", 85)
                    ("ch_type", 2)("ch_rate", 40)("rv_level", 55).vib());
    b.push_back(B("Screamer", "Lead")("mode", VM_MONO)("glide_on", 1)("glide", 36)("o2_wave", W_SAW)("o2_fine", -11)
                    ("lvl2", 230)("drive", 200)("postdrv", 60)("dist", 70)("freq", 150)("reso", 120)("fenv1", 30)
                    ("m1env_dec", 70)("m1env_sus", 70).vib()("dl_level", 60)("dl_time", 60)("rv_level", 50));
    b.push_back(B("Shimmer Drone", "Atmos")("o1_wave", W_MORE)("o1_more", 56)("o1_shsrc", SS_LFO1)("o1_shlfo", 40)
                    ("o2_wave", W_MORE)("o2_more", 44)("o2_range", 0)("lvl2", 180)("lfo1_rate", 30)("drift", 70)
                    ("diverge", 60)("aenv_att", 95)("aenv_rel", 110)("freq", 200)("rv_level", 127)("rv_time", 122)
                    ("rv_size", 127)("rv_pre", 70)("ch_level", 60)("ch_type", 2));
    b.push_back(B("Formant Voice", "Vocal")("o1_wave", W_MORE)("o1_more", 9)("o1_shsrc", SS_ENV1)("o1_shenv", 60)
                    ("m1env_att", 60)("m1env_dec", 90)("m1env_sus", 40)("o2_wave", W_MORE)("o2_more", 12)("o2_fine", 9)
                    ("lvl2", 190)("aenv_att", 30)("aenv_rel", 76)("freq", 190).vib()("ch_level", 70)("rv_level", 80));
    b.push_back(B("Unison Saws", "Lead")("unison", 3)("unidet", 40)("unispread", 100)("o2_wave", W_SAW)
                    ("o2_coarse", 12)("lvl2", 150)("freq", 175)("reso", 30)("aenv_rel", 50)("rv_level", 60)("dl_level", 40));
    b.push_back(B("Poly Brass", "Brass")("o1_wave", W_SAW)("o2_wave", W_SQUARE)("o2_shape", 20)("lvl2", 180)
                    ("freq", 100)("fenv1", 58)("m1env_att", 40)("m1env_dec", 80)("m1env_sus", 60)("aenv_att", 28)
                    ("aenv_sus", 110)("aenv_rel", 54)("slope", 0)("reso", 40)("ch_level", 50).vib());
    b.push_back(B("Laser Zap", "FX")("o1_wave", W_SQUARE)("o1_env2", 120)("m2env_att", 0)("m2env_dec", 40)
                    ("m2env_sus", 0)("o1_range", 2)("freq", 200)("reso", 120)("aenv_att", 0)("aenv_dec", 50)
                    ("aenv_sus", 0)("aenv_rel", 40)("dl_level", 60)("dl_fb", 70));
    b.push_back(B("Thump Drum", "Perc")("o1_wave", W_SINE)("o1_fixnote", 36)("o1_env2", 60)("m2env_att", 0)
                    ("m2env_dec", 34)("m2env_sus", 0)("lvl_noise", 60)("noise_hpf", 60)("aenv_att", 0)("aenv_dec", 56)
                    ("aenv_sus", 0)("aenv_rel", 40)("drive", 120)("freq", 200)("mode", VM_POLY2));
    b.push_back(B("Wind Tunnel", "Atmos")("lvl1", 0)("lvl_noise", 255)("fshape", FS_HP)("freq", 150)("reso", 170)
                    ("flfo1", 50)("lfo1_rate", 40)("aenv_att", 80)("aenv_rel", 100)("rv_level", 110)("rv_time", 115));
    b.push_back(B("Tape Echo Keys", "Keys")("o1_wave", W_TRI)("o2_wave", W_SAW)("lvl2", 90)("freq", 160)
                    ("aenv_dec", 80)("aenv_sus", 60)("aenv_rel", 60)("dl_level", 100)("dl_time", 80)("dl_fb", 80)
                    ("dl_tmode", DT_DOUBLE)("dl_slew", 20)("dl_lpd", 55)("rv_level", 50).vib());
    b.push_back(B("Phlanger Pad", "Pad")("o1_dense", 80)("o2_wave", W_SAW)("o2_fine", 12)("lvl2", 200)("freq", 140)
                    ("aenv_att", 60)("aenv_rel", 90)("ch_level", 110)("ch_mode", CM_PHLANGER)("ch_fb", 40)("ch_rate", 10)
                    ("rv_level", 80));
    b.push_back(B("Looping Env", "Seq")("o1_wave", W_SAW)("freq", 90)("reso", 150)("fenv1", 60)("m1env_att", 0)
                    ("m1env_dec", 44)("m1env_sus", 0)("m1env_rep", 127)("aenv_att", 0)("aenv_sus", 127)("aenv_rel", 70)
                    ("dl_level", 60)("dl_sync", 1)("rv_level", 60));
    b.push_back(B("Bit Chip", "Lead")("o1_wave", W_MORE)("o1_more", 41)("o1_shape", -20)("o2_wave", W_MORE)
                    ("o2_more", 40)("o2_range", 2)("lvl2", 120)("mode", VM_MONO)("glide_on", 1)("glide", 20)
                    ("aenv_att", 0)("aenv_rel", 30)("lfo2_rate", 170).mod(1, S_DIRECT, S_LFO2B, D_O123PITCH, 6)
                    ("lfo2_fade", 50)("dl_level", 50)("dl_sync", 1));
    b.push_back(B("Growl Bass", "Bass")("mode", VM_MONO)("o1_range", 0)("o3_wave", W_SINE)("o3_range", 0)
                    ("o3_coarse", 7)("fosc3", 120)("freq", 80)("reso", 140)("drive", 120)("postdrv", 50)
                    .mod(1, S_MODWHEEL, S_DIRECT, D_O3FILT, 50).mod(2, S_LFO1B, S_DIRECT, D_O3PITCH, 10)
                    ("lfo1_rate", 120)("aenv_att", 0)("aenv_rel", 30));
    b.push_back(B("Silk Strings", "Strings")("o1_dense", 50)("o2_wave", W_SAW)("o2_dense", 50)("o2_fine", 4)
                    ("lvl2", 200)("o3_wave", W_SQUARE)("o3_range", 0)("lvl3", 90)("freq", 150)("slope", 0)
                    ("aenv_att", 54)("aenv_rel", 80)("ch_level", 110)("ch_type", 2)("rv_level", 80)("rv_time", 100)
                    ("unispread", 60).vib());
    /* the Init Patch, last */
    b.push_back(B("Init Patch", "None"));
    /* each patch's level, so that they all sit about alike (a chord about -15 dB RMS) */
    static const int16_t LEVEL[NFACTORY] = {59, 50, 95, 93, 65, 104, 108, 127, 71, 127, 113, 107, 78, 20, 126, 127, 20, 20, 76, 86, 51, 33, 107, 127, 72, 104, 26, 60, 127, 60, 68, 64};
    std::vector<Patch> out;
    for (size_t i = 0; i < b.size(); i++) {
        out.push_back(b[i].p);
        if (i < (size_t)NFACTORY)
            out.back().v[P_PATCHLVL] = LEVEL[i];
    }
    return out;
}
const std::vector<Patch> &store()
{
    static const std::vector<Patch> v = build();
    return v;
}
} // namespace

const Patch *const FACTORY = store().data();
} // namespace pk
