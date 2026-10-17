"""test_integrate.py - regression test for tools/integrate.py on already-matched functions.

Run from dualshock/ with the project venv python:
    python tools/test_integrate.py <target> <func> [<func> ...]

For each function: takes its current C definition as the draft, puts INCLUDE_ASM back in its
place, runs `integrate.py --target <target> --only <funcs>` and reports whether integrate.py
brought the build back to matching (and whether the C text came back identical; the
integrated file is kept as build/test_integrate_after.c for a diff). The C file,
the stored drafts and results.json are always restored afterwards, then the target is rebuilt.
Pick functions without a `#JTALIGN2` marker or INCLUDE_RODATA lines right in front of them
(those stay in the file and would then apply to the wrong code).
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate  # noqa: E402


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    name, funcs = sys.argv[1], sys.argv[2:]
    t = integrate.Target(name)
    res_path = os.path.join(t.auto, "results.json")
    original = open(t.cfile).read()
    results_backup = open(res_path).read()
    draft_backups, created = {}, []
    try:
        lines = original.split("\n")
        spans = {n: (s, e) for s, e, n in integrate.function_spans(original)}
        results = json.loads(results_backup)
        # replace from the bottom up so earlier line numbers stay valid
        for f in sorted(funcs, key=lambda f: -spans[f][0]):
            s, e = spans[f]
            draft = os.path.join(t.auto, "c", f + ".c")
            if os.path.exists(draft):
                draft_backups[draft] = open(draft).read()
            else:
                created.append(draft)
            open(draft, "w", newline="\n").write("\n".join(lines[s - 1:e]) + "\n")
            results[f] = "match"
            lines[s - 1:e] = [t.inc(f)]
        json.dump(results, open(res_path, "w"), indent=1, sort_keys=True)
        integrate.write_file(t.cfile, "\n".join(lines))
        r = subprocess.run([sys.executable, os.path.join("tools", "integrate.py"), "--target", name,
                            "--only", *funcs], capture_output=True, text=True)
        out = r.stdout + r.stderr
        print(out.strip())
        after = open(t.cfile).read()
        open(os.path.join("build", "test_integrate_after.c"), "w", newline="\n").write(after)
        ok = r.returncode == 0 and "OK:" in out
        print("RESULT:", "PASS" if ok else "FAIL",
              "(C text identical)" if after == original else "(C text differs from before)")
    finally:
        integrate.write_file(t.cfile, original)
        open(res_path, "w", newline="\n").write(results_backup)
        for p, text in draft_backups.items():
            open(p, "w", newline="\n").write(text)
        for p in created:
            os.remove(p)
        ok_build, log = integrate.build(t)
        print("restored; rebuild", "matches" if ok_build else "FAILED:\n" + log[-2000:])


if __name__ == "__main__":
    main()
