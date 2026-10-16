import re,capstone as c
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
r=open(os.path.join(ROOT, 'reference', 'extracted', 'snes', 'us.sfc'),'rb').read()
md=c.Cs(c.CS_ARCH_MOS65XX,c.CS_MODE_MOS65XX_65816_LONG_MX)
for m in re.finditer(re.escape(bytes.fromhex('22d1d900')),r):
    o=m.start(); s=o-40
    ins=list(md.disasm(r[s:o+4],0x8000+(s%0x8000)))
    print('--- %02x:%04x'%(o//0x8000,0x8000+o%0x8000))
    for i in ins[-12:]: print('   ',i.mnemonic,i.op_str)
