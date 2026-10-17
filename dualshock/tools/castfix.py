"""castfix.py - add the missing cast where a pointer is stored/passed as an integer or the other way
round, or a pointer of another type is stored/passed (warnings 'assignment makes pointer from integer',
'... integer from pointer', 'assignment from incompatible pointer type', 'passing arg N of `f' ...',
'initialization from incompatible pointer type'). A cast between 32-bit types doesn't change the
code; every edit is still checked with tools/hunkcheck.py (per-function asm compare).

The target type is taken from
    passing arg N of f : the N-th parameter of the prototype of f in scope (the function's own block
                         declaration, else the file's / header's)
    assignment         : the declared type of the left side: a local / file / header declaration of
                         a plain name, LIT(T, x), *_AS(.., T) / FIELD_AT(.., T) macros, or a struct
                         field name that has one type in every struct that has it
Only single-line statements are handled. An existing top-level pointer cast on the value is replaced,
otherwise the cast is added. Lines with a symbol of config/elemfield_syms.txt are skipped.
usage (from the decomp directory): python tools/castfix.py [--dry] files...
"""
import os, re, sys, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hunkcheck as H
import mmfix as M

INT32 = {'s32', 'u32', 'int', 'long', 'u_long', 'unsigned', 'unsigned int', 'signed int', 'uintptr_t',
         'unsigned long', 'long int', 'size_t'}
TYPEWORDS = {'int', 'char', 'long', 'short', 'void', 'unsigned', 'signed', 'const', 'volatile', 'struct',
             'union', 'enum'}


def headers_for(f):
    hs = sorted(glob.glob('include/*.h'))
    d = os.path.dirname(f)
    if os.path.exists(os.path.join(d, 'globals.h')):
        hs.append(os.path.join(d, 'globals.h').replace('\\', '/'))
    if f.startswith('src/shared/'):
        hs += sorted(glob.glob('src/stage*/globals.h'))
    return hs


_FIELDS = {}


def field_types(f):
    """field name -> set of declared types, from the struct bodies of the headers and the file"""
    key = f
    if key in _FIELDS:
        return _FIELDS[key]
    res = {}
    for p in headers_for(f) + [f]:
        depth = 0
        for ln in H.rd(p).split('\n'):
            s = re.sub(r'/\*.*?\*/', '', ln).strip()
            if re.match(r'^(typedef\s+)?(struct|union)\b[^;]*\{$', s):
                depth += 1
                continue
            if depth and s.startswith('}'):
                depth -= 1
                continue
            if depth:
                m = re.match(r'^((?:const\s+|unsigned\s+|signed\s+|struct\s+)*\w+)\s*(\**)\s*(\w+)\s*;$', s)
                if m and m.group(1) not in ('return',):
                    t = (m.group(1) + (' ' + m.group(2) if m.group(2) else '')).strip()
                    res.setdefault(m.group(3), set()).add(t)
    _FIELDS[key] = res
    return res


def decl_type(text, name):
    """type of `name` declared in one declaration line, or None"""
    s = re.sub(r'/\*.*?\*/', '', text).strip()
    s = re.sub(r'__asm__\s*\(.*?\)', '', s).strip()
    m = re.match(r'^(?:extern\s+|static\s+|register\s+)*((?:const\s+|unsigned\s+|signed\s+|struct\s+|volatile\s+)*\w+)\s*(\**)\s*%s\s*(=[^;]*)?;$'
                 % re.escape(name), s)
    if not m or m.group(1) in M.KEYWORDS:
        return None
    return (m.group(1) + (' ' + m.group(2) if m.group(2) else '')).strip()


def name_type(lines, rng, idx, name, f):
    """declared type of a plain name at line idx: the function's locals/params, then file scope, headers"""
    for fn, a, b in rng:
        if a <= idx <= b:
            for k in range(a, idx):
                t = decl_type(lines[k], name)
                if t:
                    return t
            # parameters
            hdr = ' '.join(lines[a:a + 3])
            m = re.search(r'\((.*?)\)\s*\{', hdr)
            if m:
                for prm in m.group(1).split(','):
                    t = param_type(prm, name)
                    if t:
                        return t
    for k in range(0, idx):
        if not lines[k].startswith((' ', '\t')):
            t = decl_type(lines[k], name)
            if t:
                return t
    for h in headers_for(f):
        for ln in H.rd(h).split('\n'):
            t = decl_type(ln, name)
            if t:
                return t
    return None


