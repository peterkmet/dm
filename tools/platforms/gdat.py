import struct,sys
def parse(path):
    b=open(path,'rb').read()
    sig_be=struct.unpack('>H',b[:2])[0]; sig_le=struct.unpack('<H',b[:2])[0]
    if sig_be==0x8001: e='>'; fmt=2
    elif sig_le==0x8001: e='<'; fmt=2
    else:
        fmt=1; e='>' if struct.unpack('>H',b[:2])[0]<4096 else '<'
    if fmt==2:
        n=struct.unpack(e+'H',b[2:4])[0]; p=4
    else:
        n=struct.unpack(e+'H',b[:2])[0]; p=2
    comp=struct.unpack(e+'%dH'%n,b[p:p+2*n]); dec=struct.unpack(e+'%dH'%n,b[p+2*n:p+4*n])
    attrs=None
    off=p+4*n
    if fmt==2:
        attrs=[struct.unpack(e+'HH',b[off+4*i:off+4*i+4]) for i in range(n)]; off+=4*n
    items=[]
    for i in range(n):
        items.append(b[off:off+comp[i]]); off+=comp[i]
    return dict(e=e,fmt=fmt,n=n,comp=comp,dec=dec,attrs=attrs,items=items,end=off,size=len(b))
if __name__=='__main__':
    g=parse(sys.argv[1]); print(g['fmt'],g['e'],g['n'],g['end'],g['size'])
    lo,hi=int(sys.argv[2]),int(sys.argv[3])
    for i in range(lo,min(hi,g['n'])):
        it=g['items'][i]
        w=struct.unpack('>H',it[:2])[0] if len(it)>=2 else None
        print(i,g['comp'][i],g['dec'][i],g['attrs'][i] if g['attrs'] else '',w,it[:8].hex())
