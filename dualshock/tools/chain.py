"""chain.py <in.c> <out.c> <transform.py> [<transform.py> ...]: run several transforms one after
the other (each `transform.py in out`), so trybatch can test fixes that only match together
(stage3 func_8012B1EC needed switchtemp + emptythen + a dropped `return`). An argument written
`name.py:--flag` passes `--flag` to that transform."""
import os
import shutil
import subprocess
import sys

src, dst = sys.argv[1], sys.argv[2]
tmp = dst + ".chain"
shutil.copy(src, tmp)
for t in sys.argv[3:]:
    path, _, flag = t.partition(":")
    if not os.path.isabs(path) and not os.path.exists(path):
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), path)
    subprocess.run([sys.executable, "-I", path, tmp, tmp] + ([flag] if flag else []), check=True)
shutil.move(tmp, dst)
