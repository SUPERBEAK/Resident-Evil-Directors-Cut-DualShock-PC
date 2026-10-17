"""plusone.py <in.c> <out.c>: statement `X += 1;` / `X -= 1;` / `X = (T) (X - 1);` / `X = X + 1;`
-> `X++;` / `X--;`. On a global or scratch-pad variable, Capcom's `X++` leaves
`addiu v1,v0,1; move v0,v1; sw v0` where `X += 1` stores v1 directly; m2c prints the move-version
in these forms (ending func_800E3A20, stage4 func_80119150). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"^([ \t]*)([^;\n=]+?) ([+-])= 1;$", lambda m: f"{m.group(1)}{m.group(2)}{m.group(3) * 2};", s, flags=re.M)


def rep(m):
    lhs = m.group(2)
    if lhs.startswith("*("):
        lhs = f"({lhs})"
    return f"{m.group(1)}{lhs}{m.group(3) * 2};"


s = re.sub(r"^([ \t]*)([^;\n=]+?) = (?:\([\w ]+\) )?\(?\2 ([+-]) 1\)?;$", rep, s, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
