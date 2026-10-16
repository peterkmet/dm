import capstone as c, sys
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
r=open(os.path.join(ROOT, 'reference', 'extracted', 'snes', 'us.sfc'),'rb').read()
mode={'16':c.CS_MODE_MOS65XX_65816_LONG_MX,'8':c.CS_MODE_MOS65XX_65816,'m8x16':c.CS_MODE_MOS65XX_65816_LONG_X,'m16x8':c.CS_MODE_MOS65XX_65816_LONG_M}[sys.argv[3] if len(sys.argv)>3 else '16']
md=c.Cs(c.CS_ARCH_MOS65XX,mode)
o=int(sys.argv[1],16); n=int(sys.argv[2],16)
bank=o//0x8000
for i in md.disasm(r[o:o+n], 0x8000+(o%0x8000)):
    print('%02x:%04x  %-12s %s %s'%(bank,i.address,i.bytes.hex(),i.mnemonic,i.op_str))
