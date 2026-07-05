// VectorMix host verification.  Build with -Dprivate=public.
// Modes:
//   sum     amplitude law: DC 1 into all four corners → out == level over an
//           (x, y, law) grid (Σg = 1 identity)
//   power   equal-power law: Σg² = 1 → four uncorrelated noises give flat
//           output power across the field; single-input centre gain = 0.5
//   stereo  L/R independence through shared gains (v0.3.0)
//   zipper  continuous X CV ramp: max per-sample output step, old vs new
#include "VectorMix.h"
#include <cstdio>
#include <cstring>
#include <cmath>

using vectormix::VectorMix;

static void fill(od::Port &p, float v) { for (int i = 0; i < FRAMELENGTH; ++i) p.buffer()[i] = v; }

static uint32_t rng = 0x1234;
static float frand() { rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5;
                       return float(rng & 0x7FFFFFFFu)/float(0x7FFFFFFFu)*2.f - 1.f; }

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "mode?\n"); return 2; }

    if (!strcmp(argv[1], "sum")) {
        VectorMix d;
        fill(d.mIn1, 1.0f); fill(d.mIn2, 1.0f); fill(d.mIn3, 1.0f); fill(d.mIn4, 1.0f);
        fill(d.mLevelIn, 1.0f);
        float maxErr = 0;
        for (float x = -1.0f; x <= 1.001f; x += 0.25f)
        for (float y = -1.0f; y <= 1.001f; y += 0.25f)
        for (float law = -1.0f; law <= 1.001f; law += 0.5f) {
            fill(d.mXIn, x); fill(d.mYIn, y); fill(d.mLawIn, law);
            d.process();
            for (int s = 0; s < FRAMELENGTH; ++s)
                maxErr = std::max(maxErr, fabsf(d.mOut.buffer()[s] - 1.0f));
        }
        printf("sum (amplitude): max |out-1| = %.2e over grid\n", maxErr);
        return 0;
    }

#ifdef HAS_STEREO
    if (!strcmp(argv[1], "power")) {
        // (a) single input at centre → gain 0.5 exactly
        {
            VectorMix d;
            d.mPowerOpt.set(2);
            fill(d.mIn1, 1.0f); fill(d.mLevelIn, 1.0f);
            fill(d.mXIn, 0.0f); fill(d.mYIn, 0.0f); fill(d.mLawIn, 0.0f);
            d.process();
            printf("power: centre single-input gain = %.4f (expect 0.5)\n",
                   d.mOut.buffer()[64]);
        }
        // (b) four independent noises → output power flat across the field
        {
            VectorMix d;
            d.mPowerOpt.set(2);
            fill(d.mLevelIn, 1.0f); fill(d.mLawIn, 0.0f);
            float minP = 1e9f, maxP = 0;
            for (float x = -1.0f; x <= 1.001f; x += 0.5f)
            for (float y = -1.0f; y <= 1.001f; y += 0.5f) {
                fill(d.mXIn, x); fill(d.mYIn, y);
                double e = 0; long n = 0;
                for (int b = 0; b < 400; ++b) {
                    for (int s = 0; s < FRAMELENGTH; ++s) {
                        d.mIn1.buffer()[s] = frand();
                        d.mIn2.buffer()[s] = frand();
                        d.mIn3.buffer()[s] = frand();
                        d.mIn4.buffer()[s] = frand();
                    }
                    d.process();
                    for (int s = 0; s < FRAMELENGTH; ++s) {
                        e += double(d.mOut.buffer()[s]) * d.mOut.buffer()[s]; n++;
                    }
                }
                const float p = float(e / n);
                minP = std::min(minP, p); maxP = std::max(maxP, p);
            }
            printf("power: field power spread = %.2f dB (min %.4f max %.4f)\n",
                   10.0f * log10f(maxP / minP), minP, maxP);
        }
        return 0;
    }

    if (!strcmp(argv[1], "stereo")) {
        VectorMix d;
        fill(d.mIn1, 1.0f); fill(d.mIn1R, -1.0f);
        fill(d.mIn2, 0.3f); fill(d.mIn2R, 0.7f);
        fill(d.mLevelIn, 1.0f); fill(d.mLawIn, 0.0f);
        fill(d.mXIn, -1.0f); fill(d.mYIn, -1.0f);   // corner 1
        d.process();
        printf("stereo corner1: L=%.4f (expect +1) R=%.4f (expect -1)\n",
               d.mOut.buffer()[64], d.mOutR.buffer()[64]);
        fill(d.mXIn, 1.0f);                          // corner 2
        d.process(); d.process();                    // settle the ramp
        printf("stereo corner2: L=%.4f (expect 0.3) R=%.4f (expect 0.7)\n",
               d.mOut.buffer()[64], d.mOutR.buffer()[64]);
        return 0;
    }
#endif

    if (!strcmp(argv[1], "zipper")) {
        // Continuous X CV ramp written per sample (as a real CV source would);
        // In2 = DC 1 so out = g2(x(t)).  Metric: max per-sample output step.
        VectorMix d;
        fill(d.mIn2, 1.0f);
        fill(d.mYIn, -1.0f); fill(d.mLawIn, 0.0f); fill(d.mLevelIn, 1.0f);
        float maxStep = 0, prev = 0; bool first = true;
        long t = 0;
        const long T = 480 * FRAMELENGTH;            // full sweep over ~1.28 s
        for (int b = 0; b < 480; ++b) {
            for (int s = 0; s < FRAMELENGTH; ++s, ++t)
                d.mXIn.buffer()[s] = -1.0f + 2.0f * float(t) / float(T);
            d.process();
            for (int s = 0; s < FRAMELENGTH; ++s) {
                const float y = d.mOut.buffer()[s];
                if (!first) maxStep = std::max(maxStep, fabsf(y - prev));
                prev = y; first = false;
            }
        }
        printf("zipper: max per-sample step = %.3e (ideal continuous ≈ %.1e)\n",
               maxStep, 2.0 / double(T));
        return 0;
    }

    fprintf(stderr, "unknown mode\n");
    return 2;
}
