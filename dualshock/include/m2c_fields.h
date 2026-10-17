/*
 * m2c_fields.h - m2c's macros plus this project's two forms of its field access.
 *
 * Only for decompiler drafts (tools/autodecomp.py, refine.py, ... through
 * compiler_matrix.preamble_for); no source file in src/ includes it.
 * m2c_macros.h is m2c's own header (GPL-3.0, not in git): tools/m2c_macros.py downloads it
 * from the m2c repository into include/.
 */
#ifndef M2C_FIELDS_H
#define M2C_FIELDS_H

#include "m2c_macros.h"

/* Two explicit forms of a field access (tools/refine.py picks per access):
 *   M2C_SFIELD: a real (anonymous) struct member: `sb $v1,0x85($v0)` at -O0, like the original,
 *               instead of computing the address separately. Requires GCC's __typeof__.
 *   M2C_PFIELD: pointer arithmetic: at -O0 `addiu; move; sb ..,0(..)` (array-element style) */
#define M2C_SFIELD(expr, type_ptr, offset) (((struct { char m2c_pad[(offset) > 0 ? (offset) : 0]; __typeof__(*(type_ptr)0) m2c_f; } *)((offset) < 0 ? (void *)((s8 *)(expr) + (offset)) : (void *)(expr)))->m2c_f)
#define M2C_PFIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))

#undef M2C_FIELD
#ifdef M2C_FIELD_STRUCT
/* Overlays (-O0): defined by the Makefile / tools for -O0 targets. */
#define M2C_FIELD M2C_SFIELD
#else
/* Main executable (-O2): m2c's original pointer arithmetic, which matches there
 * (the struct form changes GCC 2.6.3's -O2 scheduling, e.g. set_ladder_zone). */
#define M2C_FIELD M2C_PFIELD
#endif

#endif /* M2C_FIELDS_H */
