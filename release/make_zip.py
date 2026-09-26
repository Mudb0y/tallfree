# The release: one zip laid out as the SD card, so installing is unzipping it
# onto a freshly formatted card and running the SHIFT-plus-power update.
#
#   SP404MKII_APP0.bin     Roland's own, untouched: the companion chip's half
#                          of the same system program, so the update brings
#                          the whole unit to 5.52 as Roland's update would
#   SP404MKII_APP1.bin     Tallfree's image, from image/make_image.py
#   TALLFREE/TALLFREE.BIN  the engine, from engine/
#
# Builds both first, from the tree as it is. Takes APP0 and the APP1 the image
# is made from out of firmware/, which is not in the repo, and checks them
# against Roland's checksums. The version is the date, yyyy.mm.dd, today's
# unless given; every file in the zip carries that date, so the same tree
# gives the same zip. Run inside nix develop.
#
#   python3 release/make_zip.py [yyyy.mm.dd]
import datetime, hashlib, os, subprocess, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.join(HERE, '..')
APP0_SHA = '3e35ff30137f741a715566e205384f9b434ca7ac3847e247bad3f0c9c8a2bd23'
APP1_SHA = '4a3d67711e14dcc97d50249a4eee7dd6df0251a2f37757cbbe2556c233730d80'

def sha(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()

version = sys.argv[1] if len(sys.argv) > 1 else datetime.date.today().strftime('%Y.%m.%d')
stamp = tuple(int(x) for x in version.split('.')[:3]) + (0, 0, 0)

for name, want in (('SP404MKII_APP0.bin', APP0_SHA), ('SP404MKII_APP1.bin', APP1_SHA)):
    got = sha(os.path.join(TOP, 'firmware', name))
    if got != want:
        sys.exit('firmware/%s is not Roland\'s 5.52: sha256 %s' % (name, got))

subprocess.check_call([sys.executable, 'make_image.py'], cwd=os.path.join(TOP, 'image'),
                      stdout=subprocess.DEVNULL)
subprocess.check_call(['make', '-s', 'device'], cwd=os.path.join(TOP, 'engine'),
                      stdout=subprocess.DEVNULL)

files = [('SP404MKII_APP0.bin', os.path.join(TOP, 'firmware', 'SP404MKII_APP0.bin')),
         ('SP404MKII_APP1.bin', os.path.join(TOP, 'image', 'build', 'SP404MKII_APP1.bin')),
         ('TALLFREE/TALLFREE.BIN', os.path.join(TOP, 'engine', 'build', 'TALLFREE.BIN'))]
os.makedirs(os.path.join(HERE, 'build'), exist_ok=True)
out = os.path.join(HERE, 'build', 'tallfree-%s.zip' % version)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    folder = zipfile.ZipInfo('TALLFREE/', stamp)
    folder.external_attr = 0o40755 << 16
    z.writestr(folder, b'')
    for arc, path in files:
        info = zipfile.ZipInfo(arc, stamp)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        z.writestr(info, open(path, 'rb').read())
for arc, path in files:
    print('%-22s %s' % (arc, sha(path)))
print('%s  %s' % (os.path.relpath(out, TOP), sha(out)))
