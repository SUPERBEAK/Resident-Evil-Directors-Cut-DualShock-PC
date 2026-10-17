"""hunkcheck.py - keep only the source edits that leave the generated assembly unchanged.

Compares the edited sources (working tree) with the last accepted copy (../temp/warn/accepted,
started from .baseline; updated with the checked files when they pass), rebuilds the affected objects and compares
their assembly per function with a baseline copy of build/**/*.ms.s made from the accepted sources
(default ../temp/warn/base_asm; make it with `hunkcheck.py --snapshot` after a clean build of accepted sources).
Every diff hunk inside a function whose assembly changed is put back to the accepted text; when the
change can't be attributed to a function, the file-scope hunks of that file are put back.
Repeats until the assembly matches. Then the edits of any function that now has more warnings than
before (per-function counts in ../temp/warn/warnbase.json, started with --initwarn LOG from a full
`make -O` log and updated on every accepted check) are put back too.
Prints the reverted hunks and the warning counts per file.

usage (from the decomp directory):
    python tools/hunkcheck.py [--base DIR] [--snapshot] [--dry] [files...]
    (default files: every src/include file that differs from the accepted copy)
"""
import os, re, sys, glob, subprocess, difflib, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BUILDENV = os.path.join(ROOT, 'tools', 'buildenv.sh')
BASE = os.path.join(ROOT, 'temp', 'warn', 'base_asm')


def rd(p):
    return open(p, 'rb').read().decode('utf-8')


def wr(p, s):
    open(p, 'wb').write(s.encode('utf-8'))


REF = os.path.join(ROOT, 'temp', 'warn', 'accepted')


def head_text(p):
    """the last accepted text of p (REF/p), None for a new file"""
    q = os.path.join(REF, p)
    return rd(q) if os.path.exists(q) else None


def accept(p):
    q = os.path.join(REF, p)
    os.makedirs(os.path.dirname(q), exist_ok=True)
    open(q, 'wb').write(open(p, 'rb').read())


def changed_files():
    out = []
    for top in ('src', 'include'):
        for d, _, fs in os.walk(top):
            for f in fs:
                p = os.path.join(d, f).replace(os.sep, '/')
                if head_text(p) != rd(p):
                    out.append(p)
    return out


def all_objs():
    return sorted('build/' + p.replace('\\', '/')[:-2] + '.o' for p in glob.glob('src/*/*.c')
                  if not p.replace('\\', '/').startswith('src/shared/'))


def objs_for(src):
    src = src.replace('\\', '/')
    if src.startswith('include/'):
        return all_objs()
    if src.startswith('src/shared/'):
        base = os.path.basename(src)
        out = []
        for w in sorted(glob.glob('src/stage*/' + base)):
            w = w.replace('\\', '/')
            if '../shared/' + base in rd(w):
                out.append('build/' + w[:-2] + '.o')
        return out
    if src.endswith('.h'):
        d = os.path.dirname(src)
        return ['build/' + c.replace('\\', '/')[:-2] + '.o' for c in sorted(glob.glob(d + '/*.c'))]
    return ['build/' + src[:-2] + '.o']


def build(objs):
    for o in objs:
        for ext in ('.o', '.ms.s', '.s', '.i'):
            p = o[:-2] + ext
            for _ in range(50):
                try:
                    if os.path.exists(p):
                        os.remove(p)
                    break
                except PermissionError:
                    time.sleep(0.2)
    r = subprocess.run(['sh', BUILDENV, 'make -j8 -O ' + ' '.join(objs)], capture_output=True)
    out = (r.stdout + r.stderr).decode('utf-8', 'replace')
    errs = [l for l in out.splitlines()
            if ': error' in l or 'Error ' in l or 'parse error' in l
            or re.search(r'^\S+:\d+: (?!warning)', l)]
    return r.returncode == 0 and not errs, errs, out


def _norm(lines):
    labels = {}
    out = []
    for ln in lines:
        def rep(m):
            k = m.group(0)
            if k not in labels:
                labels[k] = '$L#%d' % len(labels)
            return labels[k]
        out.append(re.sub(r'\$L\d+|\$LC\d+', rep, ln))
    return '\n'.join(out)


