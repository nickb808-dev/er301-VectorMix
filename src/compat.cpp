/* compat.cpp — local stubs for symbols the TI RTOS firmware does not export.
 *
 * Partially linked into libvectormix.so via -r, so these symbols are
 * resolved within the package rather than by the firmware's ELF loader.
 *
 * CRITICAL: do NOT include <stdio.h> or <cstdio> in this file.
 * Those headers declare fprintf with a FILE* first argument; our stub uses
 * void* to avoid the type dependency.  The symbol names are unmangled
 * (extern "C") and the calling convention is identical (pointer + variadic),
 * so the partial linker resolves the SWIG wrapper's fprintf references here.
 *
 * IMPORTANT: compiled WITHOUT -ffast-math (see CXXFLAGS_COMPAT in Makefile)
 * so GCC does not re-combine sinf+cosf below back into sincosf recursively.
 */

#include <stdarg.h>   /* va_list — does NOT pull in stdio/FILE */

extern "C" {

/* ── fprintf ─────────────────────────────────────────────────────────────────
 * The SWIG Lua runtime calls fprintf(stderr, ...) for type-mismatch and
 * assertion errors.  TI RTOS does not export fprintf.  Provide a silent
 * no-op so all such calls are resolved locally and produce no output.
 *
 * Using void* for `stream` avoids any dependency on the FILE typedef. */
int fprintf(void *stream, const char *fmt, ...)
{
    (void)stream;
    (void)fmt;
    return 0;
}

/* ── __getreent ──────────────────────────────────────────────────────────────
 * With -D__DYNAMIC_REENT__, newlib on arm-none-eabi (GCC 10.3) replaces the
 * global _impure_ptr with a call to __getreent() (two leading underscores)
 * wherever the reentrant I/O struct is needed (e.g. the `stderr` macro).
 * TI RTOS firmware does not export __getreent.  Return a pointer to a zeroed
 * static buffer; safe because our fprintf stub above never dereferences it. */
static char _reent_stub[256];   /* zeroed at startup; large enough for any field */
void *__getreent(void) { return _reent_stub; }

/* ── sincosf ─────────────────────────────────────────────────────────────────
 * GCC's -ffast-math combines sinf(x) + cosf(x) pairs into a single
 * sincosf(x, s, c) call.  TI RTOS may not export sincosf.  Provide a local
 * implementation using sinf + cosf, which ARE in the firmware's export table.
 * The `volatile float vx` prevents GCC from seeing this as the same pattern
 * and re-optimising back into sincosf recursively. */
extern float sinf(float);
extern float cosf(float);

void sincosf(float x, float *sinp, float *cosp)
{
    volatile float vx = x;
    *sinp = sinf(vx);
    *cosp = cosf(vx);
}

} /* extern "C" */
