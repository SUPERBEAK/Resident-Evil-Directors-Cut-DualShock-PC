"""fakearg.py <in.c> <out.c>: m2c sometimes gives a function parameters that are really scratch
registers (`void f(u16 arg0)` with `arg0 = p->x76; p->x76 = (u16) (arg0 - 8);` and stale
`D_801FEA18(arg0)` call arguments). When the original asm never saves an argument register in
its prologue (-O0 always does: `sw $aN, off($fp)` / `move $sN, $aN`), make the function
`(void)`, fold `argN = F; F = (T) (argN op e);` into `F op= e;`, `F = (T) (argN op e); argN = F;`
likewise, and drop argN from call argument lists (stage4 func_80121A2C). Needs the asm, so it
finds it from the function name in the draft. For trybatch."""
import glob
import re
import sys

s = open(sys.argv[1]).read()
m = re.search(r"^\w[\w *]*?\b(func_[0-9A-F]{8})\(([^)]*)\) \{", s, re.M)
if m and m.group(2).strip() not in ("", "void"):
    name = m.group(1)
    asm = glob.glob(f"asm/*/nonmatchings/*/{name}.s")
    head = open(asm[0]).read().split("\n")[:16] if asm else []
    saves = any(re.search(r"\b(?:sw|sh|sb)\s+\$a[0-3],|\baddu\s+\$s\d, \$a[0-3], \$zero|\bmove\s+\$s\d, \$a[0-3]", l) for l in head)
    if asm and not saves:
        s = s[:m.start(2)] + "void" + s[m.end(2):]
        F = r"(?:M2C_\w*FIELD\((?:[^()]|\([^()]*\))*\)|D_[0-9A-F]{8})"
        s = re.sub(r"\n(?P<i>[ \t]*)(?P<t>arg\d) = (?:\([\w ]+\) )?(?P<f>" + F + r");\n(?P=i)(?P<f2>" + F +
                   r") = (?:\([\w ]+\) )?\((?P=t) (?P<op>[-+|&^]) (?P<e>[^;]+)\);\n",
                   lambda g: f"\n{g.group('i')}{g.group('f2')} {g.group('op')}= {g.group('e')};\n", s)
        s = re.sub(r"\n(?P<i>[ \t]*)(?P<f2>" + F + r") = (?:\([\w ]+\) )?\((?P<t>arg\d) (?P<op>[-+|&^]) (?P<e>[^;]+)\);\n"
                   r"(?P=i)(?P=t) = (?:\([\w ]+\) )?" + F + r";\n",
                   lambda g: f"\n{g.group('i')}{g.group('f2')} {g.group('op')}= {g.group('e')};\n", s)
        s = re.sub(r"\(\s*(?:\([\w ]+\) )?arg\d(?:, (?:\([\w ]+\) )?arg\d)*\s*\)", "()", s)
open(sys.argv[2], "w", newline="\n").write(s)
