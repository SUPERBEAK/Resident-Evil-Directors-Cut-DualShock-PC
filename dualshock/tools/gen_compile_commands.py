"""gen_compile_commands.py - write dualshock/compile_commands.json for editor IntelliSense (VS Code C/C++ etc.).

Run from dualshock/ with the project venv python (or `make compile_commands`); re-run after adding / splitting files.

Every src/<program>/*.c gets the preprocessor flags the Makefile's cpp step uses (-Iinclude, the defines).
src/shared/<enemy>.c is never compiled on its own: a stage wrapper (src/stageN/<enemy>.c) includes it after the
common headers and that stage's globals.h - its entry force-includes exactly those (-include), using the first
stage that has the enemy. The file holds absolute paths, so it is not in git (see .gitignore).
"""
import json
import os
import re

ROOT = os.path.abspath(".")
BASE = ["-undef", "-nostdinc", "-Iinclude", "-D__GNUC__=2", "-Dmips", "-D__mips__", "-D_LANGUAGE_C",
        "-DLANGUAGE_C"]


def entry(path, extra):
    args = ["gcc"] + BASE + extra + ["-c", path]
    return {"directory": ROOT, "file": os.path.normpath(os.path.join(ROOT, path)), "arguments": args}


out = []
wrapper_of = {}
for prog in sorted(os.listdir("src")):
    d = os.path.join("src", prog)
    if not os.path.isdir(d) or prog == "shared":
        continue
    extra = [] if prog == "main" else ["-DM2C_FIELD_STRUCT"]
    for f in sorted(os.listdir(d)):
        if not f.endswith(".c"):
            continue
        p = os.path.join(d, f).replace(os.sep, "/")
        out.append(entry(p, extra))
        m = re.search(r'#include "\.\./shared/(\w+\.c)"', open(p, encoding="latin-1").read())
        if m and m.group(1) not in wrapper_of:
            wrapper_of[m.group(1)] = p
for f in sorted(os.listdir(os.path.join("src", "shared"))):
    if not f.endswith(".c"):
        continue
    w = wrapper_of.get(f)
    if not w:
        continue
    pre = []
    for line in open(w, encoding="latin-1"):
        m = re.match(r'#include "([^"]+)"', line)
        if not m or "../shared/" in m.group(1):
            continue
        inc = m.group(1)
        local = os.path.join(os.path.dirname(w), inc).replace(os.sep, "/")
        pre += ["-include", local if os.path.isfile(local) else "include/" + inc]
    out.append(entry(f"src/shared/{f}", ["-DM2C_FIELD_STRUCT"] + pre))
json.dump(out, open("compile_commands.json", "w", newline="\n"), indent=1)
print(len(out), "entries -> compile_commands.json")
