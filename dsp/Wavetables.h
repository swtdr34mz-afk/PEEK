/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
/* PEEK's 60 wavetables (its own, drawn here; not the Peak's): five waves each, 256 points a cycle, band-limited in
 * eight copies (128 harmonics down to 1, an octave apart) so that a note never folds back. Made once, shared. */
#pragma once

namespace pk
{
constexpr int WT_LEN = 256, WT_WAVES = 5, WT_MIPS = 8;
/* a wave's points (WT_LEN + 1: the first again at the end): table, wave 0..4, mip 0 (128 harmonics) .. 7 (one) */
const float *wtWave(int table, int wave, int mip);
void wtInit();                                         /* (made on first use; this makes them now) */
} // namespace pk
