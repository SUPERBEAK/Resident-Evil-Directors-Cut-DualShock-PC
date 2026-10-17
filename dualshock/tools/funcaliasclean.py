"""funcaliasclean.py - replace per-function function aliases by the real function name.

Run from dualshock/ with the project venv python:
    python tools/funcaliasclean.py --target main        (or logo, stage1, ...)

During matching many functions declared their callees as private aliases,
`void pad_update_l(void) __asm__("pad_update");`, so that their own prototype could differ from the
definition. Where the alias's prototype has exactly the definition's types (return type and
parameter types) AND the definition comes earlier in the file (so it is already in scope), calling
the real name compiles identically: the alias declaration is removed and its calls are renamed.
Where the prototype is identical but the definition comes LATER, the alias becomes an ordinary
block-scope prototype with the real name (`s32 func_X(s16, s16, void *);`); a function some other
function declares with a conflicting prototype ("conflicting types") is excluded and the run retried.
No new file-scope prototypes are added (they would change how callers without a prototype pass narrow
arguments). Rebuild; functions whose bytes change get their old text back, until all 13 match.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402
import declclean as dc  # noqa: E402  (bad_functions)

ALIAS = re.compile(r'^\s+(?:extern\s+)?([A-Za-z_][\w \*]*?)\s*\b(\w+)\s*\(([^()]*)\)\s*__asm__\("(\w+)"\);\s*$')
DEF = re.compile(r'^(?:static\s+)?([A-Za-z_][\w \*]*?)\s*\b(\w+)\s*\(([^()]*)\)\s*\{\s*$')


def norm_type(t):
    return " ".join(t.replace("*", " * ").split())


def param_types(params):
    """'s32 a, u8 *p' -> ('s32', 'u8 *'); '' / 'void' -> ('void',); None if not simple."""
    params = params.strip()
    if params in ("", "void"):
        return ("void",) if params == "void" else ()
    out = []
    for p in params.split(","):
        p = norm_type(p)
        if p == "...":
            out.append(p)
            continue
        words = p.split()
        # drop a trailing parameter name (an identifier after the type words)
        if len(words) > 1 and re.match(r"^[A-Za-z_]\w*$", words[-1]) and words[-1] not in (
                "int", "char", "short", "long", "unsigned", "signed", "void") and not re.match(
                r"^[su](8|16|32|64)$|^u_(char|short|int|long)$", words[-1]):
            words = words[:-1]
        out.append(" ".join(words))
    return tuple(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="main")
    a = ap.parse_args()
    t = ig.Target(a.target)
    original = open(t.cfile).read()
    lines = original.split("\n")
    spans = ig.function_spans(original)
    sigs, order = {}, {}
    for idx, (s, e, n) in enumerate(spans):
        order[n] = idx
        m = DEF.match(lines[s - 1])
        if m and m.group(2) == n:
            sigs[n] = (norm_type(m.group(1)), param_types(m.group(3)))

    def convert(excluded):
        new_funcs, n_removed = {}, 0
        for idx, (s, e, n) in enumerate(spans):
            body, renames = [], {}
            for k in range(s - 1, e):
                m = ALIAS.match(lines[k]) if k > s - 1 else None
                if m:
                    ret, alias, params, target = norm_type(m.group(1)), m.group(2), m.group(3), m.group(4)
                    if (target in sigs and target not in excluded and alias != target
                            and sigs[target] == (ret, param_types(params))):
                        renames[alias] = target
                        if order.get(target, 1 << 30) < idx:
                            continue                      # the definition above is in scope
                        # definition below: an ordinary prototype with the real name
                        indent = lines[k][:len(lines[k]) - len(lines[k].lstrip())]
                        body.append(f"{indent}{m.group(1).strip()} {target}({params});")
                        continue
                body.append(lines[k])
            out = []
            for line in body:
                for old, new in renames.items():
                    line = ig.sub_code(r"(?<![\w.])" + re.escape(old) + r"(?!\w)", new, line)
                out.append(line)
            new_funcs[n] = out
            n_removed += len(renames)
        return new_funcs, n_removed

    excluded = set()
    new_funcs, n_removed = convert(excluded)
    print(f"{a.target}: {n_removed} function aliases can be replaced by the real name")

    def assemble(keep_old):
        out, pos = [], 0
        for s, e, n in spans:
            out += lines[pos:s - 1]
            out += lines[s - 1:e] if n in keep_old else new_funcs[n]
            pos = e
        return "\n".join(out + lines[pos:])

    keep_old = set()
    for _ in range(60):
        ig.write_file(t.cfile, assemble(keep_old))
        ok, log = ig.build(t)
        if ok:
            print(f"OK: {a.target} builds and matches; {n_removed} aliases replaced, "
                  f"{len(keep_old)} function(s) kept their aliases")
            return
        errs = re.findall(re.escape(t.cfile.replace(os.sep, "/")) + r":(\d+): (?!warning)(.*)", log)
        conflicts = set(re.findall(r"conflicting types for `(\w+)'", log)) - excluded
        if conflicts and conflicts <= set(sigs):
            excluded |= conflicts
            new_funcs, n_removed = convert(excluded)
            print(f"  conflicting prototypes elsewhere, excluded: {' '.join(sorted(conflicts))}")
            continue
        if errs:
            ig.write_file(os.path.join("build", "funcaliasclean_failed.c"), open(t.cfile).read())
            print("compile errors (file kept as build/funcaliasclean_failed.c), restoring:")
            for ln, msg in errs[:10]:
                print(f"  line {ln}: {msg}")
            break
        bad = dc.bad_functions(t, [n for _, _, n in spans if n not in keep_old])
        if not bad:
            print("build failed for another reason, restoring:\n" + log[-2000:])
            break
        print(f"  {len(bad)} function(s) changed bytes, keeping their aliases: {' '.join(bad[:8])}")
        keep_old |= set(bad)
    ig.write_file(t.cfile, original)
    ig.build(t)
    sys.exit(1)


if __name__ == "__main__":
    main()