def funcs_of(path):
    funcs, rest, cur, buf = {}, [], None, []
    for ln in rd(path).splitlines():
        m = re.match(r'^\s*\.ent\s+(\w+)', ln)
        if m:
            cur, buf = m.group(1), []
            continue
        m = re.match(r'^\s*\.end\s+(\w+)', ln)
        if m and cur:
            funcs[cur] = _norm(buf)
            cur = None
            continue
        (buf if cur else rest).append(ln)
    funcs['<rest>'] = _norm(rest)
    return funcs


def changed_funcs(objs, base=BASE):
    ch = set()
    for o in objs:
        rel = o[len('build/'):-2] + '.ms.s'
        b = funcs_of(os.path.join(base, rel))
        n = funcs_of(o[:-2] + '.ms.s')
        for f in set(b) | set(n):
            if b.get(f) != n.get(f):
                ch.add(f)
    return ch


def func_ranges(lines):
    """[(name, first, last)] 0-based inclusive line ranges of function definitions."""
    out = []
    i = 0
    while i < len(lines):
        ln = lines[i]
        m = re.match(r'^(?!typedef|struct|union|enum|extern|#|/|\s)[^=;]*?\b(\w+)\s*\(', ln)
        if m and not ln.rstrip().endswith(';'):
            j = i
            while j < len(lines) and '{' not in lines[j] and not lines[j].rstrip().endswith(';'):
                j += 1
            if j < len(lines) and '{' in lines[j]:
                k = j
                while k < len(lines) and not re.match(r'^\}', lines[k]):
                    k += 1
                out.append((m.group(1), i, k))
                i = k + 1
                continue
        i += 1
    return out


def hunks(old, new):
    sm = difflib.SequenceMatcher(None, old, new, autojunk=False)
    return [(i1, i2, j1, j2) for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != 'equal']


def owner(ranges, j1, j2):
    for name, a, b in ranges:
        if a <= j1 and max(j1, j2 - 1) <= b:
            return name
    return None


def norm_src(f):
    """src/stage7/../shared/yawn.c -> src/shared/yawn.c"""
    return re.sub(r'src/[^/]+/\.\./', 'src/', f)


def func_warnings(out):
    """{normalized file: {function or None: count}} of the warnings in a `make -O` log
    (the 'previous declaration' notes are not counted)."""
    res, cur = {}, {}
    for l in out.splitlines():
        m = re.match(r"^(src/[^:]+): In function `([^']+)':", l)
        if m:
            cur[m.group(1)] = m.group(2)
            continue
        m = re.match(r'^(src/[^:]+): At top level:', l)
        if m:
            cur[m.group(1)] = None
            continue
        m = re.match(r'^(src/[^:]+|include/[^:]+):\d+: warning: (.*)$', l)
        if m and not m.group(2).startswith('previous '):
            f = norm_src(m.group(1))
            d = res.setdefault(f, {})
            k = cur.get(m.group(1)) or '<top>'
            d[k] = d.get(k, 0) + 1
    return res


WBASE = os.path.join(ROOT, 'temp', 'warn', 'warnbase.json')


def load_wbase():
    import json
    if os.path.exists(WBASE):
        return json.load(open(WBASE))
    return {}


def save_wbase(w):
    import json
    json.dump(w, open(WBASE, 'w'), indent=0, sort_keys=True)


def count_warnings(out):
    c = {}
    for l in out.splitlines():
        m = re.match(r'^(src/[^:]+|include/[^:]+):\d+: warning: (.*)$', l)
        if m and not m.group(2).startswith('previous '):
            c[m.group(1)] = c.get(m.group(1), 0) + 1
    return c


