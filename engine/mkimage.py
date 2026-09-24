# mkimage.py ELF BIN: the loadable engine image, from __image_start to
# __image_end, with the header's CRC filled in over everything after it.
import struct, subprocess, sys, zlib
elf, out = sys.argv[1], sys.argv[2]
subprocess.check_call(['arm-none-eabi-objcopy', '-O', 'binary',
                       '--remove-section=.bss', '--remove-section=.tbss',
                       '--remove-section=.qemu_vectors', elf, out])
d = bytearray(open(out, 'rb').read())
magic, length, entry, _ = struct.unpack_from('<4I', d, 0)
assert magic == 0x31565645, 'no header at the start of the image'
assert length <= len(d), 'image shorter than its header says'
d = d[:length]
struct.pack_into('<I', d, 12, zlib.crc32(bytes(d[16:])) & 0xFFFFFFFF)
open(out, 'wb').write(d)
print('%s: %d bytes, entry 0x%08x' % (out, length, entry))
