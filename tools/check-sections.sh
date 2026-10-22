#!/bin/sh
# Fail the build if any object calls a function in a different named code
# section with a PC-relative (DISP16) branch. That happens when a caller cannot
# see the function's section attribute, and on the device it jumps into the
# wrong code resource without a crash (M0 lesson, Oct 22 2026). A PC-relative
# call between two functions in the same section is correct and allowed.
# Runs inside the toolchain image: tools/check-sections.sh <linked-exe> <objs...>
set -e
exe="$1"; shift
map=$(mktemp)
# COFF symbol lines look like: [ 12](sec  1)(fl 0x00)(ty   0)(scl   2) (nx 0) 0x0000001c Name
# scl 3 lines name a section; scl 2 lines are external symbols in that section.
m68k-palmos-objdump -t "$exe" | awk '
    match($0, /\(sec +[0-9]+\)/) { sec = substr($0, RSTART + 5, RLENGTH - 6) + 0 }
    /\(scl +3\)/ { secname[sec] = $NF }
    /\(scl +2\)/ && sec > 0 { sym[$NF] = sec }
    END { for (s in sym) { n = secname[sym[s]]; if (n != ".text" && n != ".data" && n != ".bss") print s, n } }
' > "$map"
if [ ! -s "$map" ]; then echo "check-sections: no functions outside .text"; rm -f "$map"; exit 0; fi
status=0
for obj in "$@"; do
    out=$(m68k-palmos-objdump -r "$obj" | awk -v obj="$obj" '
        FNR == NR { symsec[$1] = $2; next }
        /^RELOCATION RECORDS FOR \[/ { sec = $0; sub(/.*\[/, "", sec); sub(/\].*/, "", sec) }
        /DISP16/ { s = $NF; if ((s in symsec) && symsec[s] != sec)
            print "check-sections: " obj " (" sec ") calls " s " (" symsec[s] ") with a PC-relative branch; its declaration lacks the section attribute" }
    ' "$map" -)
    if [ -n "$out" ]; then echo "$out"; status=1; fi
done
n=$(wc -l < "$map" | tr -d ' ')
rm -f "$map"
[ $status -eq 0 ] && echo "check-sections: ok ($n sectioned function(s))"
exit $status