def param_type(prm, want=None):
    prm = prm.strip()
    m = re.match(r'^(.*?[\s\*])(\w+)$', prm)
    if m and m.group(2) not in TYPEWORDS and not re.match(r'^[su](8|16|32)$|^u_(char|short|long|int)$', m.group(2)) \
            and m.group(1).strip() not in ('', 'unsigned', 'signed', 'struct'):
        if want and m.group(2) != want:
            return None
        return re.sub(r'\s+\*', ' *', m.group(1).strip()).replace('const ', '')
    if want:
        return None
    return prm.replace('const ', '')


def split_args(s, start):
    """s[start] == '(' -> (list of (a, b) arg spans, index of the closing paren) or None"""
    depth = 0
    args = []
    a = start + 1
    i = start
    while i < len(s):
        c = s[i]
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
            if depth == 0:
                if s[a:i].strip():
                    args.append((a, i))
                return args, i
        elif c == ',' and depth == 1:
            args.append((a, i))
            a = i + 1
        elif c in '"\'':
            q = c
            i += 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == '\\' else 1
        i += 1
    return None


def proto_of(lines, rng, idx, fname, f):
    """parameter list (list of strings) of the prototype of fname in scope at line idx"""
    cands = []
    for fn, a, b in rng:
        if a <= idx <= b:
            cands = [lines[k] for k in range(a, idx) if M.is_decl(lines[k], fname)]
    if not cands:
        cands = [lines[k] for k in range(0, idx) if not lines[k].startswith((' ', '\t'))
                 and M.is_decl(lines[k], fname)]
    if not cands:
        for h in headers_for(f):
            cands += [ln for ln in H.rd(h).split('\n') if M.is_decl(ln, fname)]
    if not cands:
        return None
    d = cands[-1]
    m = re.search(r'\b%s\s*\(' % re.escape(fname), d)
    r = split_args(d, m.end() - 1)
    if not r:
        return None
    return [d[a:b].strip() for a, b in r[0]]


def is_simple(e):
    e = e.strip()
    return bool(re.match(r'^[&\*]*[\w\.]+(\s*(->|\.)\s*\w+)*$', e) or re.match(r'^[&]?\w+\([^()]*(\([^()]*\))*[^()]*\)(->\w+)*$', e)
                or re.match(r'^[&]?\w+(\[[^\[\]]*\])+$', e))


def recast(expr, T):
    """expr with the cast (T): replaces an existing top-level pointer cast, else adds one"""
    lead = expr[:len(expr) - len(expr.lstrip())]
    e = expr.strip()
    # LIT(s32, x) read as a pointer: LIT(T, x) (the same 32-bit load); LITP(u8, x) -> LITP(U, x)
    m = re.match(r'^LIT\(\s*([\w\s\*]+?)\s*,\s*(\w+)\s*\)$', e)
    if m and (is_ptr(T) or T in INT32) and (is_ptr(m.group(1)) or m.group(1) in INT32):
        return lead + 'LIT(%s, %s)' % (T, m.group(2))
    m = re.match(r'^LITP\(\s*([\w\s\*]+?)\s*,\s*(\w+)\s*\)$', e)
    if m and re.match(r'^[\w\s]+\*$', T) and T.count('*') == 1:
        return lead + 'LITP(%s, %s)' % (T[:-1].strip(), m.group(2))
    m = re.match(r'^\(\s*((?:const\s+|unsigned\s+|signed\s+|struct\s+)*\w+\s*\*+)\s*\)\s*', e)
    if m:
        rest = e[m.end():]
        # only when the cast applies to the whole rest
        if is_simple(rest) or (rest.startswith('(') and split_args(rest, 0) and split_args(rest, 0)[1] == len(rest) - 1):
            return lead + '(%s)%s' % (T, rest)
    if is_simple(e) or (e.startswith('(') and split_args(e, 0) and split_args(e, 0)[1] == len(e) - 1):
        return lead + '(%s)%s' % (T, e)
    return lead + '(%s)(%s)' % (T, e)


