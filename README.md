# VectorMix ER-301 Package

**Version 0.3.0**

A 4-input vector crossfader for the ER-301. Four audio signals are placed at the corners of a unit square. Two CV inputs (X and Y) move a cursor through that space, blending the four inputs by their distance from the cursor. A third CV input (Law) morphs the blend from circular overlap through linear crossfade to sharp quadrant gating.

**New in v0.3.0:** true stereo in stereo lanes (chain L/R at corner 1, stereo corner
branches, one shared gain set driving both channels); corner faders with metering
(replacing dials that injected DC offset); block-interpolated gains (CV-swept vector
moves no longer zipper); cursor defaults to corner 1 so inserting the unit is
transparent; and an **equal-power mix law** menu option (constant loudness for four
unrelated sources — amplitude mode remains the default and is right for correlated
material).

## Corner layout

```
  In3 ──────────── In4
   │    X, Y         │
   │       ●         │
  In1 ──────────── In2

  X = −1 → left  (In1 / In3)     X = +1 → right (In2 / In4)
  Y = −1 → bottom (In1 / In2)    Y = +1 → top   (In3 / In4)
```

In1 is connected automatically from the chain. In2, In3, and In4 are branch patch points — navigate into them to patch any mono audio source.

## Panning law

Each axis uses a generalised power-law shaper:

```
f(t, p) = t^p / (t^p + (1−t)^p)
```

This satisfies `f(t) + f(1−t) = 1` for all `p > 0`, so the four corner gains always sum to unity regardless of the law setting. The overall output level never changes as you move the cursor.

The law exponent `p` is mapped from the Law CV:

```
p = exp(Law × 2.0)

Law = −1  →  p ≈ 0.14  (circular)
Law =  0  →  p = 1.00  (linear)    ← default
Law = +1  →  p ≈ 7.39  (gate)
```

**Circular (Law < 0):** Influence zones are broad and overlapping. All four inputs are always audible across most of the XY field. Moving to a corner emphasises that input but never fully silences the others.

**Linear (Law = 0):** Standard bilinear interpolation. Each corner is fully heard only when the cursor sits exactly at that corner. The centre gives equal weight (0.25) to all four inputs.

**Gate (Law > 0):** Influence zones sharpen into quadrants. At high Law values only the input in the cursor's current quadrant is audible. The unit acts as a four-way audio gate.

## Controls

| Control | Range | Default | Description |
|---------|-------|---------|-------------|
| **In2** | patch | — | Bottom-right corner. Patch any mono source. |
| **In3** | patch | — | Top-left corner. Patch any mono source. |
| **In4** | patch | — | Top-right corner. Patch any mono source. |
| **x** | −1 to +1 | 0 | Horizontal cursor position. CV-patchable. |
| **y** | −1 to +1 | 0 | Vertical cursor position. CV-patchable. |
| **law** | −1 to +1 | 0 | Panning law exponent. −1 circular, 0 linear, +1 gate. CV-patchable. |
| **level** | 0 – 1 | 1.0 | Output gain. |

## Instructions

1. Place VectorMix in a chain. The chain audio appears at **In1** (bottom-left).
2. Navigate into **In2**, **In3**, and **In4** to patch three more audio sources into the remaining corners.
3. At default settings (x=0, y=0, law=0) all four inputs are blended equally at 0.25 each — the cursor is at the centre.
4. Move **x** left to emphasise In1/In3; right for In2/In4.
5. Move **y** down to emphasise In1/In2; up for In3/In4.
6. Patch an LFO into **x** to sweep left-right continuously.
7. Patch a second LFO (different rate) into **y** to create Lissajous-style paths through the mix field.
8. Increase **law** toward +1 to sharpen the zones — the cursor becomes a hard switch between inputs.
9. Decrease **law** toward −1 for maximum overlap — all four inputs blend smoothly regardless of cursor position.
10. Patch slow random CV (S&H or Turing Machine) into **x** and **y** simultaneously for an unpredictable four-way mix that drifts between inputs organically.
11. Modulate **law** with a slow envelope to morph from a gentle blend during the attack to a sharp gate at the peak.

## Notes

- **Unity gain is guaranteed at all cursor positions.** The four weights always sum to 1 regardless of X, Y, or Law. You will not hear level changes as you move the cursor in linear mode.
- **In a stereo chain the output is mono.** VectorMix produces a single blended signal. Both outputs receive identical audio. This is intentional: VectorMix handles blend, not stereo placement. Use a panner after VectorMix if you want the result positioned in the stereo field.
- **Cursor at centre with law=0 gives −12 dB effective gain per input.** Each of the four sources contributes 0.25 of its amplitude. If you want the centre position to be louder, increase law toward +1 (narrows zones) or reduce the number of active inputs.
- **Law CV is clamped to [−1, +1].** Patching a signal that swings wider is safe; it clips at the law boundaries without distortion.

## CPU

All mixing gains are computed once per 128-sample block, not per sample. The sample loop is four multiply-adds with no branches, making it trivially NEON-vectorisable. Estimated CPU on the AM3358 Cortex-A8 @ 600 MHz: **< 2%** in a mono or stereo chain.

## Building

macOS (Docker):

```bash
make docker-image
make swig-docker ER301_SDK=~/er-301
make docker-build ER301_SDK=~/er-301
make pkg
```

Linux (native):

```bash
make swig ER301_SDK=~/er-301
make build TOOLCHAIN=native ER301_SDK=~/er-301
make pkg
```

Output: `build/am335x/vectormix-<version>.pkg`
