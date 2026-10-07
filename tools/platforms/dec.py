import struct
def nibbles(b,hi_first=True):
    for x in b:
        if hi_first: yield x>>4; yield x&15
        else: yield x&15; yield x>>4
def snd1(item,hi=True):
    n=struct.unpack('>H',item[:2])[0]; out=[]; it=nibbles(item[2:],hi); last=0
    try:
        while len(out)<n:
            v=next(it)
            if v: last=v; out.append(v)
            else:
                c=0
                while True:
                    m=next(it); c=(c<<3)|(m&7)
                    if not m&8 or c>1<<20: break
                out+= [last]*min(c+3,n)
    except StopIteration: pass
    return bytes(min(255,v*17) for v in out[:n]), n, len(out)
STEPS=[16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552]
ADJ=[-1,-1,-1,-1,2,4,6,8]
def oki(data,hi=False):
    s=0;i=0;out=[]
    for n in nibbles(data,hi):
        st=STEPS[i]; d=st>>3
        if n&1: d+=st>>2
        if n&2: d+=st>>1
        if n&4: d+=st
        s = s-d if n&8 else s+d
        s=max(-2048,min(2047,s)); i=max(0,min(48,i+ADJ[n&7])); out.append(s)
    return out  # 12-bit signed
def snd8(item):
    n=struct.unpack('>H',item[:2])[0]; out=[]; s=0; it=nibbles(item[2:],True)
    try:
        while len(out)<n:
            v=next(it); v=v-16 if v>7 else v
            if v==-8:
                d=next(it)*16+next(it); d=d-256 if d>127 else d
            else: d=v
            s=(s+d)&255 if False else s+d
            out.append(s)
    except StopIteration: pass
    return out,n
