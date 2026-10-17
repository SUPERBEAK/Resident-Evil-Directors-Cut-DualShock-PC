# jtalign.awk - main program: GCC aligns switch jump tables to 8 (`.align 3`). Capcom's main was
# built from several source files, so some tables are only 4-aligned. A function whose table is
# 4-aligned in the original is preceded in the C file by the marker
#     __asm__("#JTALIGN2");
# and its next `.align 3` becomes `.align 2` (e.g. func_8004748C, jtbl_8001074C).
# One marker per table: two markers convert the next two tables. `#JTALIGN2PAD` does the same and
# also appends a `.word 0` pad after that table, before GCC switches back to `.text` - for a
# function whose second table follows a padded first one (func_80049478: jtbl_80010764 + pad,
# jtbl_8001078C).
/^[ \t]*#JTALIGN2PAD/ { n++; padq++; next }
/^[ \t]*#JTALIGN2/ { n++; next }
n > 0 && /^\t?\.align\t3$/ {
    print "\t.align\t2"; n--
    if (padq > 0) { padq--; inpad = 1 }
    next
}
inpad && /^\t?\.text/ { print "\t.word\t0"; inpad = 0 }
{ print }
