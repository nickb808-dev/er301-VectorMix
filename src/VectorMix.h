/* VectorMix.h — 4-input stereo vector crossfader for ER-301 v0.3.0
 *
 * CONCEPT
 * ───────
 * Four audio inputs are placed at the corners of a unit square.
 * Two CV inputs (X, Y) move a cursor through that space.  Each input's
 * contribution is determined by bilinear interpolation weighted by a
 * variable panning law — smoothly CV-adjustable from circular blending
 * through linear crossfade to sharp four-quadrant gating.
 *
 * CORNER LAYOUT
 * ─────────────
 *   In3 ────────────── In4
 *    │                  │
 *    │      X, Y        │
 *    │         ●        │
 *    │                  │
 *   In1 ────────────── In2
 *
 *   X = −1 → left (In1 / In3),     X = +1 → right (In2 / In4)
 *   Y = −1 → bottom (In1 / In2),   Y = +1 → top   (In3 / In4)
 *
 * PANNING LAW
 * ───────────
 * Each axis uses the same generalised power-law shaper:
 *
 *   f(t, p) = t^p / (t^p + (1−t)^p)      (f(t) + f(1−t) = 1 for all p > 0)
 *
 *   law = −1  →  p ≈ 0.14  (circular: all four inputs always audible)
 *   law =  0  →  p = 1.0   (linear: standard bilinear interpolation)
 *   law = +1  →  p ≈ 7.4   (gate: only the nearest corner is heard)
 *
 * MIX LAW OPTION (v0.3.0 — unit menu, preset-serialised)
 * ──────────────────────────────────────────────────────
 *   Power 1 = amplitude — gains sum to 1 (constant amplitude; right for
 *             correlated material, −6 dB power dip at centre for four
 *             unrelated sources).
 *   Power 2 = equal power — the square roots of the bilinear weights are
 *             used, so Σg² = 1: constant loudness anywhere in the field
 *             for uncorrelated sources (centre = 0.5 per corner).
 *   (1-based values — the core OptionControl convention.)
 *
 * STEREO (v0.3.0)
 * ───────────────
 * The object is stereo-capable: each corner has an optional right-channel
 * inlet (In1R…In4R) and there is a second outlet (OutR).  The SAME gain set
 * drives both channels (the cursor pans the stereo image as a whole).  In a
 * mono lane the R ports are simply left unconnected (zero buffers, unused
 * outlet) — no extra configuration.
 *
 * SMOOTHING (v0.3.0)
 * ──────────────────
 * X/Y/Law/Level are read at BLOCK ENDPOINTS and the four pre-scaled gains
 * are linearly ramped across the block (the crossfilter v2.6.2 pattern).
 * This removes the 2.67 ms gain stair-step that made CV-swept vector moves
 * zipper.  Correct for CV slewing slower than the block rate (LFOs,
 * envelopes); for audio-rate X/Y patch a SlewLimiter.
 *
 * PORTS
 * ─────
 *   In1..In4     Corner audio, left channel (In1 = chain lane)
 *   In1R..In4R   Corner audio, right channel (stereo lanes; optional)
 *   X, Y         Cursor position [−1, +1]
 *   Law          Panning law exponent [−1, +1]
 *   Level        Output gain [0, 2]
 *   Out, OutR    Mixed output (left / right)
 *
 * CPU BUDGET
 * ──────────
 * Gains at block rate (≤ 10 transcendentals/block).  Sample loop: 8 MACs +
 * 4 ramp adds per sample — trivially NEON-vectorisable.  < 3% stereo. */

#pragma once

#include <od/objects/Object.h>
#include <od/config.h>

namespace vectormix {

class VectorMix : public od::Object
{
public:
    VectorMix();
    virtual ~VectorMix() = default;

#ifndef SWIGLUA

    void process() override;

    /* ── Ports ─────────────────────────────────────────────────────────── */
    od::Inlet  mIn1      {"In1"};
    od::Inlet  mIn2      {"In2"};
    od::Inlet  mIn3      {"In3"};
    od::Inlet  mIn4      {"In4"};
    od::Outlet mOut      {"Out"};
    od::Inlet  mXIn      {"X"};
    od::Inlet  mYIn      {"Y"};
    od::Inlet  mLawIn    {"Law"};
    od::Inlet  mLevelIn  {"Level"};
    // v0.3.0 — appended last (preserves port indices for existing wiring):
    od::Inlet  mIn1R     {"In1R"};
    od::Inlet  mIn2R     {"In2R"};
    od::Inlet  mIn3R     {"In3R"};
    od::Inlet  mIn4R     {"In4R"};
    od::Outlet mOutR     {"OutR"};

private:
    /* ── Constants ───────────────────────────────────────────────────────── */
    // law ∈ [−1, +1]  →  p = exp(law × kLawRange) ∈ [0.14, 7.39]
    static constexpr float kLawRange = 2.0f;

    // Mix law option (1-based): 1 = amplitude (Σg = 1), 2 = equal power
    // (Σg² = 1).  See header note.
    od::Option mPowerOpt {"Power", 1};

    /* f(t, p) = t^p / (t^p + (1−t)^p) — unity-sum shaper.  Boundary values
     * (t=0, t=1) handled explicitly to avoid 0^p and the p→0 indeterminate. */
    static float shapeLaw(float t, float p);

    // Compute the four gains (×level) for one parameter snapshot.
    void gainsAt(float x, float y, float law, float level, bool power,
                 float g[4]) const;

#endif // SWIGLUA
};

} // namespace vectormix
