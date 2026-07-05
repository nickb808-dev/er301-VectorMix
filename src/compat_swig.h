/* compat_swig.h — included before the SWIG wrapper via -include.
 *
 * Suppresses calls to C library functions that are not exported by the
 * TI RTOS firmware.  Must be processed before any #include <stdio.h>
 * so that the macro definitions take effect for all subsequent code.
 *
 * How it works: defining fprintf as a variadic macro causes the C
 * preprocessor to expand every fprintf(...) call site to ((void)0) before
 * the compiler sees it.  No call instruction is emitted, so no external
 * symbol reference for fprintf or stderr appears in the object file.
 */
#pragma once

/* fprintf(stderr, ...) — used by the SWIG Lua runtime for error diagnostics.
 * TI RTOS does not export fprintf or provide a FILE/stderr implementation.
 * Suppressing here also eliminates the _impure_ptr reference that comes from
 * newlib expanding `stderr` as `(&_impure_ptr->_stderr_r)`. */
#define fprintf(f, ...) ((void)0)
