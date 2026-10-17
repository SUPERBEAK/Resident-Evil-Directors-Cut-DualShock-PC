"""sfield.py <in.c> <out.c> [all|stores|loads|base:<expr>]: turn `M2C_FIELD(...)` accesses of a
main-program draft into `M2C_SFIELD(...)` (a real struct member).

In main (-O2) M2C_FIELD is a plain pointer cast, so the MEM is not MEM_IN_STRUCT_P. GCC 2.6.3's
alias rule lets a struct-member access with a varying address pass a fixed-address scalar
(`*(u16 *)0x800C8660`, `D_800C8660`) during scheduling, so whether a load is hoisted past a
store depends on that flag (main func_8005B8C0, func_8002BEB0 family).

Modes: all (default) - every M2C_FIELD; stores - only on the left of `=`; loads - only
elsewhere; base:<expr> - only the ones whose first argument is <expr>. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
mode = sys.argv[3] if len(sys.argv) > 3 else "all"
FIELD = re.compile(r"\bM2C_FIELD\(")


def args_end(text, i):
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                return j
    return -1


out, pos = [], 0
for m in FIELD.finditer(s):
    start = m.start()
    if start < pos:
        continue
    end = args_end(s, m.end() - 1)
    if end < 0:
        break
    call = s[start:end + 1]
    inner = call[len("M2C_FIELD("):-1]
    base = inner.split(",")[0].strip()
    after = s[end + 1:end + 6]
    is_store = re.match(r"\s*(?:[-+*/&|^]|<<|>>)?=(?!=)", after) is not None
    take = (mode == "all" or (mode == "stores" and is_store) or (mode == "loads" and not is_store)
            or (mode.startswith("base:") and base == mode[5:]))
    out.append(s[pos:start])
    out.append("M2C_SFIELD(" + inner + ")" if take else call)
    pos = end + 1
out.append(s[pos:])
open(sys.argv[2], "w", newline="\n").write("".join(out))