def lhs_type(lines, rng, idx, lhs, f):
    lhs = lhs.strip()
    m = re.match(r'^LIT\(\s*(.+?)\s*,\s*\w+\s*\)$', lhs)
    if m:
        return m.group(1)
    m = re.match(r'^\w+_AS\(.*,\s*([\w\s\*]+?)\s*\)$', lhs) or re.match(r'^FIELD_AT\(.*,\s*([\w\s\*]+?)\s*\)$', lhs)
    if m:
        return m.group(1)
    if re.match(r'^\w+$', lhs):
        return name_type(lines, rng, idx, lhs, f)
    m = re.search(r'(->|\.)\s*(\w+)$', lhs)
    if m:
        ts = field_types(f).get(m.group(2), set())
        if len(ts) == 1:
            return next(iter(ts))
    return None


def is_ptr(t):
    return t is not None and t.rstrip().endswith('*')


def fix_line(lines, rng, idx, msg, f):
    """new text of line idx or None"""
    s = lines[idx]
    m = re.match(r"passing arg (\d+) of `(\w+)'", msg)
    if m:
        n, fname = int(m.group(1)), m.group(2)
        if len(re.findall(r'\b%s\s*\(' % re.escape(fname), s)) != 1:
            return None
        prm = proto_of(lines, rng, idx, fname, f)
        if not prm or n > len(prm):
            return None
        T = param_type(prm[n - 1])
        if not T or (not is_ptr(T) and T not in INT32) or '(' in T:
            return None
        c = re.search(r'\b%s\s*\(' % re.escape(fname), s)
        r = split_args(s, c.end() - 1)
        if not r or len(r[0]) < n:
            return None
        a, b = r[0][n - 1]
        return s[:a] + recast(s[a:b], T) + s[b:]
    if msg.startswith('assignment') or msg.startswith('initialization'):
        code = s.split('/*')[0].rstrip()
        if code.count(';') != 1 or not code.endswith(';'):
            return None
        body = code[:-1]
        # one plain '=' (not ==, <=, >=, !=, +=...)
        eqs = [i for i in range(len(body)) if body[i] == '=' and body[i - 1:i] not in ('=', '!', '<', '>', '+', '-', '*', '/', '&', '|', '^', '%')
               and body[i + 1:i + 2] != '=']
        if len(eqs) != 1:
            return None
        i = eqs[0]
        lhs, rhs = body[:i], body[i + 1:]
        if msg.startswith('initialization'):
            mm = re.match(r'^\s*((?:const\s+|unsigned\s+|signed\s+|struct\s+)*\w+\s*\**)\s*\w+\s*$', lhs)
            T = mm.group(1).strip() if mm else None
        else:
            T = lhs_type(lines, rng, idx, lhs, f)
        if not T:
            return None
        T = re.sub(r'\s+\*', ' *', T)
        if 'makes integer' in msg and T not in INT32:
            return None
        if ('pointer from integer' in msg or 'incompatible pointer' in msg) and not is_ptr(T):
            return None
        return s[:i + 1] + recast(rhs, T) + s[len(body):]
    return None


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    files = [a for a in args if a != '--dry']
    for f in files:
        ok, errs, out = H.build(H.objs_for(f))
        lines = H.rd(f).split('\n')
        rng = H.func_ranges(lines)
        new = list(lines)
        done = set()
        recs = []
        for l in out.splitlines():
            m = re.match(r'^(src/[^:]+):(\d+): warning: (.*)$', l)
            if m and H.norm_src(m.group(1)) == f:
                recs.append((int(m.group(2)) - 1, m.group(3)))
        n = 0
        for idx, msg in sorted(set(recs)):
            if idx in done or M.protected(lines[idx]):
                continue
            t = fix_line(new, rng, idx, msg, f)
            if t and t != new[idx]:
                if dry:
                    print('%s:%d %s\n   - %s\n   + %s' % (f, idx + 1, msg, new[idx].strip(), t.strip()))
                new[idx] = t
                n += 1
                # several warnings on one line: fix them one by one (positions shift)
        if dry or not n:
            print(f, n, 'casts' + (' (dry)' if dry else ''))
            continue
        H.wr(f, '\n'.join(new))
        okc, rev, out2 = H.check_accept([f])
        if not okc:
            err = [l for l in out2.splitlines() if re.search(r'^\S+:\d+: (?!warning)', l)]
            print(f, 'build failed:', ' | '.join(err[:3]))
            H.wr(f, '\n'.join(lines))
            H.check_accept([f])
            continue
        for f2, o, newt, oldt in rev:
            print('  reverted [%s] %s' % (o, newt.strip()[:120]))
        print(f, n, 'casts, warnings now', sum(H.load_wbase().get(f, {}).values()))


if __name__ == '__main__':
    main()
