"""prefield.py <in.c> <out.c> [--all]: `F = F + 1;` on a field whose address is computed
(`M2C_FIELD((D_800CAB14 + 0x16C), u8 *, 3)`, Entity sub-struct, nested pointers) is Capcom's `++F;`
- GCC -O0 expands the pre-increment the long way and leaves the target address computed twice
(`addiu v1,a0,364; move v0,v1` + again for the read) where `F = F + 1` computes it once
(stage2 func_8011B95C, func_8011920C). Also `- 1` -> `--F;`. Without --all only the first
statement changes. For trybatch."""
import re
import sys


def norm(t):
    return re.sub(r"\bM2C_[SP]?FIELD\(", "M2C_FIELD(", re.sub(r"\b[su](?:8|16|32) \*", "T *", t)).replace(" ", "")


s = open(sys.argv[1]).read()
do_all = "--all" in sys.argv[3:]
F = r"M2C_\w*FIELD\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\)"
pat = re.compile(r"^(?P<i>[ \t]*)(?P<a>" + F + r") = (?:\([\w ]+\) )?\(?(?P<b>" + F + r") (?P<op>[-+]) 1\)?;$", re.M)
done = False


def rep(m):
    global done
    if (done and not do_all) or norm(m.group("a")) != norm(m.group("b")) or "(" not in m.group("a")[10:]:
        return m.group(0)
    done = True
    return f"{m.group('i')}{'++' if m.group('op') == '+' else '--'}{m.group('a')};"


s = pat.sub(rep, s)
open(sys.argv[2], "w", newline="\n").write(s)
