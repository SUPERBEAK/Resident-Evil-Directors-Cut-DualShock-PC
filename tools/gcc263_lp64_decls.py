"""gcc263_lp64_decls.py <gcc-2.6.3 source dir>: declare the pointer-returning functions that
GCC 2.6.3 calls without a prototype.

Our cc1 is a 64-bit (LP64) MSYS2 build. An implicitly declared function returns `int`, so the
caller keeps only the low 32 bits of the returned rtx/tree pointer (sign-extended). When the heap
pointer is above 2 GB the compiler crashes - e.g. reorg.c's `negate_rtx` call crashed the
delay-slot pass (dbr) on ordinary `do { i--; ... } while (...)` loops (main func_8001A248 drafts).
When the pointer is small the truncation is harmless, so declaring the functions only turns
crashes into the normal result; it cannot change code that already compiled.

The declarations use `struct rtx_def *` / `union tree_node *` so they need no GCC headers and are
inserted right after `#include "config.h"`."""
import os
import re
import sys

ROOT = sys.argv[1]
RTX, TREE = "struct rtx_def *", "union tree_node *"
FIX = {
    "sdbout.c": [(RTX, "alter_subreg")],
    "expr.c": [(TREE, "truthvalue_conversion")],
    "insn-emit.c": [(RTX, "emit")],
    "stmt.c": [(RTX, "emit_insns_after"), (TREE, "reorder_blocks")],
    "c-typeck.c": [(RTX, "expand_expr")],
    "stor-layout.c": [(RTX, "expand_expr")],
    "c-decl.c": [(RTX, "label_rtx")],
    "reorg.c": [(RTX, "negate_rtx")],
    "jump.c": [(RTX, "remove_death"), (RTX, "replace_regs")],
    "local-alloc.c": [(RTX, "remove_death")],
    "loop.c": [(RTX, "replace_regs")],
}
for name, decls in FIX.items():
    path = os.path.join(ROOT, name)
    if not os.path.exists(path):          # insn-emit.c is generated during the build
        continue
    text = open(path, errors="replace").read()
    if "LP64 host fix" in text:
        continue
    block = ("/* LP64 host fix: pointer-returning functions this file calls without a declaration */\n"
             + "".join(f"extern {t}{f} ();\n" for t, f in decls))
    m = re.search(r'^#include "config.h"\n', text, re.M)
    pos = m.end() if m else 0
    text = text[:pos] + block + text[pos:]
    open(path, "w", newline="\n").write(text)
    print("patched", name)
