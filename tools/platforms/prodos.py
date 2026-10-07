import sys,os,struct
d=open(sys.argv[1],'rb').read(); out=sys.argv[2]
blk=lambda n:d[n*512:(n+1)*512]
def data(st,key,eof):
    if st==1: b=blk(key)
    elif st==2:
        ix=blk(key); b=b''.join(blk(ix[i]|ix[256+i]<<8) for i in range(256))
    elif st==3:
        mi=blk(key); b=b''
        for j in range(128):
            p=mi[j]|mi[256+j]<<8
            if not p: break
            ix=blk(p); b+=b''.join(blk(ix[i]|ix[256+i]<<8) for i in range(256))
    elif st==5:  # extended (forked) file: data fork only
        e=blk(key); return data(e[0]&15, struct.unpack('<H',e[1:3])[0], e[5]|e[6]<<8|e[7]<<16)
    else: return b''
    return b[:eof]
def walk(key,path):
    os.makedirs(path,exist_ok=True); first=True
    while key:
        b=blk(key); nxt=struct.unpack('<H',b[2:4])[0]
        for i in range(13):
            if first and i==0: continue
            e=b[4+39*i:4+39*(i+1)]
            st=e[0]>>4; nl=e[0]&15
            if not st: continue
            name=e[1:1+nl].decode(); ft=e[0x10]; k=struct.unpack('<H',e[0x11:0x13])[0]; eof=e[0x15]|e[0x16]<<8|e[0x17]<<16
            p=os.path.join(path,name)
            if st==0xd: walk(k,p)
            else:
                open(p,'wb').write(data(st,k,eof)); print(p,eof,'type %02x'%ft,'st',st)
        first=False; key=nxt
        first=False
walk(2,out)
