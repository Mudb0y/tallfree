"""Map decompressed output bytes back to the literal bytes that produced them,
so a same-length ASCII edit can be made directly in the compressed stream."""
import struct

FW='firmware/SP404MKII_APP1.bin'

def decompress_prov(src, outlen, src_base):
    """returns (data, {out_index: file_offset}) for literal bytes only"""
    o=bytearray(); prov={}; i=0
    while len(o)<outlen:
        tok=src[i]; i+=1
        lit=tok&7
        if lit==0: lit=src[i]; i+=1
        cpy=tok>>4
        if cpy==0: cpy=src[i]; i+=1
        n=lit-1
        for k in range(n):
            prov[len(o)]=src_base+i
            o.append(src[i]); i+=1
        if tok&8:
            off=src[i]; i+=1
            start=len(o)-off
            for k in range(cpy+2):
                o.append(o[start+k])
        else:
            o+=b'\x00'*cpy
    return bytes(o), prov

if __name__=='__main__':
    D=open(FW,'rb').read()
    data,prov=decompress_prov(D[0x20300:], 0x9180, 0x20300)
    # locate the iProduct string descriptor
    off=0x9140
    blen=data[off]
    s=data[off+2:off+blen].decode('utf-16-le')
    print('iProduct descriptor at DTCM 0x%08x, bLength=%d, value=%r'%(0x20000000+off,blen,s))
    print()
    print('provenance of each character (output offset -> file offset in APP1.bin):')
    ok=True
    for n,ch in enumerate(s):
        oi=off+2+n*2
        fo=prov.get(oi)
        print('   %2d %r  out 0x%04x  file %s'%(n,ch,oi,('0x%06x'%fo) if fo else 'NOT A LITERAL (back-reference)'))
        if fo is None: ok=False
    print()
    print('all characters individually patchable in place:', ok)
