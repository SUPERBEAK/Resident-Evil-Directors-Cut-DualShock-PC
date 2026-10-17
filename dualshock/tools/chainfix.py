"""chainfix.py <in.c> <out.c>: m2c splits Capcom's chained assignment `a = b = c = v;`:
 1. into `b = c = v;` + `a = v;` on the next line (the original stores the same register three
    times; the split version reloads v) - stage6 func_80115590, stage4 func_80115718;
 2. through a temp: `t = X; c = (cast) t; b = (cast) t; a = t;` - stage3 func_80129E58.
Merge them back into one chain (the first store in the source order is the innermost).
For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"^([ \t]*)(\S[^;\n]*? = [^;\n]*? = (\w+));\n\1([^;\n=]+?) = \3;\n",
           lambda m: f"{m.group(1)}{m.group(4)} = {m.group(2)};\n", s, flags=re.M)

pat = re.compile(r"^([ \t]*)(temp_\w+) = ([^;\n]+);\n((?:\1[^;\n=]+? = (?:\([\w ]+\*?\) )?\2;\n){2,})", re.M)
while True:
    m = pat.search(s)
    if not m:
        break
    t = m.group(2)
    lhs = [re.match(r"[ \t]*(.+?) = ", l).group(1) for l in m.group(4).splitlines()]
    if len(re.findall(rf"\b{t}\b", s)) != 2 + len(lhs):       # declaration + assignment + uses
        break
    chain = " = ".join(reversed(lhs))
    s = s[:m.start()] + f"{m.group(1)}{chain} = {m.group(3)};\n" + s[m.end():]
    s = re.sub(rf"\n[ \t]*[\w ]+\*?\s*\b{t};(?=\n)", "", s, count=1)
open(sys.argv[2], "w", newline="\n").write(s)
