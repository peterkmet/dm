import sys
for f in sys.argv[1:]:
    d = open(f, 'rb').read(); p = 0; out = []
    while p + 6 <= len(d):
        tag = d[p:p + 2].decode('latin1'); size = int.from_bytes(d[p + 2:p + 4], 'big'); par = int.from_bytes(d[p + 4:p + 6], 'big')
        out.append((p, tag, size, par)); p += size + 6
        if size == 0 and tag.strip('\0') == '': break
    print(f, len(out), 'chunks')
    for o in out:
        if o[1] in ('TD', 'TR', 'WA', 'EN', 'MI', 'DS', 'DM', 'SO', 'FO', 'LP', 'LD', 'NE'): print('  ', hex(o[0]), o[1], o[2], o[3])
