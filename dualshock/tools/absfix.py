"""absfix.py <in.c> <out.c>: m2c's abs() blocks -> __builtin_abs, inlined into the single later use.
Forms: `v = E; if (temp_x < 0) { v = -v; }` and `if ((v = E) < 0) { v = -v; }`."""
import re, sys

s = open(sys.argv[1]).read()
pats = [r"^([ \t]*)(var_\w+) = ([^\n;]+);\n[ \t]*if \((?:temp_\w+|\2) < 0\) \{\n[ \t]*\2 = -\2;\n[ \t]*\}\n",
        r"^([ \t]*)if \(\((var_\w+) = (.+)\) < 0\) \{\n[ \t]*\2 = -\2;\n[ \t]*\}\n"]
changed = True
while changed:
    changed = False
    for p in pats:
        m = re.search(p, s, re.M)
        if not m:
            continue
        v, e = m.group(2), m.group(3)
        rest = s[m.end():]
        if len(re.findall(rf"\b{v}\b", rest)) == 1:
            rest = re.sub(rf"\b{v}\b", lambda _: f"__builtin_abs({e})", rest)
            s = s[:m.start()] + rest
        else:
            s = s[:m.start()] + f"{m.group(1)}{v} = __builtin_abs({e});\n" + rest
        changed = True
# drop now-unused declarations of the m2c temps
for d in re.findall(r"^    [\w ]+? (temp_\w+|var_\w+);\n", s, re.M):
    if len(re.findall(rf"\b{d}\b", s)) == 1:
        s = re.sub(rf"^    [\w ]+? {d};\n", "", s, flags=re.M)
open(sys.argv[2], "w", newline="\n").write(s)
