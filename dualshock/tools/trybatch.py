"""trybatch.py "<transform cmd>" <target> [...]: run `<transform cmd> in.c out.c` on every
unmatched draft of the targets; if the output differs, measure it and store improvements
(status match / diff:N). The transform is any script taking in/out paths (extra args allowed)."""
import json, os, shlex, subprocess, sys, shutil
sys.path.insert(0, 'tools')
import refine

cmd = shlex.split(sys.argv[1])
for t in sys.argv[2:]:
    ctx = refine.Ctx(t)
    rp = f'build/auto/{t}/results.json'
    rr = json.load(open(rp))
    os.makedirs('build/trybatch', exist_ok=True)
    n_better = n_match = 0
    # only functions still waiting as INCLUDE_ASM (results.json keeps stale entries of
    # functions that were matched by hand and integrated)
    import glob, re
    pending = set()
    for cf in glob.glob(f'src/{t}/*.c'):
        pending |= set(re.findall(r'INCLUDE_ASM\("[^"]+", (\w+)\)', open(cf).read()))
    for n, st in sorted(rr.items()):
        if st.startswith('match') or n not in pending:
            continue
        f = f'build/auto/{t}/c/{n}.c'
        if not os.path.exists(f):
            continue
        out = f'build/trybatch/{t}_{n}.c'
        r = subprocess.run(cmd[:1] + ['-I'] + cmd[1:2] + [f, out] + cmd[2:], capture_output=True, text=True)
        if r.returncode or not os.path.exists(out):
            continue
        c, orig = open(out).read(), open(f).read()
        if c == orig:
            continue
        d = refine.distance(ctx, n, c, 'try')
        shutil.rmtree(os.path.join(ctx.dir, 'work', n, 'try'), ignore_errors=True)
        old = int(st.split(':')[1].split(',')[0]) if st.startswith('diff:') else 10 ** 6
        if d is not None and d < old:
            n_better += 1
            n_match += d == 0
            print(f'{t} {n}: {st} -> {d}', flush=True)
            open(f, 'w', newline='\n').write(c)
            rr = json.load(open(rp))
            rr[n] = 'match' if d == 0 else f'diff:{d}'
            json.dump(rr, open(rp, 'w'), indent=1, sort_keys=True)
    print(f'{t}: {n_better} improved, {n_match} matched', flush=True)
