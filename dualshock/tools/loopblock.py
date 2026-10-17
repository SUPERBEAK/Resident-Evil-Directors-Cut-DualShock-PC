"""loopblock.py <in.c> <out.c> [--all]: GCC 2.6.3's parser calls emit_nop() at the start of every
`while`/`for`/`do` and before user labels; at -O0 that emits a real `nop` when the previous insn is
a CODE_LABEL (e.g. the end of an `if`). When the original has no such `nop` but ours does, the
original had a `{ }` block around the preceding `if` (its block-end NOTE becomes the previous insn).
Wrap the `if ... { } [else { }]` statement that directly precedes a loop in braces
(stage2 func_801163E0). Without --all only the first loop is handled. For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
do_all = "--all" in sys.argv[3:]
lines = s.split("\n")
i = 0
changed = False
while i < len(lines):
    m = re.match(r"^([ \t]*)(?:while \(|for \(|do \{)", lines[i])
    if m and i > 0 and lines[i - 1] == m.group(1) + "}" and not (changed and not do_all):
        ind = m.group(1)
        # walk back to the `if` at the same indentation that starts this if/else chain
        j = i - 1
        while j >= 0 and not re.match(rf"^{re.escape(ind)}if \(", lines[j]):
            if re.match(rf"^{re.escape(ind)}\S", lines[j]) and not lines[j].startswith(ind + "}"):
                j = -1
                break
            j -= 1
        if j >= 0:
            body = ["    " + l if l else l for l in lines[j:i]]
            lines[j:i] = [ind + "{"] + body + [ind + "}"]
            i += 2
            changed = True
    i += 1
open(sys.argv[2], "w", newline="\n").write("\n".join(lines))
