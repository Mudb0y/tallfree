import os, struct, sys
HERE=os.path.dirname(os.path.abspath(__file__))
D=open(os.path.join(HERE,'..','firmware','SP404MKII_APP1.bin'),'rb').read()
OUT=os.path.join(HERE,'regions')
os.makedirs(OUT,exist_ok=True)
FLASH=0x60080000
def decompress(src, outlen):
    o=bytearray(); i=0
    while len(o)<outlen:
        tok=src[i]; i+=1
        lit=tok&7
        if lit==0: lit=src[i]; i+=1
        cpy=tok>>4
        if cpy==0: cpy=src[i]; i+=1
        n=lit-1
        if n>0:
            o+=src[i:i+n]; i+=n
        if tok&8:
            off=src[i]; i+=1
            cnt=cpy+2
            start=len(o)-off
            for k in range(cnt): o.append(o[start+k])
        else:
            o+=b'\x00'*cpy
    return bytes(o), i
ENTRIES=[(0x20300,0x20000000,0x9180,'DTCM data'),
         (0x20840,0x2023f000,0x7a250,'OCRAM data'),
         (0x267200,0x80245740,0xa20c0,'SDRAM data'),
         (0x287200,0x83ff0000,0x1800,'SDRAM high data')]
out={}
for foff,dst,size,name in ENTRIES:
    try:
        data,used=decompress(D[foff:], size)
        ok = len(data)>=size
        data=data[:size]
        print('%-16s src file 0x%06x -> 0x%08x  %d bytes out, %d bytes of input consumed  %s'
              %(name,foff,dst,len(data),used,'OK' if ok else 'SHORT'))
        out[dst]=(data,name)
        open(os.path.join(OUT,'seg_%08x.bin'%dst),'wb').write(data)
    except Exception as e:
        print('%-16s FAILED: %s'%(name,e))
# The regions the scatter table copies plainly, which are what Ghidra imports.
for foff,dst,size,name in [(0x6ec,0x00000000,0x400,'vectors'),
                           (0xaf0,0x00000400,0x1f800,'itcm_code'),
                           (0x21ac0,0x80000000,0x245740,'sdram_code')]:
    open(os.path.join(OUT,name+'.bin'),'wb').write(D[foff:foff+size])
    print('%-16s src file 0x%06x -> 0x%08x  %d bytes, copied'%(name,foff,dst,size))
