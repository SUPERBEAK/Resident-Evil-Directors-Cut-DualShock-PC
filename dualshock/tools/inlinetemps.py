"""inlinetemps.py <in.c> <out.c> [--pfield] [--abs]: inline `temp_*`/`var_*` variables that are
assigned once and used once within the next few lines of the same straight-line block (no
`if`/`while`/label/brace in between). m2c splits Capcom's one-line expressions into such temps
when the original's register use confused it; at -O0 every temp is a stack slot, so the split
version never matches. Example (stage7 func_8011B890, func_8011ACA4):
    temp = A - p->x34; v = abs(temp); t2 = p->x3C; t3 = B - t2; v2 = abs(t3); *X = v + v2;
 -> *X = abs(A - p->x34) + abs(B - p->x3C);
--pfield: inlined M2C_PFIELD(...) becomes M2C_FIELD(...). --abs: only blocks that contain
__builtin_abs. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
flags = sys.argv[3:]
WINDOW = 6
STOP = re.compile(r"^\s*(if|else|while|for|do|switch|case|default|return|goto|\}|\{|\w+:)|[{}]\s*$")
SIMPLE = re.compile(r"^[\w.>\-\[\]]+$|^M2C_P?FIELD\([^()]*\)$|^\w+\([^()]*\)$")


def once():
    global s
    lines = s.split("\n")
    for i, l in enumerate(lines):
        m = re.match(r"^([ \t]*)((?:temp|var)_\w+) = (.+);$", l)
        if not m:
            continue
        ind, name, expr = m.groups()
        if len(re.findall(rf"\b{name}\b", s)) != 3:             # declaration, assignment, one use
            continue
        if "--abs" in flags and "__builtin_abs" not in "\n".join(lines[i:i + WINDOW + 1]):
            continue
        for j in range(i + 1, min(i + 1 + WINDOW, len(lines))):
            lj = lines[j]
            if re.search(rf"\b{name}\b", lj):
                if not lj.startswith(ind) or (len(lj) > len(ind) and lj[len(ind)] in " \t"):
                    break                                       # use at another depth
                e = expr
                if "--pfield" in flags:
                    e = e.replace("M2C_PFIELD(", "M2C_FIELD(")
                if not SIMPLE.match(e):
                    e = f"({e})"
                # a cast directly applied to the temp is dropped together with it
                lj2 = re.sub(rf"\((?:[us](?:8|16|32)|void \*)\) {name}\b", name, lj)
                lines[j] = re.sub(rf"\b{name}\b", lambda _: e, lj2, count=1)
                del lines[i]
                s = "\n".join(lines)
                s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{name};(?=\n)", "", s, count=1)
                return True
            if STOP.search(lj) or "(D_801FEC18 = " in lj or re.search(r"\bfunc_\w+\(", lj):
                break
    return False


while once():
    pass
open(sys.argv[2], "w", newline="\n").write(s)
