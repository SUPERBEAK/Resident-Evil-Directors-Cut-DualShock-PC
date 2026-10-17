"""decsign.py <in.c> <out.c> [--unsigned]: m2c often guesses the wrong signedness for a counter that
is decremented inside a test (`if (--x == 0)`): a signed short is sign-extended after the
decrement (`sll/sra`), an unsigned one masked (`li 0xFFFF; addu`; `andi`). Default: u16/u8 -> s16/s8
in `--F(...)` / `F(...)--` tests; --unsigned the other way (stage4 func_80114D20). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
a, b = ("s", "u") if "--unsigned" in sys.argv[3:] else ("u", "s")
F = r"(M2C_P?FIELD\((?:[^(),]|\([^()]*\))+?, )" + a + r"(8|16)( \*, (?:0x[0-9A-F]+|\d+)\))"
s = re.sub(r"--" + F, lambda m: f"--{m.group(1)}{b}{m.group(2)}{m.group(3)}", s)
s = re.sub(F + r"--", lambda m: f"{m.group(1)}{b}{m.group(2)}{m.group(3)}--", s)
open(sys.argv[2], "w", newline="\n").write(s)
