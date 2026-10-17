"""stmtexpr.py <in.c> <out.c>: wrap a `bit * 2` multiplier in a GNU statement expression.

The original computes `v - v * (bit * 2)` (stage1 func_8010F6DC and its twins, the
func_8010F7FC family) as `andi; move; sll 1; mult`: the `move` is synth_mult's copy for `* 2`,
so it IS a multiply, yet GCC's fold() did not re-associate it into `(v * 2) * bit`. Every cast
around `bit * 2` is either stripped by split_tree() (same mode) or adds a mask (narrower). A
statement expression is opaque to fold() and costs no code: `v * ({ bit * 2; })` matches.

Rewrites m2c's  `(u8) ((((u32) (E & M) >> N) & 0xFF) * 2)`
and the hand form `(u16) ((u8) ((E & M) >> N) * 2)`
into            `({ (u8) ((u8) (E & M) >> N) * 2; })`. For trybatch."""
import re
import sys


def close_paren(s, i):
    """Index of the ')' matching the '(' at s[i]."""
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            depth += 1
        elif s[j] == ")":
            depth -= 1
            if depth == 0:
                return j
    return -1


def split_mask(inner):
    """'E & M' -> (E, M) when M is a constant at depth 0."""
    m = re.search(r" & (0x[0-9A-Fa-f]+|\d+)$", inner)
    return (inner[:m.start()], m.group(1)) if m else None


FORMS = [
    # (prefix up to the '(' opening "E & M", suffix after it)
    ("(u8) ((((u32) ", re.compile(r" >> (\d+)\) & 0xFF\) \* 2\)")),
    ("(u16) ((u8) (", re.compile(r" >> (\d+)\) \* 2\)")),
]


def rewrite(s):
    for prefix, tail in FORMS:
        pos = 0
        while True:
            i = s.find(prefix, pos)
            if i < 0:
                break
            o = i + len(prefix)
            c = close_paren(s, o)
            m = tail.match(s, c + 1) if c > 0 else None
            em = split_mask(s[o + 1:c]) if m else None
            if not em:
                pos = i + 1
                continue
            new = f"({{ (u8) ((u8) ({em[0]} & {em[1]}) >> {m.group(1)}) * 2; }})"
            s = s[:i] + new + s[m.end():]
            pos = i + len(new)
    return s


if __name__ == "__main__":
    src = open(sys.argv[1]).read()
    open(sys.argv[2], "w", newline="\n").write(rewrite(src))
