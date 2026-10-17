"""compiler_matrix.py - which PSX GCC version reproduces which function?

Run from dualshock/ with the project venv python:
    python tools/compiler_matrix.py <file.c> [--versions 2.6.3,2.7.2,2.8.1] [--flags "-O2 -G0"] [-v]

<file.c> contains decompiled functions named func_XXXXXXXX (the original address). Each
version compiles it like the Makefile does (cpp -> cc1 -> maspsx -> as); every function's
.text is then compared word by word with the original SLUS_007.47 bytes. Words carrying a
relocation (jal targets, %hi/%lo address halves) are only compared on their opcode/register
bits, since their address fields are filled in by the linker.
Output: a matrix of MATCH / size difference / number of differing instructions.
"""

import argparse
import os
import re
import struct
import subprocess
import sys

import m2c_macros

TOOLS = os.path.abspath(os.path.join("..", "tools", "external"))
BIN = os.path.join(TOOLS, "mips-toolchain", "bin")
# Platform: on Windows the venv has Scripts/*.exe and the old GCC is an MSYS2 program; elsewhere bin/.
WINDOWS = os.name == "nt"
EXE = ".exe" if WINDOWS else ""
VENV_BIN = os.path.join(TOOLS, "venv", "Scripts" if WINDOWS else "bin")
PY = os.path.join(VENV_BIN, "python" + EXE)
ORIG = os.path.join("..", "extracted", "RE1-Dualshock", "SLUS_007.47")
OVERLAYS = ["logo", "prologue", "title", "select", "ending"] + [f"stage{i}" for i in range(1, 8)]


def target_info(target):
    """(original exe path, text vram base, nonmatchings dir, C file) for main or an overlay."""
    if target in ("main", "slus_007.47"):
        return ORIG, 0x80010000, os.path.join("asm", "main", "nonmatchings", "capcom"),             os.path.join("src", "main", "capcom.c")
    exe = os.path.join("..", "extracted", "RE1-Dualshock", "PSXUSA", "PROG2", target.upper() + ".EXE")
    base = struct.unpack_from("<I", open(exe, "rb").read(), 0x18)[0]
    return exe, base, os.path.join("asm", target, "nonmatchings", target),         os.path.join("src", target, f"{target}.c")
ENV = dict(os.environ, PATH=os.path.join(TOOLS, "msys64", "usr", "bin") + os.pathsep + os.environ["PATH"]) \
    if WINDOWS else dict(os.environ)


