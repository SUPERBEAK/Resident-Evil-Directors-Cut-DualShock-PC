"""condassign.py <in.c> <out.c>: `if (((v = EXPR) < Y) && ...)` -> `v = EXPR; if ((v < Y) && ...)`.
At -O0 Capcom's separate statement stores v and reloads it (sb + lbu); the assignment inside
the condition keeps the value in a register (stage5 func_8011A79C, stage7 func_801198C4).
Only for a plain local `v` (sp*/var_*/temp_*). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
pat = re.compile(r"^([ \t]*)(if|while) \((\(*)\(((?:sp|var_|temp_)\w+) = ([^()]*(?:\([^()]*(?:\([^()]*\)[^()]*)*\)[^()]*)*)\)", re.M)
s = pat.sub(lambda m: f"{m.group(1)}{m.group(4)} = {m.group(5)};\n{m.group(1)}{m.group(2)} ({m.group(3)}{m.group(4)}"
            if m.group(2) == "if" else m.group(0), s)
open(sys.argv[2], "w", newline="\n").write(s)
