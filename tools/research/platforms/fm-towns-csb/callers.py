import os, sys, struct
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
d = open(ROOT + 'reference/extracted/fm-towns-csb/files/' + sys.argv[1], 'rb').read()[0x200:]
for t in sys.argv[2:]:
    t = int(t, 16)
    print(hex(t), [hex(i) for i in range(len(d) - 5) if d[i] == 0xe8 and (i + 5 + struct.unpack_from('<i', d, i + 1)[0]) == t])
