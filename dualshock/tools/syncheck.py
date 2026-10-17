"""syncheck.py - parse every C file with a modern GCC (the MIPS toolchain's mips64-elf-gcc -fsyntax-only -std=gnu89)
and count the errors. The sources must stay valid GNU C89 for editors (IntelliSense) and modern compilers, not
only for GCC 2.6.3. Expected result: `total errors 0`.

    make compile_commands
    python tools/syncheck.py          (from dualshock/)
"""
import collections
import json
import os
import subprocess

GCC = os.path.abspath("../tools/external/mips-toolchain/bin/mips64-elf-gcc" + (".exe" if os.name == "nt" else ""))
cc = json.load(open("compile_commands.json"))
errs = collections.Counter(); per = collections.Counter(); samples = {}
for e in cc:
    args = [GCC, "-fsyntax-only", "-std=gnu89", "-mabi=32", "-march=r3000", "-w"] + e["arguments"][1:-2] + [e["file"]]
    r = subprocess.run(args, capture_output=True, text=True, cwd=e["directory"])
    for l in r.stderr.splitlines():
        if ": error:" in l:
            msg = l.split(": error:")[1].strip()
            key = msg.split("'")[0][:60]
            errs[key] += 1; per[os.path.basename(e["file"])] += 1
            samples.setdefault(key, l[-200:])
print("total errors", sum(errs.values()))
for k, v in errs.most_common(15): print(v, k, "|", samples[k][-140:])
print(per.most_common(8))