def check(files, base=BASE, dry=False):
    """returns (ok, reverted hunks, build output)"""
    objs = sorted({o for f in files for o in objs_for(f)})
    reverted = []
    for it in range(8):
        ok, errs, out = build(objs)
        if not ok:
            return False, reverted, out
        ch = changed_funcs(objs, base)
        if not ch:
            # also put back the edits of functions that now have more warnings than before
            wb = load_wbase()
            fw = func_warnings(out)
            up = set()
            for f in files:
                if not f.endswith('.c'):
                    continue
                now = fw.get(f, {})
                was = wb.get(f, {})
                for k, n in now.items():
                    if k != '<top>' and n > was.get(k, 0):
                        up.add((f, k))
            if not up or dry:
                return True, reverted, out
            for f in sorted({f for f, k in up}):
                old = (head_text(f) or '').split('\n')
                new = rd(f).split('\n')
                rng = func_ranges(new)
                names = {k for g, k in up if g == f}
                todo = [h for h in hunks(old, new) if owner(rng, h[2], h[3]) in names]
                for (i1, i2, j1, j2) in sorted(todo, key=lambda t: -t[2]):
                    reverted.append((f, owner(rng, j1, j2) + ' (more warnings)', '\n'.join(new[j1:j2]),
                                     '\n'.join(old[i1:i2])))
                    new[j1:j2] = old[i1:i2]
                wr(f, '\n'.join(new))
                if not todo:
                    print('more warnings in', f, sorted(names), '(no hunk inside; kept)')
                    up = {u for u in up if u[0] != f}
            if not up:
                return True, reverted, out
            continue
        if dry:
            print('changed functions:', sorted(ch))
            return False, reverted, out
        progress = False
        for f in files:
            if not f.endswith('.c'):
                continue
            old = (head_text(f) or '').split('\n')
            new = rd(f).split('\n')
            rng = func_ranges(new)
            hs = hunks(old, new)
            todo = [(owner(rng, h[2], h[3]), h) for h in hs if owner(rng, h[2], h[3]) in ch]
            if not todo and (ch - {o for o, a, b in rng}):
                todo = [(None, h) for h in hs if owner(rng, h[2], h[3]) is None]
            for o, (i1, i2, j1, j2) in sorted(todo, key=lambda t: -t[1][2]):
                reverted.append((f, o, '\n'.join(new[j1:j2]), '\n'.join(old[i1:i2])))
                new[j1:j2] = old[i1:i2]
                progress = True
            if todo:
                wr(f, '\n'.join(new))
        if not progress:
            print('UNRESOLVED changed functions:', sorted(ch))
            return False, reverted, out
    return False, reverted, out


def check_accept(files, base=BASE, dry=False):
    """check(); when it passes, the files become the accepted text and their warning counts the base"""
    ok, rev, out = check(files, base, dry)
    if ok and not dry:
        wb = load_wbase()
        fw = func_warnings(out)
        for f in files:
            accept(f)
            if f.endswith('.c'):
                wb[f] = {k: v for k, v in fw.get(f, {}).items()}
        save_wbase(wb)
    return ok, rev, out


def main():
    args = sys.argv[1:]
    base = BASE
    dry = False
    if '--base' in args:
        i = args.index('--base')
        base = args[i + 1]
        del args[i:i + 2]
    if '--dry' in args:
        args.remove('--dry')
        dry = True
    if '--initwarn' in args:     # per-function warning counts from a full `make -O` log
        i = args.index('--initwarn')
        fw = func_warnings(rd(args[i + 1]))
        save_wbase({f: {k: v for k, v in d.items()} for f, d in fw.items()})
        return
    if '--snapshot' in args:
        import shutil
        for p in glob.glob('build/src/*/*.ms.s'):
            d = os.path.join(base, os.path.relpath(p, 'build'))
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(p, d)
        return
    files = args or changed_files()
    if not files:
        print('nothing changed')
        return
    ok, rev, out = check_accept(files, base, dry)
    for f, o, newt, oldt in rev:
        print('REVERTED %s [%s]\n  - %s\n  + %s' % (f, o, newt.replace('\n', '\n    '),
                                                   oldt.replace('\n', '\n    ')))
    c = count_warnings(out)
    print('warnings:', sum(c.values()), ' '.join('%s=%d' % kv for kv in sorted(c.items())))
    print('RESULT', 'OK' if ok else 'FAIL')
    if not ok:
        print('\n'.join([l for l in out.splitlines() if 'rror' in l][:20]))


if __name__ == '__main__':
    main()
