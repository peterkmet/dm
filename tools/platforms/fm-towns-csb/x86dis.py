import os, sys, capstone
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
d = open(ROOT + 'reference/extracted/fm-towns-csb/files/' + sys.argv[1], 'rb').read()[0x200:]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
a, b = int(sys.argv[2], 16), int(sys.argv[3], 16)
for i in md.disasm(d[a:b], a):
    print(f"{i.address:06x}: {i.mnemonic} {i.op_str}")
