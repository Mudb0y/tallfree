# Image 31: Roland's APP1 but for room for the engine, whose compiled rules
# need 1 MB more than image 29 gave it, and the engine started at boot by a
# boot loader that, unlike image 30's, writes nothing to the card.
#
# The looper and skip-back buffer loses its top 4.75 MB: its end moves from
# 0x83F7A424 to 0x83ABA424. Every user computes its planes and lengths from
# (end - start) at run time, so moving the end in the seven places it is
# written shrinks everything consistently. With the 480 KB above it that
# nothing claims, the engine gets 0x83AC0000 to 0x83FF0000.
#
# Skip-back's normal capacity also changes from 5/7.25 of the buffer to all
# of it, by turning the 7.25 in both setup routines into 5.0.
#
# The main screen's status-line draw, vtable word 0x80226E98, points at the
# boot loader in flash, boot.c, which puts the word back on its first call
# and loads the engine from the card or the eMMC.
import struct, hashlib, os, subprocess
from patchtool import decompress_prov
import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'firmware', 'SP404MKII_APP1.bin')
BUILD = os.path.join(HERE, 'build')
OUT = os.path.join(BUILD, 'SP404MKII_APP1.bin')
os.makedirs(BUILD, exist_ok=True)
d = bytearray(open(SRC, 'rb').read()); orig = bytes(d)
FILE_BASE = 0x21ac0; MEM_BASE = 0x80000000
def f(a): return a - MEM_BASE + FILE_BASE
PAYLOAD_FLASH = 0x308000; payload_off = PAYLOAD_FLASH - 0x80000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)

def insn(a):
    return next(md.disasm(bytes(d[f(a):f(a) + 4]), a))

# The boot loader, built here so the image comes from one command. Run
# inside nix develop.
def build_boot():
    flags = ['-mcpu=cortex-m7', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv5-d16', '-Os',
             '-ffreestanding', '-fno-builtin', '-fno-delete-null-pointer-checks',
             '-ffunction-sections', '-Wall']
    subprocess.check_call(['arm-none-eabi-gcc'] + flags + ['-c', 'boot.c', '-o', 'build/boot.o'], cwd=HERE)
    subprocess.check_call(['arm-none-eabi-ld', '-T', 'boot.ld', 'build/boot.o', '-o', 'build/boot.elf'], cwd=HERE)
    subprocess.check_call(['arm-none-eabi-objcopy', '-O', 'binary', '-j', '.payload',
                           'build/boot.elf', 'build/boot.bin'], cwd=HERE)
    return open(os.path.join(BUILD, 'boot.bin'), 'rb').read()

# movt, Thumb-2 T1: 11110 i 101100 imm4 | 0 imm3 Rd imm8, imm16 = imm4:i:imm3:imm8
def movt_imm(h1, h2):
    return ((h1 & 0xF) << 12) | (((h1 >> 10) & 1) << 11) | (((h2 >> 12) & 7) << 8) | (h2 & 0xFF)

def movt_set(h1, h2, imm):
    h1 = (h1 & ~0x040F) | ((imm >> 12) & 0xF) | (((imm >> 11) & 1) << 10)
    h2 = (h2 & ~0x70FF) | (((imm >> 8) & 7) << 12) | (imm & 0xFF)
    return h1, h2

OLD_END_HI, NEW_END_HI = 0x83F7, 0x83AB
END_SITES = [0x8001C116, 0x8001C1B8, 0x800459CE, 0x800704A2, 0x800710DA, 0x80142692, 0x80142832]
for a in END_SITES:
    h1, h2 = struct.unpack_from('<HH', d, f(a))
    i = insn(a)
    assert i.mnemonic == 'movt' and movt_imm(h1, h2) == OLD_END_HI, (hex(a), i.mnemonic, i.op_str)
    h1, h2 = movt_set(h1, h2, NEW_END_HI)
    struct.pack_into('<HH', d, f(a), h1, h2)
    j = insn(a)
    assert j.mnemonic == 'movt' and j.op_str == i.op_str.replace('#0x83f7', '#0x83ab'), (hex(a), j.op_str)
    print('0x%08x  %s %s  ->  %s %s' % (a, i.mnemonic, i.op_str, j.mnemonic, j.op_str))
print('looper and skip-back buffer: end 0x83F7A424 -> 0x83ABA424, 7.25 MB -> 2.5 MB, 13.65 s a channel')

# The 7.25 in each skip-back setup routine. vmov.f32 immediate keeps its
# eight-bit float in imm4H (bits 16-19) and imm4L (bits 0-3): 7.25 is 0x1D
# and 5.0 is 0x14.
for start in (0x80142678, 0x80142818):
    found = 0
    for i in md.disasm(bytes(d[f(start):f(start) + 0x60]), start):
        if i.mnemonic == 'vmov.f32' and i.op_str.endswith('#7.250000e+00'):
            h1, h2 = struct.unpack_from('<HH', d, f(i.address))
            assert (h1 & 0xF) == 0x1 and (h2 & 0xF) == 0xD
            h1 = (h1 & ~0xF) | 0x1
            h2 = (h2 & ~0xF) | 0x4
            struct.pack_into('<HH', d, f(i.address), h1, h2)
            j = insn(i.address)
            assert j.op_str.endswith('#5.000000e+00'), j.op_str
            print('0x%08x  %s %s  ->  %s %s' % (i.address, i.mnemonic, i.op_str, j.mnemonic, j.op_str))
            found += 1
    assert found == 1, 'expected one vmov #7.25 in the routine at 0x%08x' % start
print('skip-back normal capacity: 5/7.25 of the buffer -> all of it')

# The boot hook: one vtable word, checked first.
STATUS_WORD, STATUS_DRAW, BOOT_ENTRY = 0x80226E98, 0x8014A019, 0x60308001
assert struct.unpack_from('<I', d, f(STATUS_WORD))[0] == STATUS_DRAW
struct.pack_into('<I', d, f(STATUS_WORD), BOOT_ENTRY)
print('status-line draw 0x%08x: 0x%08x -> 0x%08x, the boot loader' % (STATUS_WORD, STATUS_DRAW, BOOT_ENTRY))

pl = build_boot()
assert 0 < len(pl) < 0x20000
d += b'\xff' * (payload_off - len(d)) + pl
print('boot loader %d bytes at flash 0x%06x; image ends 0x%06x, limit 0x3ff000'
      % (len(pl), PAYLOAD_FLASH, 0x80000 + len(d)))
assert 0x80000 + len(d) <= 0x3ff000

data, _ = decompress_prov(bytes(d)[0x20300:], 0x9180, 0x20300); o = 0x9140
product = data[o + 2:o + data[o]].decode('utf-16-le')
print('USB product string: %r' % product)
assert product == 'Roland SP-404MKII', product

# Everything but the patched words and the appended payload is Roland's.
changed = [k for k in range(len(orig)) if d[k] != orig[k]]
print('bytes changed inside Roland\'s image: %d' % len(changed))
open(OUT, 'wb').write(bytes(d))
print(OUT)
print('sha256:', hashlib.sha256(bytes(d)).hexdigest())
