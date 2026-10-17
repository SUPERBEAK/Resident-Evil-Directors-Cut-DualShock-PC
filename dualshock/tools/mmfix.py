"""mmfix.py - remove 'type mismatch with previous external/implicit decl' warnings where the
assembly allows it (every candidate is checked with tools/hunkcheck.py: per-function asm compare
against the baseline; edits of functions whose code changes or that get more warnings are put back).

A mismatch pair is the line L that is warned about and the earlier declaration P it conflicts with.
Each of them is a definition (def), a block-scope declaration (decl) or a call without a
declaration in scope (use = an implicit `int f()`). Candidates, in this order:
    L=def,  P=decl : every declaration of the symbol in the file := the definition's prototype
    L=decl, P=decl : all := the definition's prototype (if the file defines it); L := P; P := L
    L=use,  P=decl : P's declaration is added to L's function (the call gets that type)
    L=decl, P=use  : L's declaration is added to P's function
    L=def,  P=use  : the definition's prototype is added to P's function
    P in a header  : L is dropped; L := P
Per file and round the first untried candidate of every pair is tried together; if that batch
doesn't build, each one alone. Tried candidates are remembered in ../temp/warn/mmfix_tried.json.
Lines holding a symbol of config/elemfield_syms.txt (element aliases such as g_PacketArea_1) are never touched.
usage (from the decomp directory): python tools/mmfix.py [files...]   (default: every file with one)
"""
import os, re, sys, json, difflib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hunkcheck as H

KEYWORDS = {'return', 'else', 'do', 'case', 'goto', 'if', 'while', 'for', 'switch', 'sizeof'}
STATE = os.path.join(H.ROOT, 'temp', 'warn', 'mmfix_tried.json')
# lines with these symbols are being changed by someone else: never touch them
_EF = os.path.join(H.ROOT, 'dualshock', 'config', 'elemfield_syms.txt')
PROTECTED = set(open(_EF).read().split()) if os.path.exists(_EF) else set()


def protected(text):
    return any(w in PROTECTED for w in re.findall(r'\w+', text))


def is_def(line, sym):
    return (re.match(r'^(?!static|extern|typedef|#|\s)[\w\s\*]*?\b%s\s*\(' % re.escape(sym), line)
            and not line.rstrip().endswith(';'))


def is_decl(line, sym):
    s = line.strip()
    if not s.endswith(';') or '=' in s.split('__asm__')[0]:
        return False
    m = re.match(r'^((?:extern|const|unsigned|signed|struct|union|volatile)\s+)*(\w+)[\s\*]+%s\b'
                 % re.escape(sym), s)
    return bool(m) and m.group(2) not in KEYWORDS


def def_proto(lines, i):
    """prototype text of the definition starting at line i (None for a static one)"""
    hdr = []
    j = i
    while True:
        hdr.append(lines[j].strip())
        if '{' in lines[j]:
            break
        j += 1
    t = ' '.join(hdr)
    t = t[:t.index('{')].strip()
    if t.startswith('static'):
        return None
    return t + ';'


def find_def(lines, sym):
    for i, l in enumerate(lines):
        if is_def(l, sym):
            return i
    return None


def indent_of(l):
    return l[:len(l) - len(l.lstrip())]


def func_open(lines, rng, line):
    """index of the line with the opening brace of the function that contains `line`"""
    for name, a, b in rng:
        if a <= line <= b:
            j = a
            while '{' not in lines[j]:
                j += 1
            return j
    return None


def has_decl_in_func(lines, rng, line, sym):
    for name, a, b in rng:
        if a <= line <= b:
            return any(is_decl(lines[k], sym) for k in range(a, b + 1))
    return False


def pairs(out):
    """mismatch pairs of a make -O log: (file, L, Pfile, P, sym), 1-based lines"""
    res = []
    ls = out.splitlines()
    for i, l in enumerate(ls):
        m = re.match(r'^(src/[^:]+):(\d+): warning: type mismatch with previous', l)
        if not m or i + 1 >= len(ls):
            continue
        n = re.match(r"^(src/[^:]+|include/[^:]+):(\d+): warning: previous \w+ decl\w* of `([^']+)'", ls[i + 1])
        if not n:
            continue
        res.append((H.norm_src(m.group(1)), int(m.group(2)), H.norm_src(n.group(1)), int(n.group(2)),
                    n.group(3)))
    return sorted(set(res))


def kr(text, sym):
    """`T f(args) __asm__(..);` -> `T f() __asm__(..);` (no parameter list: calls aren't converted)"""
    m = re.search(r'\b%s\s*\(' % re.escape(sym), text)
    if not m:
        return None
    i = m.end()
    depth = 1
    while i < len(text) and depth:
        depth += {'(': 1, ')': -1}.get(text[i], 0)
        i += 1
    r = text[:m.end()] + text[i - 1:]
    return r if r != text else None


def candidates(lines, f, L, pf, P, sym):
    out = _candidates(lines, f, L, pf, P, sym)
    # K&R variants of every candidate (for calls with extra / fake arguments)
    more = []
    for name, ops in out:
        k = [(op[0], op[1], kr(op[2], sym)) if op[0] in ('set', 'ins') else op for op in ops]
        if all(len(op) < 3 or op[2] for op in k) and k != ops and name != 'L:=P(hdr)':
            more.append((name + '-kr', k))
    return out + more


