import os, sys
import capstone,struct
HERE=os.path.dirname(os.path.abspath(__file__))
d=open(os.path.join(HERE,'regions','sdram_code.bin'),'rb').read()
N=len(d); B=0x80000000
md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB|capstone.CS_MODE_MCLASS)
md.skipdata=True
bl=set()
for i in md.disasm(d, B):
    if i.mnemonic=='bl':
        t=int(i.op_str.lstrip('#'),16)
        if B<=t<B+N: bl.add(t)
ptr=set()
for o in range(0,N-3,4):
    v=struct.unpack_from('<I',d,o)[0]
    if v&1 and B<=v<B+N:
        ptr.add(v&~1)
print(len(bl),len(ptr),file=sys.stderr)
open(os.path.join(HERE,'out','ptr_targets.txt'),'w').write('\n'.join('%x'%t for t in sorted(ptr)))