def run(cmd, **kw):
    r = subprocess.run(cmd, env=ENV, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit(f"failed: {' '.join(cmd)}\n{r.stderr}")
    return r.stdout


def build(src, ver, flags, out):
    g = os.path.join(TOOLS, "old-gcc-build", f"gcc-{ver}-psx")
    i, s, ms, o = (os.path.join(out, f"m.{e}") for e in ("i", "s", "ms.s", "o"))
    extra = ["-DM2C_FIELD_STRUCT"] if "-O0" in flags.split() else []   # see include/m2c_fields.h
    run([os.path.join(g, "cpp" + EXE), "-undef", "-nostdinc", "-Iinclude", "-D__GNUC__=2", "-Dmips",
         "-D__mips__", "-D_LANGUAGE_C", "-DLANGUAGE_C", *extra, src, i])
    run([os.path.join(g, "cc1" + EXE), "-quiet", *flags.split(), "-mcpu=3000", "-mips1",
         "-fno-builtin", "-msoft-float", i, "-o", s])
    with open(s) as fi, open(ms, "w") as fo:
        r = subprocess.run([PY, os.path.join(TOOLS, "maspsx", "maspsx.py"), "--aspsx-version=2.56", "--expand-div"],
                           stdin=fi, stdout=fo, env=ENV)
        if r.returncode:
            sys.exit("maspsx failed")
    t = open(ms).read()
    with open(ms, "w", newline="\n") as fo:
        fo.write(fix_align(t, flags))
    run([os.path.join(BIN, "mips64-elf-as"), "-EL", "-march=r3000", "-mtune=r3000", "-mabi=32",
         "-no-pad-sections", "-O1", "-G0", "-Iinclude", "-o", o, ms])
    return o


def text_and_syms(o):
    raw = os.path.join(os.path.dirname(o), "text.bin")
    run([os.path.join(BIN, "mips64-elf-objcopy"), "-O", "binary", "-j", ".text", o, raw])
    text = open(raw, "rb").read()
    syms = {}
    for line in run([os.path.join(BIN, "mips64-elf-nm"), "-S", "--defined-only", o]).splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "Tt" and re.fullmatch(r"func_[0-9A-F]{8}", p[3]):
            syms[p[3]] = (int(p[0], 16), int(p[1], 16))
    relocs = set()
    for line in run([os.path.join(BIN, "mips64-elf-objdump"), "-r", "-j", ".text", o]).splitlines():
        m = re.match(r"^([0-9a-f]{8}) R_MIPS_", line)
        if m:
            relocs.add(int(m.group(1), 16))
    return text, syms, relocs


# ---------------------------------------------------------------- fast in-process helpers
# Every external process costs ~0.1 s on Windows; maspsx (a Python script) and objcopy/nm/
# objdump were 4 of the ~8 processes per candidate. These do the same work in-process.
_MASPSX = None


def maspsx_inprocess(asm_text):
    """maspsx with the project's settings (--aspsx-version=2.56), without a new process."""
    global _MASPSX
    if _MASPSX is None:
        sys.path.insert(0, os.path.join(TOOLS, "maspsx"))
        from maspsx import MaspsxProcessor
        _MASPSX = MaspsxProcessor
    proc = _MASPSX(asm_text.splitlines(keepends=True), sdata_limit=0, expand_div=True,
                   expand_li=False, nop_at_expansion=False, nop_mflo_mfhi=True, nop_lw_lw=False,
                   sltu_at=True, addiu_at=False, div_uses_tge=False, gp_allow_offset=False,
                   gp_allow_la=False, use_comm_section=False, use_comm_for_lcomm=False,
                   max_comm_alignment=16)
    return "\n".join([""] + proc.process_lines()) + "\n"


def fix_align(asm_text, flags):
    """Overlays (-O0) only: jump tables `.align 3` -> `.align 2`, like the Makefile's
    alignfix_for (Capcom's overlay source files were linked 4-aligned; see the Makefile)."""
    if "-O0" not in flags.split():
        return asm_text
    return re.sub(r"^\.align\t3$", ".align\t2", asm_text, flags=re.M)


def read_elf(path):
    """Minimal ELF32 little-endian reader: {section name: (header fields dict, data)}."""
    d = open(path, "rb").read()
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
    hdrs = [struct.unpack_from("<10I", d, shoff + i * shentsize) for i in range(shnum)]
    strtab = hdrs[shstrndx]
    names = d[strtab[4]:strtab[4] + strtab[5]]

    def cstr(blob, off):
        return blob[off:blob.index(b"\0", off)].decode()
    secs = {}
    for idx, h in enumerate(hdrs):
        name = cstr(names, h[0])
        data = d[h[4]:h[4] + h[5]] if h[1] != 8 else b""          # SHT_NOBITS has no data
        secs[name] = ({"index": idx, "type": h[1], "addr": h[3], "link": h[6], "info": h[7]}, data)
    return secs, hdrs, d


def text_and_syms_fast(o):
    """Same result as text_and_syms(): (.text bytes, {func: (offset, size)}, {reloc offsets})."""
    secs, hdrs, d = read_elf(o)
    text_h, text = secs[".text"]
    syms = {}
    if ".symtab" in secs:
        sh, sym = secs[".symtab"]
        strs = secs[".strtab"][1]
        for i in range(0, len(sym), 16):
            st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from("<IIIBBH", sym, i)
            if st_shndx == text_h["index"] and st_name:
                n = strs[st_name:strs.index(b"\0", st_name)].decode()
                if re.fullmatch(r"func_[0-9A-F]{8}", n):
                    syms[n] = (st_value, st_size)
    relocs = set()
    for name, (h, data) in secs.items():
        if h["type"] in (9, 4) and h["info"] == text_h["index"]:      # SHT_REL / SHT_RELA for .text
            step = 8 if h["type"] == 9 else 12
            for i in range(0, len(data), step):
                relocs.add(struct.unpack_from("<I", data, i)[0])
    return text, syms, relocs


def build_fast(src, ver, flags, out):
    """Same as build(), with maspsx in-process."""
    g = os.path.join(TOOLS, "old-gcc-build", f"gcc-{ver}-psx")
    i, s, ms, o = (os.path.join(out, f"m.{e}") for e in ("i", "s", "ms.s", "o"))
    extra = ["-DM2C_FIELD_STRUCT"] if "-O0" in flags.split() else []   # see include/m2c_fields.h
    run([os.path.join(g, "cpp" + EXE), "-undef", "-nostdinc", "-Iinclude", "-D__GNUC__=2", "-Dmips",
         "-D__mips__", "-D_LANGUAGE_C", "-DLANGUAGE_C", *extra, src, i])
    run([os.path.join(g, "cc1" + EXE), "-quiet", *flags.split(), "-mcpu=3000", "-mips1",
         "-fno-builtin", "-msoft-float", i, "-o", s])
    try:
        out_text = fix_align(maspsx_inprocess(open(s).read()), flags)
    except Exception as e:                       # same failure mode as the script version
        sys.exit(f"maspsx failed: {e}")
    with open(ms, "w", newline="\n") as fo:
        fo.write(out_text)
    run([os.path.join(BIN, "mips64-elf-as"), "-EL", "-march=r3000", "-mtune=r3000", "-mabi=32",
         "-no-pad-sections", "-O1", "-G0", "-Iinclude", "-o", o, ms])
    return o


_CTX = None


def _context_decls():
    """{name: declaration} for Sony prototypes (psyq_protos.h) and typed Capcom globals
    (re1_externs.h)."""
    global _CTX
    if _CTX is None:
        # Built in a local dict and published at once: refine/autodecomp call this from many
        # threads, and iterating a half-filled shared dict raised "changed size during iteration".
        ctx = {}
        for hdr, pat in (("psyq_protos.h", r"^[A-Za-z][^;#{}]*?\([^;{}]*\);"),
                         ("re1_externs.h", r"^extern [^;(]*;")):
            text = re.sub(r"/\*.*?\*/", "", open(os.path.join("include", hdr)).read(), flags=re.S)
            for stmt in re.findall(pat, text, re.M):
                one = " ".join(stmt.split())
                m = re.search(r"(\w+)\s*\((?:[^()]|\([^()]*\))*\);$", one) or \
                    re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*;$", one)
                if m:
                    ctx[m.group(1)] = one
        _CTX = ctx
    return _CTX


def preamble_for(code):
    """Includes + the context declarations `code` uses but doesn't declare itself.
    Old drafts declare Sony functions/globals with m2c's guessed types at file scope; adding
    the real prototype for those would be a 'conflicting types' error."""
    m2c_macros.ensure()                     # m2c's header is downloaded, not in git
    lines = ['#include "common.h"', '#include "m2c_fields.h"', '#include "psyq_types.h"',
             '#include "re1_types.h"']
    for name, decl in _context_decls().items():
        if not re.search(rf"(?<![\w.>]){name}\b", code):
            continue
        declared = re.search(rf"^[^\n(]*\b{name}\s*(?:\(|;|\[)", code, re.M) or \
            re.search(rf"^.*\(\*\s*{name}\s*\)", code, re.M)
        if not declared:
            lines.append(decl)
    return "\n".join(lines) + "\n"


# The fast versions produce byte-identical objects/results (checked 2026-10-06); use them.
build_slow, text_and_syms_slow = build, text_and_syms
build, text_and_syms = build_fast, text_and_syms_fast


def disasm_word(w, vram):
    raw = os.path.join("build", "matrix", "_w.bin")
    open(raw, "wb").write(struct.pack("<I", w))
    out = run([os.path.join(BIN, "mips64-elf-objdump"), "-D", "-b", "binary", "-m", "mips:3000",
               "-EL", f"--adjust-vma=0x{vram:X}", raw])
    lines = [l.split("\t", 2) for l in out.splitlines() if re.match(r"^\s*[0-9a-f]+:\t", l)]
    return lines[0][2].replace("\t", " ") if lines and len(lines[0]) > 2 else f"{w:08x}"


def compare(text, off, size, relocs, orig, oaddr, verbose=False, base=0x80010000):
    o = oaddr - base + 0x800
    want = orig[o:o + size]
    got = text[off:off + size]
    ndiff = 0
    for k in range(0, size, 4):
        a, b = struct.unpack_from("<I", want, k)[0], struct.unpack_from("<I", got, k)[0]
        ma, mb = a, b
        if (off + k) in relocs:
            op = a >> 26
            mask = 0xFC000000 if op in (2, 3) else 0xFFFF0000   # j/jal: opcode; else opcode+regs
            ma, mb = a & mask, b & mask
        if ma != mb:
            ndiff += 1
            if verbose:
                print(f"    +0x{k:03X}: original {disasm_word(a, oaddr + k):32s} built {disasm_word(b, oaddr + k)}")
    return ndiff


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("--versions", default="2.6.3,2.7.2,2.8.1")
    ap.add_argument("--flags", default="-O2 -G0")
    ap.add_argument("-v", "--verbose", action="store_true", help="print differing instructions")
    args = ap.parse_args()
    orig = open(ORIG, "rb").read()
    results, names = {}, []
    for ver in args.versions.split(","):
        out = os.path.join("build", "matrix", ver)
        os.makedirs(out, exist_ok=True)
        text, syms, relocs = text_and_syms(build(args.src, ver, args.flags, out))
        for name, (off, size) in syms.items():
            if name not in names:
                names.append(name)
            oaddr = int(name[5:], 16)
            # original size: from the splat asm file header if present
            osize = size
            for root, _, files in os.walk(os.path.join("asm", "main")):
                if name + ".s" in files:
                    # Not necessarily the first line: migrated jump tables (.rodata) can come first.
                    m = re.search(rf"^nonmatching {name},\s*(0x[0-9A-Fa-f]+)",
                                  open(os.path.join(root, name + ".s")).read(), re.M)
                    osize = int(m.group(1), 16) if m else size
                    break
            if size != osize:
                results[(name, ver)] = f"size {size:#x}/{osize:#x}"
            else:
                if args.verbose:
                    print(f"{name} (gcc {ver}):")
                n = compare(text, off, size, relocs, orig, oaddr, args.verbose)
                results[(name, ver)] = "MATCH" if n == 0 else f"{n} insn diff"
    vers = args.versions.split(",")
    print(f"{'function':16s} " + " ".join(f"{v:>16s}" for v in vers))
    for name in sorted(names):
        print(f"{name:16s} " + " ".join(f"{results.get((name, v), '-'):>16s}" for v in vers))


if __name__ == "__main__":
    main()
