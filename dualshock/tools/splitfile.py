"""splitfile.py - split one main-program topic file in two at a function boundary, keeping the byte match.

Run from dualshock/ with the project venv python (after `make split`, so asm/main/nonmatchings/ is current):
    python tools/splitfile.py <file> <first function of part 2> <part 1 name> <part 2 name> "<part 2 header>"
    e.g. python tools/splitfile.py enemy_path set_initial_items enemy_path inventory "Inventory and item box"

Part 1 keeps the functions before the split (and may get a new name: the file is then renamed), part 2 gets
the rest. Every function keeps seeing what it saw before:
  - part 2 starts with part 1's includes and its "functions defined in the files before this one" block,
    plus prototypes (copied from the definitions) of part-1 functions it uses, plus the file-scope typedefs /
    macros / declarations of part 1 whose names part 2 uses;
  - functions defined later stay unseen (implicit declarations behave as before).
The yaml gets a `c` subsegment for part 2 at its first function's file offset and `.rodata` subsegments at
the first rodata item of each part's functions (splat's per-function .s files). Then `make split` and a clean
build must match all 13 executables; otherwise every change is restored.
(The original single-file split was tools/splitc.py; the yaml now uses subalign 4.)
"""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import integrate as ig  # noqa: E402

BUILDENV = os.path.join("..", "tools", "buildenv.sh")
YAML = "slus_007.47.yaml"
RODATA_LINE = re.compile(r"^\s*/\* ([0-9A-F]+) ([0-9A-F]{8}) ")


def offsets(folder, name):
    """(text file offset, first rodata file offset or None) of one function from splat's .s file."""
    text = open(os.path.join("asm", "main", "nonmatchings", folder, name + ".s")).read()
    ro, tx = text.split(".section .text", 1) if ".section .text" in text else ("", text)
    t = re.search(r"^\s*/\* ([0-9A-F]+) [0-9A-F]{8} [0-9A-F]{8} \*/", tx, re.M)
    r = [int(m.group(1), 16) for m in (RODATA_LINE.match(l) for l in ro.splitlines()) if m]
    return int(t.group(1), 16), (min(r) if r else None)


def items(text):
    """Split file-scope text (no function bodies) into items: preprocessor lines, `...;` declarations and
    typedef blocks; a comment directly before an item belongs to it. Returns [(text, defined names)]."""
    out, i, n = [], 0, len(text)
    pending = ""
    while i < n:
        if text[i].isspace():
            j = i
            while j < n and text[j].isspace():
                j += 1
            pending += text[i:j]
            i = j
            continue
        if text.startswith("/*", i):
            j = text.index("*/", i) + 2
            pending += text[i:j]
            i = j
            continue
        if text[i] == "#":
            j = i
            while True:
                e = text.find("\n", j)
                e = n if e < 0 else e
                if text[e - 1] == "\\":
                    j = e + 1
                    continue
                break
            body = text[i:e]
            m = re.match(r"#\s*define\s+(\w+)", body)
            out.append((pending + body + "\n", {m.group(1)} if m else set()))
            pending, i = "", e + 1
            continue
        depth, j = 0, i
        while j < n:
            c = text[j]
            if text.startswith("/*", j):
                j = text.index("*/", j) + 2
                continue
            depth += (c in "{(") - (c in "})")
            if c == ";" and depth == 0:
                break
            j += 1
        body = text[i:j + 1]
        flat = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        names = set()
        m = re.search(r"\}\s*(\w+)\s*;\s*$", flat)
        if m:
            names.add(m.group(1))
        m = re.match(r"\s*typedef\b.*?(\w+)\s*(?:\[[^\]]*\])*\s*;\s*$", flat, re.S)
        if m:
            names.add(m.group(1))
        for m in re.finditer(r"\b(\w+)\s*(?:\(|\[|__asm__|;|=)", re.sub(r"\{.*\}", "{}", flat, flags=re.S)):
            if m.group(1) not in ("typedef", "struct", "union", "extern", "void", "const", "__asm__", "sizeof"):
                names.add(m.group(1))
        for m in re.finditer(r"\b(?:struct|union)\s+(\w+)", flat):
            names.add(m.group(1))
        out.append((pending + body + "\n", names))
        pending, i = "", j + 1
    return out


