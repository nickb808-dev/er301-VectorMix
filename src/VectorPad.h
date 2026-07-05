/* VectorPad.h — X/Y cursor graphic for VectorMix v0.2.0
 *
 * Renders a square pad with crosshairs and a filled-circle dot that tracks the
 * current X and Y parameter values.  Designed to fill one SECTION_PLY (42 px)
 * slot in the ER-301 main display.
 *
 * LAYOUT (42 × 64 px slot)
 * ─────────────────────────
 *   ┌──────────────────────┐  ←  top of slot  (mWorldBottom + 64)
 *   │  small top margin    │
 *   ├──────────────────────┤  padTop
 *   │                      │
 *   │   ·  ·  +  ·  ·     │  ← crosshairs (GRAY3)
 *   │           ●          │  ← cursor dot (WHITE, fillCircle r=2)
 *   │                      │
 *   ├──────────────────────┤  padBottom
 *   │  "xy" label (GRAY7)  │
 *   └──────────────────────┘  mWorldBottom
 *
 * The outer box is drawn in GRAY6.  Interior is 37×37 px.
 * X maps [-1,+1] → [left+1, right-1]; Y maps [-1,+1] → [bottom+1, top-1].
 *
 * USAGE FROM LUA
 * ──────────────
 *   local pad = libvectormix.VectorPad(0, 0, app.SECTION_PLY, 64)
 *   pad:setXParameter(objects.xParam:getParameter("Bias"))
 *   pad:setYParameter(objects.yParam:getParameter("Bias")) */

#pragma once

#include <od/graphics/Graphic.h>

// Forward declarations — these types are %import-ed in mod.cpp.swig so SWIG
// can marshal pointer arguments; full types included below under SWIGLUA guard.
namespace od { class Parameter; }
namespace od { class Outlet; }

#ifndef SWIGLUA
#include <od/objects/Parameter.h>
#include <od/objects/Object.h>   // provides od::Outlet (and od::Inlet)
#endif

namespace vectormix {

class VectorPad : public od::Graphic
{
public:
    VectorPad(int left, int bottom, int width, int height);
    virtual ~VectorPad() = default;

    // Wire up the bias parameters — used as fallback when no outlet is set.
    // Called from Lua after construction; safe to call with nullptr to detach.
    void setXParameter(od::Parameter *p);
    void setYParameter(od::Parameter *p);

    // Wire up the GainBias output outlets so CV modulation moves the dot.
    // When set, draw() reads outlet->buffer()[0] instead of parameter->value().
    // Safe to call with nullptr to detach.
    void setXOutlet(od::Outlet *o);
    void setYOutlet(od::Outlet *o);

#ifndef SWIGLUA
    virtual void draw(od::FrameBuffer &fb) override;

private:
    od::Parameter *mpX        = nullptr;
    od::Parameter *mpY        = nullptr;
    od::Outlet    *mpXOutlet  = nullptr;
    od::Outlet    *mpYOutlet  = nullptr;
#endif
};

} // namespace vectormix
