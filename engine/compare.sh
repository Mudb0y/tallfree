#!/bin/sh
# compare.sh RAW TEXT: holds 16-bit samples from the target against what the
# desktop evv, built from the same OpenEVV tree, says for the same text.
set -e
raw=$1
text=$2
openevv=${OPENEVV:-/home/stas/Projects/openevv-sp404}
ref=$(mktemp --suffix=.wav)
trap 'rm -f "$ref"' EXIT

"$openevv/build/evv" -o "$ref" "$text"

python3 - "$raw" "$ref" <<'EOF'
import struct, sys
got = open(sys.argv[1], 'rb').read()
w = open(sys.argv[2], 'rb').read()
i = 12
while i < len(w):
    tag, n = w[i:i+4], struct.unpack_from('<I', w, i + 4)[0]
    if tag == b'data':
        want = w[i+8:i+8+n]
        break
    i += 8 + n + (n & 1)
g = struct.unpack('<%dh' % (len(got) // 2), got[:len(got) // 2 * 2])
r = struct.unpack('<%dh' % (len(want) // 2), want)
if g == r:
    print('match: %d samples, identical to the desktop engine' % len(r))
    sys.exit(0)
first = next((k for k in range(min(len(g), len(r))) if g[k] != r[k]), min(len(g), len(r)))
print('DIFFER: target %d samples, desktop %d, first difference at sample %d'
      % (len(g), len(r), first))
sys.exit(1)
EOF
