"""Whole-codebase scan for leftover decompiler patterns (run from dualshock/). Prints counts per pattern and
per folder, plus the top files. Comments are stripped before matching code patterns."""
import os
import re
import sys
from collections import Counter, defaultdict

COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)

PATTERNS = {
    # names
    "D_/func_ names": r"\b(?:D|func|jtbl)_[0-9A-F]{8}\b",
    "unk/pad fields": r"(?:->|\.)(?:unk|pad|field)_?[0-9A-Fa-f]+\b",
    "decompiler locals (sp/temp/var/arg)": r"\b(?:sp[0-9A-F]{2,3}|temp_[a-z0-9_]+|var_[a-z][0-9]\w*|phi_\w+|saved_reg_\w+)\b",
    "argN params": r"\barg\d+\b",
    "hex-suffixed local types": r"\b[A-Z][A-Za-z]*_?[0-9A-F]{5,8}\b",
    # m2c-isms
    "M2C_ macros": r"\bM2C_\w+",
    "FIELD_AT/raw field macros": r"\bFIELD_AT\b|\bFIELD\w*\(",
    "LIT/LITP fixed addresses": r"\bLITP?\(",
    "__asm__ aliases": r"__asm__\(\"\w+\"\)",
    "__asm__ statements (non-alias)": r"__asm__\((?!\"\w+\"\))",
    "goto": r"\bgoto\b",
    "statement exprs ({ })": r"\(\{",
    "register keyword": r"\bregister\b",
    "dead blocks { register ... }": r"\{\s*register\b",
    "casts to (s32)/(u32)": r"\((?:s32|u32)\)",
    "casts to (u8)/(s8)/(u16)/(s16)": r"\((?:u8|s8|u16|s16)\)",
    "pointer casts (T *)": r"\(\s*(?:u8|s8|u16|s16|s32|u32|void)\s*\*+\s*\)",
    "& 0xFF masks": r"&\s*0xFF\b",
    "double parens (( ))": r"\(\(\w+\)\)",
    "implicit-int K&R protos ()": r"\b\w+\s*\(\)\s*;",
    "block-scope extern": r"^\s{4,}extern\b",
    "block-scope prototypes": r"^\s{4,}(?:void|s32|u32|s16|u16|u8|s8|int|long)\s+\w+\s*\([^;{]*\)\s*;",
    "magic big hex (>=0x100)": r"\b0x[0-9A-Fa-f]{3,}\b",
    "/* irregular */ markers": None,
    "do {} while (0)": r"do\s*\{[^{}]*\}\s*while\s*\(\s*0\s*\)",
    "if (0)/while (1) ": r"\bif\s*\(\s*0\s*\)",
    "empty statements ';;'": r";\s*;",
    "unused locals named unused": r"\bunused\w*\b",
}
RAW = {"/* irregular */ markers": r"/\*\s*irregular\s*\*/",
       "TODO/FIXME/XXX": r"\b(?:TODO|FIXME|XXX|HACK)\b",
       "?/unknown in comments": r"/\*[^*]*\?[^*]*\*/"}

files = []
for d, _, fs in os.walk("src"):
    for f in sorted(fs):
        if f.endswith((".c", ".h")):
            files.append(os.path.join(d, f).replace(os.sep, "/"))
for f in sorted(os.listdir("include")):
    if f.endswith(".h") and f not in ("re1_views.h", "re1_locals.h", "re1_addrs.h"):
        files.append("include/" + f)

total = Counter()
per_dir = defaultdict(Counter)
per_file = defaultdict(Counter)
for p in files:
    raw = open(p, encoding="latin-1").read()
    code = COMMENT.sub("", raw)
    d = p.split("/")[1] if p.startswith("src/") else "include"
    for name, pat in PATTERNS.items():
        if pat is None:
            continue
        n = len(re.findall(pat, code, re.M))
        total[name] += n
        per_dir[d][name] += n
        per_file[name][p] += n
    for name, pat in RAW.items():
        n = len(re.findall(pat, raw, re.M))
        total[name] += n
        per_dir[d][name] += n
        per_file[name][p] += n

dirs = sorted(per_dir)
print(f"{'pattern':42s} {'total':>6s}  " + " ".join(f"{d[:7]:>7s}" for d in dirs))
for name in list(PATTERNS) + list(RAW):
    if PATTERNS.get(name, 1) is None:
        continue
    print(f"{name:42s} {total[name]:6d}  " + " ".join(f"{per_dir[d][name]:7d}" for d in dirs))
if len(sys.argv) > 1:
    for name in sys.argv[1:]:
        print(f"\n== top files: {name}")
        for p, n in per_file[name].most_common(8):
            if n:
                print(f"  {n:5d} {p}")
