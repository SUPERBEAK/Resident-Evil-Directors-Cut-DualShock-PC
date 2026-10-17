"""Why does a function that matches alone fail inside the full file?
Inserts the stored draft like integrate.py does, builds the target, prints funcdiff for it,
and ALWAYS restores the C file. Usage (from dualshock/, via buildenv.sh):
    python tools/intdiag.py <target> <func>"""
import os, re, subprocess, sys
sys.path.insert(0, 'tools')
import integrate as ig

t_name, fn = sys.argv[1], sys.argv[2]
t = ig.Target(t_name)
text = open(t.cfile).read()
backup = text
try:
    defined = set(re.findall(r'^\S[^\n]*?\b(func_[0-9A-F]{8}|main)\s*\([^;\n]*\{', text, re.M))
    ig.FILE_PROTOS.clear()
    ig.FILE_PROTOS.update(re.findall(r"^[A-Za-z_][\w \*]*?\b(func_[0-9A-F]{8})\s*\([^;{]*\);", text, re.M))
    c = ig.convert(fn, open(os.path.join(t.auto, "c", f"{fn}.c")).read(), defined, ig.extern_types(text))
    if t_name != "main" and ig.needs_rodata_pad(os.path.join(t.folder, fn + ".s")) \
            and '__asm__(".section .rodata' not in c:
        c = c.rstrip("\n") + "\n" + ig.RODATA_PAD   # same as integrate.py
    print(c)
    text = text.replace(t.inc(fn), c, 1)
    ig.write_file(t.cfile, text)
    ok, log = ig.build(t)
    print("build ok" if ok else "build FAILED")
    if not ok:
        print("\n".join(l for l in log.splitlines() if "error" in l.lower() or ": " in l)[-3000:])
    exe = {"stage1": "stage1"}.get(t_name, t_name)
    import compiler_matrix as cm
    exe_path, vbase = cm.target_info(t_name)[:2]
    built = open(os.path.join("build", "SLUS_007.47" if t_name == "main" else f"{t_name.upper()}.EXE"), "rb").read()
    orig = open(exe_path, "rb").read()
    base = vbase - 0x800
    diffs = [i for i in range(min(len(built), len(orig))) if built[i] != orig[i]]
    print("size", len(built), len(orig), "differing bytes", len(diffs))
    runs = []
    for i in diffs:
        if runs and i - runs[-1][1] <= 8:
            runs[-1][1] = i
        else:
            runs.append([i, i])
    for a, b in runs[:12]:
        print(f"  diff 0x{a + base:08X}-0x{b + base:08X}")
    r = subprocess.run([sys.executable, "tools/funcdiff.py", "--target", t_name, fn],
                       capture_output=True, text=True)
    print(r.stdout[-6000:], r.stderr[-2000:])
finally:
    ig.write_file(t.cfile, backup)
    print("restored", t.cfile)
