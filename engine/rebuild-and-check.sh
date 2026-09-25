#!/bin/sh
# Rebuilds everything from clean, runs the QEMU cases against the desktop
# engine, then OpenEVV's own matrix. Written after a power cut left build
# outputs that could not be trusted.
set -e
here=$(cd "$(dirname "$0")" && pwd)
OPENEVV=${OPENEVV:-$here/openevv}
arm="nix develop $here/.. --command"

echo "== clean"
rm -rf "$OPENEVV/build" "$here/build"
echo "== rulecode"
(cd "$OPENEVV" && nix develop --command make rulecode)
echo "== arm library and qemu image"
(cd "$here" && $arm make lib qemu)
echo "== qemu cases"
(cd "$here" && $arm ./cases.sh run)
echo "== desktop reference"
(cd "$OPENEVV" && nix develop --command make -j24 RULES=bytecode build/evv)
echo "== compare"
(cd "$here" && $arm ./cases.sh compare) || echo "COMPARE FAILED"
echo "== matrix, compiled rules"
(cd "$OPENEVV" && nix develop --command make matrix) || echo "MATRIX C FAILED"
echo "== matrix, bytecode"
(cd "$OPENEVV" && nix develop --command make matrix RULES=bytecode) || echo "MATRIX BYTECODE FAILED"
echo "== done"
