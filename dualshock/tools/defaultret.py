"""defaultret.py <in.c> <out.c>: m2c ends a switch whose last case falls out of the switch with
`/* fallthrough */ default: return;` (or just `default: return;` after the case's last
statement). Capcom's source has `break;` there and no default (stage7 func_8011CF54,
func_8011B890, stage3 func_8011ABD4, title func_800E07BC). For trybatch."""
import re
import sys

s = open(sys.argv[1]).read()
s = re.sub(r"\n([ \t]*)/\* fallthrough \*/\n[ \t]*default:[^\n]*\n[ \t]*return;\n", r"\n\1break;\n", s)
s = re.sub(r"(;|\})\n([ \t]*)default:[^\n]*\n[ \t]*return;\n(\2\})", r"\1\n\2    break;\n\3", s)
open(sys.argv[2], "w", newline="\n").write(s)
