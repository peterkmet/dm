import struct,sys,os
def extract(img,out,be_bpb=False):
    d=open(img,'rb').read()
    e='>' if be_bpb else '<'
    o=0x12 if be_bpb else 0x0b
    if be_bpb:
        bps,spc,nf,res,root,tot,med,spf=struct.unpack('>HBBHHHBB',d[o:o+12])
    else:
        bps,spc,res,nf,root,tot,med,spf=struct.unpack('<HBHBHHBH',d[o:o+13])
    fat_off=res*bps; root_off=fat_off+nf*spf*bps; data_off=root_off+root*32
    fat=d[fat_off:fat_off+spf*bps]
    def nxt(c):
        v=struct.unpack('<H',fat[c*3//2:c*3//2+2])[0]
        return v>>4 if c&1 else v&0xfff
    def chain(c):
        r=[]
        while 2<=c<0xff0 and len(r)<5000: r.append(c); c=nxt(c)
        return r
    def read(c,size=None):
        b=b''.join(d[data_off+(x-2)*spc*bps:data_off+(x-1)*spc*bps] for x in chain(c))
        return b if size is None else b[:size]
    def walk(raw,path):
        for i in range(0,len(raw),32):
            ent=raw[i:i+32]
            if ent[0]==0: break
            if ent[0]==0xe5 or ent[11]&8: continue
            name=ent[:8].rstrip(b' \0')+ (ent[12:22].rstrip(b'\0 ') if be_bpb else b'')
            ext=ent[8:11].rstrip(b' ')
            n=name.decode('latin1')+('.'+ext.decode('latin1') if ext else '')
            if n in('.','..'): continue
            cl=struct.unpack('<H',ent[26:28])[0]; size=struct.unpack('<I',ent[28:32])[0]
            p=os.path.join(path,n)
            if ent[11]&0x10:
                os.makedirs(p,exist_ok=True); walk(read(cl),p)
            else:
                open(p,'wb').write(read(cl,size)); print(p,size)
    os.makedirs(out,exist_ok=True)
    walk(d[root_off:data_off],out)
extract(sys.argv[1],sys.argv[2],sys.argv[3]=='be' if len(sys.argv)>3 else False)
