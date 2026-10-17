"""aliasren.py - give a function-local declaration that has another type than an earlier one in the
same file its own name (an __asm__ alias of the same symbol), so the two no longer conflict
('type mismatch with previous external decl'). The code is unchanged: both names are the same symbol.

usage (from the decomp directory):
    python tools/aliasren.py FILE LINE OLD NEW [FILE LINE OLD NEW ...]
LINE (1-based) is the declaration of OLD inside a function; OLD is renamed to NEW in that function
only, and the declaration gets __asm__("<real symbol>") if it has none.
Check the result with tools/hunkcheck.py.
"""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hunkcheck as H


def rename(f, line, old, new):
    lines = H.rd(f).split('\n')
    i = line - 1
    if not re.search(r'\b%s\b' % re.escape(old), lines[i]):
        raise SystemExit('%s:%d does not mention %s' % (f, line, old))
    for name, a, b in H.func_ranges(lines):
        if a <= i <= b:
            break
    else:
        raise SystemExit('%s:%d is not inside a function' % (f, line))
    d = lines[i]
    if '__asm__' not in d:
        d = re.sub(r';\s*$', ' __asm__("%s");' % old, d.rstrip())
    lines[i] = d
    for k in range(a, b + 1):
        lines[k] = re.sub(r'\b%s\b' % re.escape(old), new, lines[k])
    # the __asm__ string names the real symbol: keep it
    lines[i] = re.sub(r'__asm__\("%s"\)' % re.escape(new), '__asm__("%s")' % old, lines[i])
    H.wr(f, '\n'.join(lines))


if __name__ == '__main__':
    a = sys.argv[1:]
    for k in range(0, len(a), 4):
        rename(a[k], int(a[k + 1]), a[k + 2], a[k + 3])
