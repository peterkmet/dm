import struct,wave,res
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
d=open(os.path.join(ROOT, 'reference', 'extracted', 'snes', 'driver.bin'),'rb').read()
T=[struct.unpack_from('<H',d,0x1b46-0x800+2*k)[0] for k in range(0x34)]
def brr(b):
    out=[];p1=p2=0
    for k in range(0,len(b)-8,9):
        h=b[k]; sh=h>>4; f=(h>>2)&3
        for j in range(16):
            n=b[k+1+j//2]; n=(n>>4) if j%2==0 else n&15
            if n>=8: n-=16
            s=(n<<sh)>>1 if sh<=12 else (-2048 if n<0 else 0)
            if f==1: s+=p1+((-p1)>>4)
            elif f==2: s+=(p1<<1)+((-3*p1)>>5)-p2+(p2>>4)
            elif f==3: s+=(p1<<1)+((-13*p1)>>6)-p2+((3*p2)>>4)
            s=max(-32768,min(32767,s)); s=((s<<1)&0xffff); s=s-65536 if s>=32768 else s; s>>=1  # 15-bit wrap like the DSP
            out.append(s); p2,p1=p1,s
        if h&1: break
    return out
def sample(sid):
    i=0x247+(sid&0x7f); d=res.get(i)
    if len(d)<20: return None
    tun=d[7]<<8|d[8]; loop=d[9]|d[10]<<8; lf=d[11]
    return dict(item=i,adsr=d[4:6].hex(),tuning=tun,loop=loop,looped=bool(lf&2),pcm=brr(d[20:]))
def rate(note,tun): return 32000*(T[note&0x7f]*tun>>8)/4096
def write(path,pcm,hz):
    w=wave.open(path,'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(int(round(hz)))
    w.writeframes(struct.pack('<%dh'%len(pcm),*[max(-32768,min(32767,2*v)) for v in pcm])); w.close()
if __name__=='__main__':
    print(T)
    for sid in range(0x90,0xc9):
        s=sample(sid)
        if s: print(hex(sid),s['item'],s['tuning'],len(s['pcm']),'peak',max(abs(v) for v in s['pcm']),'rate@a1 %.0f'%rate(0xa1,s['tuning']))
