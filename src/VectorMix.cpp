/* VectorMix.cpp — 4-input stereo vector crossfader for ER-301 v0.3.0
 *
 * See VectorMix.h for full algorithm and port documentation.
 *
 * SIGNAL FLOW PER BLOCK
 * ──────────────────────
 *   1. Read X, Y, Law, Level at BOTH block endpoints (buffer[0] and
 *      buffer[N−1]) — clamped (Level to [0, 2]: negative CV can no longer
 *      invert polarity, hot CV can no longer amplify without bound).
 *   2. Compute the four bilinear gains (× level) at each endpoint via
 *      gainsAt(): normalise → law exponent → shapeLaw per axis → bilinear
 *      products → optional equal-power sqrt → × level.
 *   3. Ramp the four gains linearly across the block (endpoint
 *      interpolation, crossfilter v2.6.2 pattern) — removes the per-block
 *      gain stair-step that zippered CV-swept vector moves.
 *   4. Sample loop: one gain set drives BOTH channels:
 *        outL[s] = Σ gk·inkL[s]      outR[s] = Σ gk·inkR[s]
 *      (mono lanes leave the R ports unconnected — zero buffers.)
 *
 * MIX LAW
 * ────────
 *   amplitude (default): Σg = 1 — constant amplitude, right for correlated
 *     material; four unrelated sources dip −6 dB (power) at centre.
 *   equal power: gains = sqrt(bilinear weights) so Σg² = 1 — constant
 *     loudness for uncorrelated sources anywhere in the field.
 *
 * OPTIMISATIONS
 * ──────────────
 *   • All transcendentals (2× expf, 8× powf, ≤ 8× sqrtf) at block rate.
 *   • Sample loop: 8 MACs + 4 adds, no branches — NEON-vectorisable. */

#include "VectorMix.h"

#include <algorithm>
#include <cmath>

namespace vectormix {

/* ── Constructor ─────────────────────────────────────────────────────────────── */

VectorMix::VectorMix()
{
    addInput(mIn1);
    addInput(mIn2);
    addInput(mIn3);
    addInput(mIn4);
    addOutput(mOut);
    addInput(mXIn);
    addInput(mYIn);
    addInput(mLawIn);
    addInput(mLevelIn);
    // v0.3.0 stereo ports — appended last.
    addInput(mIn1R);
    addInput(mIn2R);
    addInput(mIn3R);
    addInput(mIn4R);
    addOutput(mOutR);

    addOption(mPowerOpt);
}

/* ── shapeLaw ────────────────────────────────────────────────────────────────── */

// Unity-sum power-law shaper: f(t) + f(1−t) = 1 for all p > 0.
float VectorMix::shapeLaw(float t, float p)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float tp = powf(t,        p);
    const float cp = powf(1.0f - t, p);
    return tp / (tp + cp);
}

/* ── gainsAt — four gains (× level) for one parameter snapshot ──────────────── */

void VectorMix::gainsAt(float x, float y, float law, float level, bool power,
                        float g[4]) const
{
    const float Xn = (x + 1.0f) * 0.5f;
    const float Yn = (y + 1.0f) * 0.5f;
    const float p  = expf(law * kLawRange);
    const float fX = shapeLaw(Xn, p);
    const float fY = shapeLaw(Yn, p);

    //   In3 ─────── In4          g[2] = (1−fX)·fY     g[3] = fX·fY
    //    │     ●     │
    //   In1 ─────── In2          g[0] = (1−fX)·(1−fY) g[1] = fX·(1−fY)
    g[0] = (1.0f - fX) * (1.0f - fY);
    g[1] =          fX * (1.0f - fY);
    g[2] = (1.0f - fX) *          fY;
    g[3] =          fX *          fY;

    if (power) {
        // Equal power: Σ(√g)² = Σg = 1 by the bilinear identity.
        g[0] = sqrtf(g[0]);
        g[1] = sqrtf(g[1]);
        g[2] = sqrtf(g[2]);
        g[3] = sqrtf(g[3]);
    }

    g[0] *= level;
    g[1] *= level;
    g[2] *= level;
    g[3] *= level;
}

/* ── process() ──────────────────────────────────────────────────────────────── */

void VectorMix::process()
{
    const float *i1L = mIn1.buffer();
    const float *i2L = mIn2.buffer();
    const float *i3L = mIn3.buffer();
    const float *i4L = mIn4.buffer();
    const float *i1R = mIn1R.buffer();
    const float *i2R = mIn2R.buffer();
    const float *i3R = mIn3R.buffer();
    const float *i4R = mIn4R.buffer();
    float       *oL  = mOut.buffer();
    float       *oR  = mOutR.buffer();
    const int N = FRAMELENGTH;

    const bool power = (mPowerOpt.value() >= 2);   // 1-based option

    // ── Endpoint parameter reads (v0.3.0: block-boundary interpolation) ────
    const float *xB = mXIn.buffer();
    const float *yB = mYIn.buffer();
    const float *lB = mLawIn.buffer();
    const float *vB = mLevelIn.buffer();
    const float x0 = std::max(-1.0f, std::min(xB[0],     1.0f));
    const float x1 = std::max(-1.0f, std::min(xB[N - 1], 1.0f));
    const float y0 = std::max(-1.0f, std::min(yB[0],     1.0f));
    const float y1 = std::max(-1.0f, std::min(yB[N - 1], 1.0f));
    const float l0 = std::max(-1.0f, std::min(lB[0],     1.0f));
    const float l1 = std::max(-1.0f, std::min(lB[N - 1], 1.0f));
    const float v0 = std::max(0.0f,  std::min(vB[0],     2.0f));
    const float v1 = std::max(0.0f,  std::min(vB[N - 1], 2.0f));

    float ga[4], gb[4];
    gainsAt(x0, y0, l0, v0, power, ga);
    gainsAt(x1, y1, l1, v1, power, gb);

    const float inv = 1.0f / float(N - 1);
    const float d0 = (gb[0] - ga[0]) * inv;
    const float d1 = (gb[1] - ga[1]) * inv;
    const float d2 = (gb[2] - ga[2]) * inv;
    const float d3 = (gb[3] - ga[3]) * inv;
    float g0 = ga[0], g1 = ga[1], g2 = ga[2], g3 = ga[3];

    // ── Sample loop: one ramped gain set drives both channels ──────────────
    for (int s = 0; s < N; ++s) {
        oL[s] = g0 * i1L[s] + g1 * i2L[s] + g2 * i3L[s] + g3 * i4L[s];
        oR[s] = g0 * i1R[s] + g1 * i2R[s] + g2 * i3R[s] + g3 * i4R[s];
        g0 += d0;
        g1 += d1;
        g2 += d2;
        g3 += d3;
    }
}

} // namespace vectormix
