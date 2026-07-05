/* libvectormix.swig — SWIG interface for the VectorMix ER-301 Object.
 *
 * COMPILE (inside Docker via `make swig-docker`):
 *   swig -c++ -lua \
 *        -I/er301_sdk \
 *        -Isrc \
 *        -o src/libvectormix_wrap.cpp src/libvectormix.swig
 *
 * LUA USAGE:
 *   local lib = require "vectormix.libvectormix"
 *   local vm  = self:addObject("vm", lib.VectorMix())
 *   connect(self, "In1", vm, "In1")
 *   connect(vm, "Out", self, "Out1")
 *
 * DESIGN NOTES
 * ────────────
 * Follows the exact pattern from er-301/tutorial/minimal/minimal.cpp.swig:
 *
 *   1. %include <od/glue/mod.cpp.swig>
 *      Pulls in the ER-301 boilerplate: %import(module="app") directives for
 *      od::Object, od::Inlet, od::Outlet, od::Parameter, etc., plus the
 *      #undef SWIGLUA / #define SWIGLUA trick that lets Object.h compile with
 *      the full ExecutionTimer type.
 *
 *   2. Our own %{ %} block repeats the trick for VectorMix.h so the generated
 *      wrapper .cpp sees the complete class definition when it compiles.
 *
 *   3. %include <VectorMix.h> at the SWIG parser level is clean because all
 *      DSP internals are hidden inside #ifndef SWIGLUA — SWIG only sees the
 *      constructor and destructor. */

%module vectormix_libvectormix

/* ── ER-301 boilerplate: Object / Inlet / Outlet / Parameter imports ──────── */
%include <od/glue/mod.cpp.swig>

/* ── Compiled implementation block (not processed by the SWIG parser) ──────── */
%{
/* Temporarily expose the full (non-SWIG) headers so the generated wrapper
 * .cpp has complete type information for od::Object and its dependencies
 * (including od::ExecutionTimer via Profiler.h). */
#undef SWIGLUA
#include <VectorMix.h>
#define SWIGLUA
using namespace vectormix;
%}

/* ── VectorMix exposed to Lua ─────────────────────────────────────────────── */
/* SWIG sees this with SWIGLUA defined, so only the constructor / destructor
 * are visible — exactly the same trick used by GainBias.h and the tutorial. */
%include <VectorMix.h>

/* ── VectorPad exposed to Lua ─────────────────────────────────────────────── */
/* VectorPad is an od::Graphic subclass — od::Graphic is already %import-ed
 * above via mod.cpp.swig, so SWIG knows the base type.  od::Parameter is also
 * %import-ed, so setXParameter / setYParameter are marshaled correctly.
 *
 * Pattern mirrors VectorMix above: implementation block provides the full
 * header to the generated .cpp, then %include exposes only the SWIGLUA-visible
 * surface (constructor + setXParameter + setYParameter) to the Lua binding. */
%{
#undef SWIGLUA
#include <VectorPad.h>
#define SWIGLUA
using namespace vectormix;
%}
%include <VectorPad.h>
