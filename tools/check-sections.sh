#!/bin/sh
# Fail the build if any object calls a function in a named code section with a
# PC-relative (DISP16) branch. That happens when a caller cannot see the
# function's section attribute, and on the device it jumps into the wrong
# code resource without a crash (M0 lesson, Oct 22 2026).
# Runs inside the toolchain image: tools/check-sections.sh <linked-exe> <objs...>
set -e
exe="$1"; shift
syms=$(mktemp)
m68k-palmos-objdump -t "$exe" > "$syms"
# COFF symbol lines look like: [ 12](sec  1)(fl 0x00)(ty   0)(scl   2) (nx 0) 0x0000001c Name
# scl 3 lines name a section; scl 2 lines are external symbols in that section.
sectioned=$(awk '
    match($0, /\(sec +[0-9]+\)/) { sec = substr($0, RSTART + 5, RLENGTH - 6) + 0 }
    /\(scl +3\)/ { secname[sec] = $NF }
    /\(scl +2\)/ && sec > 0 { sym[$NF] = sec }
    END { for (s in sym) { n = secname[sym[s]]; if (n != ".text" && n != ".data" && n != ".bss") print s } }
' "$syms")
rm -f "$syms"
[ -n "$sectioned" ] || { echo "check-sections: no functions outside .text"; exit 0; }
status=0
for obj in "$@"; do
    for sym in $sectioned; do
        if m68k-palmos-objdump -r "$obj" | grep -q "DISP16[[:space:]]*$sym\$"; then
            echo "check-sections: $obj calls $sym with a PC-relative branch; its declaration lacks the section attribute"
            status=1
        fi
    done
done
[ $status -eq 0 ] && echo "check-sections: ok ($(echo $sectioned | wc -w | tr -d ' ') sectioned function(s): $(echo $sectioned | tr '\n' ' '))"
exit $status
