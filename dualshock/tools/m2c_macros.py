"""m2c_macros.py - download m2c's own m2c_macros.h into include/ (it is GPL-3.0, so it is not in git).

m2c's "valid syntax" output uses that header's macros (M2C_UNK, M2C_FIELD, M2C_ERROR, ...). The
decompiler drafts (tools/autodecomp.py, refine.py, ...) compile with it through include/m2c_fields.h,
which adds this project's two forms of M2C_FIELD. The file comes from the same m2c commit the venv
installs (tools/requirements.txt); keep the two in step.

    python tools/m2c_macros.py        (from dualshock/; setup_tools.sh runs it, and
                                      compiler_matrix.preamble_for() calls ensure() itself)
"""

import os
import threading
import urllib.request

M2C_COMMIT = "708d2d2cb2698f091a92492b328f73b24209f72d"
URL = f"https://raw.githubusercontent.com/matt-kempster/m2c/{M2C_COMMIT}/m2c_macros.h"
PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "include", "m2c_macros.h")

_LOCK = threading.Lock()    # refine/autodecomp build drafts from many threads


def ensure():
    """Download include/m2c_macros.h if it is missing; returns its path."""
    with _LOCK:
        if not os.path.exists(PATH):
            with urllib.request.urlopen(URL, timeout=60) as r:
                data = r.read()
            tmp = f"{PATH}.{os.getpid()}.tmp"
            with open(tmp, "wb") as f:
                f.write(data)
            os.replace(tmp, PATH)
    return PATH


if __name__ == "__main__":
    print(os.path.normpath(ensure()))
