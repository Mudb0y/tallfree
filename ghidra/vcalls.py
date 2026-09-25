import capstone,os,sys
HERE=os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
d=open(os.path.join(HERE,'regions','sdram_code.bin'),'rb').read(); B=0x80000000
md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB|capstone.CS_MODE_MCLASS); md.skipdata=True
offs={0x12c:'drawstr',0x134:'drawchar',0x128:'fwd128',0x140:'fwd140'}
hits={k:[] for k in offs}
win=[]
for i in md.disasm(d,B):
    win.append(i); win=win[-8:]
    if i.mnemonic=='blx':
        r=i.op_str
        for p in win[:-1]:
            if p.mnemonic.startswith('ldr') and p.op_str.startswith(r+',') :
                for k in offs:
                    if ('#0x%x]'%k) in p.op_str: hits[k].append(i.address)
for k,v in hits.items():
    print(offs[k], len(v))
    open(os.path.join(HERE,'out','vcall_%x.txt'%k),'w').write('\n'.join('%x'%a for a in v))
