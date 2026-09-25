#!/bin/sh
# import.sh [DIR]: builds the Ghidra project from Roland's image, in
# ghidra/project unless DIR names another. Nothing here is saved anywhere
# else, so this is the only way back if the project is lost.
#
# After the import, Ghidra's own analysis leaves most methods reached through
# vtables and pointer tables outside any function, and misses the tail-call
# functions with no push. MkMissing and MkFns put those right; notes.md has
# why, under "Ghidra's function coverage was badly incomplete".
set -e
here=$(cd "$(dirname "$0")" && pwd)
proj=${1:-$here/project}
if [ -e "$proj/sp404.gpr" ]; then
    echo "a project already exists at $proj" >&2
    exit 1
fi
mkdir -p "$proj"
log=$proj/import.log
: > "$log"
# The Ghidra of the locked nixpkgs, as in gdec.
rev=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["nodes"]["nixpkgs"]["locked"]["rev"])' "$here/../flake.lock")

gh() {
    if ! nix shell "github:NixOS/nixpkgs/$rev#ghidra" --command ghidra-analyzeHeadless "$proj" sp404 "$@" >> "$log" 2>&1; then
        echo "Ghidra failed; the end of $log:" >&2
        tail -n 20 "$log" >&2
        exit 1
    fi
}

echo "import: unpacking"
python3 "$here/unpack.py" >> "$log"
echo "import: SDRAM code at 0x80000000, with analysis"
gh -import "$here/regions/sdram_code.bin" -processor ARM:LE:32:Cortex \
   -loader BinaryLoader -loader-baseAddr 0x80000000
echo "import: ITCM code at 0x400, with analysis"
gh -import "$here/regions/itcm_code.bin" -processor ARM:LE:32:Cortex \
   -loader BinaryLoader -loader-baseAddr 0x400
echo "import: functions at every call target and code pointer"
nix develop "$here/.." --command python3 "$here/codeptrs.py" >> "$log" 2>&1
gh -process sdram_code.bin -noanalysis -scriptPath "$here/scripts" \
   -postScript MkMissing.java "$here/out/ptr_targets.txt"
echo "import: the tail-call functions"
gh -process sdram_code.bin -noanalysis -scriptPath "$here/scripts" \
   -postScript MkFns.java 80124f38 800ff060 80138180 801386d0 80141c98 80172188 \
   80101778 80111468 80114188 8012b9b8 80168ac0 80003342 8000327a
# The early finder scripts created these as they ran; without them the
# project falls nine functions short of the one the notes were written from.
gh -process sdram_code.bin -noanalysis -scriptPath "$here/scripts" \
   -postScript MkFns.java 80003380 80008ec8 80009490 80034d4c 80074500 800fea30 \
   800ffcb8 801016e0 80111458
grep -h -E 'orphan instructions|created from' "$log"
echo "import: done, $proj"
