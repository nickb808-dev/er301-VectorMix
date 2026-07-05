/* VectorPad.cpp — X/Y cursor graphic implementation for VectorMix v0.2.0
 *
 * See VectorPad.h for layout documentation.
 *
 * COORDINATE SYSTEM
 * ─────────────────
 * The ER-301 graphics subsystem places (0,0) at the BOTTOM-LEFT of the screen.
 * Y increases upward.  mWorldLeft / mWorldBottom are the absolute screen
 * coordinates of the bottom-left corner of this graphic's area; they are set
 * by the framework before each draw() call.
 *
 * THREAD SAFETY
 * ─────────────
 * draw() runs on the graphics thread.  od::Parameter::value() is read on the
 * same thread.  The DSP thread writes parameter values as single float stores;
 * on ARM Cortex-A8 aligned 32-bit stores/loads are atomic, so there is no
 * data race — we read an either-old-or-new value, never a torn write. */

#include "VectorPad.h"
#include <od/graphics/constants.h>
#include <algorithm>

namespace vectormix {

VectorPad::VectorPad(int left, int bottom, int width, int height)
    : od::Graphic(left, bottom, width, height)
{
}

void VectorPad::setXParameter(od::Parameter *p)
{
    mpX = p;
}

void VectorPad::setYParameter(od::Parameter *p)
{
    mpY = p;
}

void VectorPad::setXOutlet(od::Outlet *o)
{
    mpXOutlet = o;
}

void VectorPad::setYOutlet(od::Outlet *o)
{
    mpYOutlet = o;
}

void VectorPad::draw(od::FrameBuffer &fb)
{
    // ── Layout ────────────────────────────────────────────────────────────
    // Square pad: offset 4 px from the left edge so there's a visible gap
    // between the pad and the unit title to its left.  The right side keeps
    // 2 px clearance.  Remaining vertical space holds the "xy" label below.

    const int padLeft   = mWorldLeft + 4;             // 3 px extra left gap
    const int padRight  = mWorldLeft + mWidth - 2;    // mWorldLeft + 40
    const int side      = padRight - padLeft;         // 36 px
    const int vMargin   = (mHeight - side) / 2;       // 14 px
    const int padBottom = mWorldBottom + vMargin;
    const int padTop    = padBottom + side;
    const int centerX   = (padLeft + padRight) / 2;
    const int centerY   = padBottom + side / 2;

    // ── Border box ────────────────────────────────────────────────────────
    fb.box(GRAY6, padLeft, padBottom, padRight, padTop);

    // ── Crosshairs ────────────────────────────────────────────────────────
    // Very faint (GRAY3) lines through the centre — visual origin reference.
    fb.hline(GRAY3, padLeft  + 1, padRight  - 1, centerY);
    fb.vline(GRAY3, centerX, padBottom + 1, padTop - 1);

    // ── Cursor dot ────────────────────────────────────────────────────────
    // Prefer the GainBias outlet buffer (CV + bias) so that patched CV moves
    // the dot.  Fall back to the bias Parameter when no outlet is wired, and
    // to 0 (centre) if neither is set.
    float x = mpXOutlet ? mpXOutlet->buffer()[0]
                        : (mpX ? mpX->value() : 0.0f);
    float y = mpYOutlet ? mpYOutlet->buffer()[0]
                        : (mpY ? mpY->value() : 0.0f);

    // Clamp to [-1, +1].
    x = std::max(-1.0f, std::min(x, 1.0f));
    y = std::max(-1.0f, std::min(y, 1.0f));

    // Map [-1,+1] → pixel within the inner area (padLeft+1 … padRight-1).
    const int innerW = padRight - padLeft - 2;    // 37 px
    const int innerH = padTop   - padBottom - 2;  // 37 px
    const int dotX   = padLeft  + 1 + (int)((x + 1.0f) * 0.5f * float(innerW) + 0.5f);
    const int dotY   = padBottom + 1 + (int)((y + 1.0f) * 0.5f * float(innerH) + 0.5f);

    // Clamp so the dot never overlaps the border line.
    const int clampedX = std::max(padLeft  + 1, std::min(dotX, padRight  - 1));
    const int clampedY = std::max(padBottom + 1, std::min(dotY, padTop    - 1));

    fb.fillCircle(WHITE, clampedX, clampedY, 2);

    // ── Label ─────────────────────────────────────────────────────────────
    // "xy" in the lower margin.  ALIGN_MIDDLE centres the text vertically on y.
    fb.text(GRAY7, centerX, mWorldBottom + vMargin / 2, "xy", 10, ALIGN_MIDDLE);
}

} // namespace vectormix