def run(cmd):
    r = subprocess.run(["sh", BUILDENV, cmd], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    old, first2, name1, name2, header2 = sys.argv[1:]
    src = os.path.join("src", "main", old + ".c")
    text = open(src).read()
    lines = text.split("\n")
    spans = ig.function_spans(text)          # (first line, last line, name), 1-based
    names = [n for _, _, n in spans]
    k = names.index(first2)
    if k == 0:
        sys.exit("part 1 would be empty")
    # part 2 starts right after the closing brace of part 1's last function
    cut = spans[k - 1][1]
    part1 = "\n".join(lines[:cut]) + "\n"
    rest = "\n".join(lines[cut:])
    prelude = "\n".join(lines[:spans[0][0] - 1])
    # file-scope text of part 1: its prelude (includes, prototype block, ...) + the text between functions
    scope1 = prelude + "\n" + "\n".join("\n".join(lines[spans[i][1]:spans[i + 1][0] - 1]) for i in range(k - 1))
    used = set(re.findall(r"\b\w+\b", re.sub(r"/\*.*?\*/", "", rest, flags=re.S)))
    head_lines = []
    for line in prelude.split("\n"):
        if line.startswith("#include"):
            head_lines.append(line)
    keep = []
    for body, defined in items(scope1):
        if body.lstrip().startswith("#include") or re.match(r"\s*#\s*(?:ifndef|define\s+\w+_H\b|endif)", body):
            continue
        if defined & used:
            keep.append(body.strip("\n"))
    # prototypes of part-1 functions that part 2 uses (exactly the definition's header)
    protos = []
    for s, e, n in spans[:k]:
        if n in used:
            hdr, j = "", s - 1
            while "{" not in lines[j]:
                hdr += lines[j] + " "
                j += 1
            hdr += lines[j].split("{")[0]
            protos.append(re.sub(r"\s+", " ", hdr).strip() + ";")
    import textwrap
    header = "\n".join(textwrap.wrap(f"/* {name2}.c - {header2} */", 110, subsequent_indent="   "))
    part2 = (header + "\n\n" + "\n".join(head_lines) + "\n\n"
             + ("\n\n".join(keep) + "\n\n" if keep else "")
             + ("/* Defined earlier in the same file before it was split (tools/splitfile.py): what these\n"
                "   functions saw when they were one file. */\n" + "\n".join(protos) + "\n\n" if protos else "")
             + rest.lstrip("\n"))

    # yaml
    folder = old
    t2, r2 = offsets(folder, first2)
    r1 = [offsets(folder, n)[1] for n in names[:k]]
    r1 = min([r for r in r1 if r is not None], default=None)
    r2s = [offsets(folder, n)[1] for n in names[k:]]
    r2 = min([r for r in r2s if r is not None], default=None)
    y = open(YAML).read()
    y0 = y
    m_ro = re.search(rf"^      - \[0x[0-9A-F]+, \.rodata, {old}\]\n", y, re.M)
    m_c = re.search(rf"^      - \[(0x[0-9A-F]+), c, {old}\]\n", y, re.M)
    assert m_c, "yaml c subsegment not found"
    c_lines = f"      - [{m_c.group(1)}, c, {name1}]\n      - [0x{t2:X}, c, {name2}]\n"
    y = y[:m_c.start()] + c_lines + y[m_c.end():]
    ro_lines = ""
    if r1 is not None:
        ro_lines += f"      - [0x{r1:X}, .rodata, {name1}]\n"
    if r2 is not None:
        ro_lines += f"      - [0x{r2:X}, .rodata, {name2}]\n"
    if m_ro:
        m_ro = re.search(rf"^      - \[0x[0-9A-F]+, \.rodata, {old}\]\n", y, re.M)
        y = y[:m_ro.start()] + ro_lines + y[m_ro.end():]
    elif ro_lines:
        sys.exit("part has rodata but the file had no .rodata subsegment - not handled")

    backup = {src: text, YAML: y0}
    new1 = os.path.join("src", "main", name1 + ".c")
    new2 = os.path.join("src", "main", name2 + ".c")
    for p in (new1, new2):
        if p != src and os.path.exists(p):
            sys.exit(f"{p} exists")
    if new1 != src:
        os.remove(src)
    open(new1, "w", newline="\n").write(part1)
    open(new2, "w", newline="\n").write(part2)
    open(YAML, "w", newline="\n").write(y)
    print(f"{old}.c -> {name1}.c ({k} functions) + {name2}.c ({len(names) - k} functions); "
          f"text 0x{t2:X}, rodata {hex(r1) if r1 is not None else '-'} / {hex(r2) if r2 is not None else '-'}")
    rc, log = run("make split")
    if rc == 0:
        rc, log = run("make clean && make -j8")
    if rc == 0 and "All 13 executables match" in log:
        print("OK: all 13 executables still match")
        return
    print("FAILED - restoring:\n" + "\n".join(l for l in log.splitlines()
                                             if "rror" in l or "MISMATCH" in l or "undeclared" in l)[-3000:])
    for p in (new1, new2):
        if os.path.exists(p):
            os.remove(p)
    for p, t in backup.items():
        open(p, "w", newline="\n").write(t)
    run("make split")
    rc, log = run("make clean && make -j8")
    print("restored;", "build matches" if "All 13 executables match" in log else "BUILD STILL BROKEN")
    sys.exit(1)


if __name__ == "__main__":
    main()
