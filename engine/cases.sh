#!/bin/sh
# cases.sh run|compare: speaks every case through the QEMU image, then holds
# each against the desktop evv. The two halves are separate so the runs can
# happen while the OpenEVV tree is busy building something else.
#
# The cases are OpenEVV's English plain and long files, the first 40 of its
# crashers, and 30 lines of 20 dictionary words each.
set -e
cd "$(dirname "$0")"
openevv=${OPENEVV:-/home/stas/Projects/openevv-sp404}
out=build/cases

cases() {
    cat "$openevv/test/cases/plain.txt" "$openevv/test/cases/long.txt"
    head -n 40 "$openevv/test/cases/crashers.txt"
    head -n 600 "$openevv/test/cases/words-enus.txt" | paste -d' ' - - - - - - - - - - - - - - - - - - - -
}

case "$1" in
run)
    rm -rf "$out"
    mkdir -p "$out"
    n=0
    cases | while IFS= read -r line; do
        n=$((n + 1))
        printf '%s' "$line" > in.txt
        printf '%s' "$line" > "$out/$n.txt"
        rm -f out.raw
        if timeout 300 qemu-system-arm -M mps2-an500 -nographic -monitor none \
             -serial none -semihosting-config enable=on,target=native \
             -kernel build/qemu.elf > "$out/$n.log" 2>&1 && [ -f out.raw ]; then
            mv out.raw "$out/$n.raw"
        else
            echo "case $n: the run failed: $(tail -n 1 "$out/$n.log")"
        fi
    done
    rm -f in.txt
    echo "ran $(ls "$out"/*.txt | wc -l) cases"
    ;;
compare)
    pass=0
    fail=0
    for t in "$out"/*.txt; do
        n=$(basename "$t" .txt)
        if [ -f "$out/$n.raw" ] && ./compare.sh "$out/$n.raw" "$(cat "$t")" > "$out/$n.cmp" 2>&1; then
            pass=$((pass + 1))
        else
            fail=$((fail + 1))
            echo "case $n: $(tail -n 1 "$out/$n.cmp" 2>/dev/null)"
        fi
    done
    echo "$pass match, $fail differ"
    [ "$fail" -eq 0 ]
    ;;
*)
    echo "usage: cases.sh run|compare" >&2
    exit 2
    ;;
esac
