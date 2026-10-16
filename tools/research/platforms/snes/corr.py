import numpy as np, sys, brr
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '')  # the repository root
sys.path.insert(0, os.path.join(ROOT, 'tools', 'platforms')); import gdat
am=gdat.parse(ROOT + 'game/Graphics.DAT')['items']
SND=[g for g in list(range(671,694))+list(range(701,713)) if g not in(676,686)]
A={g:np.frombuffer(am[g][2:],dtype=np.int8).astype(float) for g in SND}
def env(x,n=200):
    x=np.convolve(np.abs(x),np.ones(max(1,len(x)//100))/max(1,len(x)//100),'same'); x=np.interp(np.linspace(0,len(x)-1,n),np.arange(len(x)),x); return (x-x.mean())/(x.std()+1e-9)
res={}
for sid in range(0x90,0xc9):
    s=brr.sample(sid)
    if not s or len(s['pcm'])<1000: continue
    x=np.array(s['pcm'],float)
    best=[]
    for g,a in A.items():
        # trim trailing silence
        def trim(v):
            nz=np.nonzero(np.abs(v)>np.abs(v).max()*0.02)[0]; return v[nz[0]:nz[-1]+1]
        xs,as_=trim(x),trim(a)
        c=float(np.dot(env(xs),env(as_))/200)
        best.append((c,g,len(xs)/len(as_)))
    best.sort(reverse=True)
    print(hex(sid),len(x),' '.join('%d:%.2f(x%.2f)'%(g,c,r) for c,g,r in best[:3]))