def _candidates(lines, f, L, pf, P, sym):
    """[(name, ops)]; ops: ('set', idx, text) / ('ins', idx of the function's '{', text) / ('del', idx)"""
    rng = H.func_ranges(lines)
    l = lines[L - 1]
    out = []
    if pf != f:
        if is_decl(l, sym):
            hl = H.rd(pf).split('\n')[P - 1]
            out.append(('drop-L', [('del', L - 1)]))
            out.append(('L:=P(hdr)', [('set', L - 1, indent_of(l) + hl.strip())]))
        return out
    p = lines[P - 1]
    kl = 'def' if is_def(l, sym) else 'decl' if is_decl(l, sym) else 'use'
    kp = 'def' if is_def(p, sym) else 'decl' if is_decl(p, sym) else 'use'
    d = find_def(lines, sym)
    proto = def_proto(lines, d) if d is not None else None
    alldecl = [i for i, x in enumerate(lines) if is_decl(x, sym) and H.owner(rng, i, i + 1) is not None]
    if kl == 'def' and kp == 'decl' and proto:
        out.append(('all:=def', [('set', i, indent_of(lines[i]) + proto) for i in alldecl
                                 if lines[i].strip() != proto]))
    elif kl == 'decl' and kp == 'decl':
        if proto and '__asm__' not in l:
            out.append(('all:=def', [('set', i, indent_of(lines[i]) + proto) for i in alldecl
                                     if lines[i].strip() != proto]))
        out.append(('L:=P', [('set', L - 1, indent_of(l) + p.strip())]))
        out.append(('P:=L', [('set', P - 1, indent_of(p) + l.strip())]))
    elif kl == 'use' and kp in ('decl', 'def'):
        t = p.strip() if kp == 'decl' else proto
        fo = func_open(lines, rng, L - 1)
        if t and fo is not None and not has_decl_in_func(lines, rng, L - 1, sym):
            out.append(('ins-P-in-L', [('ins', fo, t)]))
    elif kl in ('decl', 'def') and kp == 'use':
        t = l.strip() if kl == 'decl' else proto
        fo = func_open(lines, rng, P - 1)
        if t and fo is not None and not has_decl_in_func(lines, rng, P - 1, sym):
            out.append(('ins-L-in-P', [('ins', fo, t)]))
    return out


def apply_ops(lines, ops):
    sets, ins, dels = {}, {}, set()
    for op in ops:
        if op[0] == 'set':
            if op[1] in sets or op[1] in dels:
                return None
            sets[op[1]] = op[2]
        elif op[0] == 'del':
            if op[1] in sets:
                return None
            dels.add(op[1])
        else:
            if op[2] not in ins.get(op[1], []):
                ins.setdefault(op[1], []).append(op[2])
    new = []
    for i, x in enumerate(lines):
        if i in dels:
            continue
        new.append(sets.get(i, x))
        for t in ins.get(i, []):
            new.append('    ' + t)
    return new


def remap(ops, old, new):
    """ops on `old` -> the same ops on `new` (None if a target line changed)"""
    m = {}
    sm = difflib.SequenceMatcher(None, old, new, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            for k in range(i2 - i1):
                m[i1 + k] = j1 + k
    out = []
    for op in ops:
        if op[1] not in m:
            return None
        out.append((op[0], m[op[1]]) + tuple(op[2:]))
    return out


def try_ops(f, ops, label):
    cur = H.rd(f).split('\n')
    new = apply_ops(cur, ops)
    if new is None:
        return False
    prev = H.head_text(f)
    before = sum(H.load_wbase().get(f, {}).values())
    H.wr(f, '\n'.join(new))
    ok, rev, out = H.check_accept([f])
    if not ok:
        err = [l for l in out.splitlines() if re.search(r'^\S+:\d+: (?!warning)', l)]
        print('  build failed (%s): %s' % (label[:150], ' | '.join(err[:2])[:200]))
        H.wr(f, prev)
        H.check_accept([f])
        return False
    after = sum(H.load_wbase().get(f, {}).values())
    if after >= before:
        print('  no gain (%s): %d -> %d, restored' % (label[:150], before, after))
        H.wr(f, prev)
        H.check_accept([f])
        return False
    for f2, o, newt, oldt in rev:
        print('  reverted [%s] %s' % (o, newt.strip().replace('\n', ' / ')[:110]))
    print('  tried', label)
    return True


def main():
    tried = json.load(open(STATE)) if os.path.exists(STATE) else {}
    files = sys.argv[1:]
    for rnd in range(12):
        if not files:
            ok, errs, out = H.build(H.all_objs())
            todo = sorted({p[0] for p in pairs(out)})
        else:
            todo = files
        any_applied = False
        for f in todo:
            ok, errs, out = H.build(H.objs_for(f))
            ps = [p for p in pairs(out) if p[0] == f]
            if not ps:
                continue
            lines = H.rd(f).split('\n')
            batch, used = [], set()
            for (ff, L, pf, P, sym) in ps:
                key = '%s|%s|%s|%s' % (f, sym, lines[L - 1].strip(), H.rd(pf).split('\n')[P - 1].strip())
                for name, ops in candidates(lines, f, L, pf, P, sym):
                    k2 = key + '|' + name
                    if k2 in tried or not ops:
                        continue
                    if any(protected(lines[op[1]]) or (len(op) > 2 and protected(op[2])) for op in ops):
                        continue
                    touched = {op[1] for op in ops}
                    if touched & used:
                        break       # another pair edits the same lines this round; next round
                    tried[k2] = 1
                    used |= touched
                    batch.append(('%s:%d %s %s' % (f, L, sym, name), ops))
                    break
            json.dump(tried, open(STATE, 'w'), indent=0)
            if not batch:
                continue
            print(f, '-', len(batch), 'candidates')
            if len(batch) > 1 and try_ops(f, [op for n, ops in batch for op in ops], '%d together' % len(batch)):
                any_applied = True
                continue
            for name, ops in batch:
                ops2 = remap(ops, lines, H.rd(f).split('\n'))
                if ops2 is not None and try_ops(f, ops2, name):
                    any_applied = True
        if not any_applied:
            break


if __name__ == '__main__':
    main()
