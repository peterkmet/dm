import struct
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
D=os.path.join(ROOT, 'reference', 'extracted', 'snes'); r=open(D+'/us.sfc','rb').read()
def off(bank,addr): return bank*0x8000+(addr-0x8000)
SZ=0x83d2; TY=0x8940; N=(TY-SZ)//2
sizes=[struct.unpack_from('<H',r,SZ+2*i)[0] for i in range(N)]
types=[r[TY+i] for i in range(N)]
def items():
    bank,a=9,0x8000; out=[]
    for i in range(N):
        if a+sizes[i]>0x10000: bank+=1; a=0x8000
        out.append((off(bank,a),sizes[i],types[i])); a+=sizes[i]
    return out
if __name__=='__main__':
    it=items()
    print(N, it[-1], hex(it[-1][0]+it[-1][1]), len(r))
    from collections import Counter; print(Counter(types))
    for i in range(505,530): print(i,hex(it[i][0]),it[i][1],it[i][2])
def lz3(d):
    n=(d[2]<<8|d[3]); y=4; o=bytearray()
    while len(o)<n:
        b=d[y]
        if b&0x80:
            b2=d[y+1]; y+=2; cnt=(b>>1)&0x3f; dist=b2|(b&1)<<8
            for _ in range(cnt or 0x10000): o.append(o[-dist])
        else:
            y+=1; o+=d[y:y+b]; y+=b
    return bytes(o)
def get(i):
    o,s,t=items()[i]; d=r[o:o+s]
    if t==3: return lz3(d)
    if t==0: return d
    return None
