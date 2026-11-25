/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (PEEK) */
#include "Dsp.h"
#include <algorithm>
#include <vector>

namespace pk
{
const float *blepTables()
{
    /* PROPHET 67's: a Kaiser-windowed sinc (cutoff 0.9 of Nyquist, beta 8) over 16 samples, integrated once (the
     * band-limited step) and its residual once more (the ramp's), 1024 points a sample. Made once, shared */
    static const std::vector<float> t = [] {
        const int H = BL_H, OS = BL_OS, N = BL_N;
        const double fc = 0.9, beta = 8.0, pi = 3.14159265358979323846;
        auto i0 = [](double x) {
            double s = 1, term = 1;
            for (int k = 1; k < 60; k++) {
                term *= (x / (2 * k)) * (x / (2 * k));
                s += term;
            }
            return s;
        };
        std::vector<double> h(N), B(N), R(N);
        for (int i = 0; i < N; i++) {
            double x = -H + (double)i / OS, r = x / H;
            double w = std::fabs(r) <= 1 ? i0(beta * std::sqrt(1 - r * r)) / i0(beta) : 0;
            double sc = x == 0 ? 1.0 : std::sin(pi * fc * x) / (pi * fc * x);
            h[i] = fc * sc * w;
        }
        double acc = 0;
        B[0] = 0;
        for (int i = 1; i < N; i++)
            B[i] = (acc += 0.5 * (h[i - 1] + h[i]) / OS);
        double total = B[2 * H * OS];
        for (int i = 0; i < N; i++)
            B[i] /= total;
        acc = 0;
        R[0] = 0;
        for (int i = 1; i < N; i++) {
            double x0 = -H + (double)(i - 1) / OS, x1 = -H + (double)i / OS;
            double e0 = B[i - 1] - (x0 >= 0 ? 1.0 : 0.0), e1 = B[i] - (x1 >= 0 ? 1.0 : 0.0);
            if (x0 < 0 && x1 >= 0)
                e0 = B[i - 1];
            R[i] = (acc += 0.5 * (e0 + e1) / OS);
        }
        double drift = R[2 * H * OS];
        std::vector<float> out(2 * N);
        for (int i = 0; i < N; i++) {
            out[i] = (float)B[i];
            out[N + i] = (float)(R[i] - drift * std::min(1.0, (double)i / (2 * H * OS)));
        }
        out[N - 1] = 1.0f;
        out[2 * N - 1] = 0.0f;
        return out;
    }();
    return t.data();
}

} // namespace pk
