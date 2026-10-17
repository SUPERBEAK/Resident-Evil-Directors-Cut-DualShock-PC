"""distprobe.py - distance of a hand-edited draft, measured exactly like refine.py.

Run from dualshock/ with the project venv python (through buildenv.sh):
    python tools/distprobe.py <target> <func> <file.c>

Compiles <file.c> (a draft of <func>, without includes: the same preamble as refine is
added), links it at the function's original address and prints the number of differing
words (0 = byte-identical, None = doesn't compile/link). The built assembly stays in
build/refine/<target>/work/<func>/hand/m.s for comparison with asm/.../<func>.s.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import refine  # noqa: E402

ctx = refine.Ctx(sys.argv[1])
print(refine.distance(ctx, sys.argv[2], open(sys.argv[3]).read(), "hand"))
