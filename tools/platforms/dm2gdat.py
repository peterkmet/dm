# Reads the DM2 (Skullkeep, DOS) GRAPHICS.DAT: key table and items (docs: dm2/docs/skull-formats.md).
import os, struct
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '')  # the repository root
PATH = ROOT + '../dm2/game/DATA/GRAPHICS.DAT'  # the DM2 repository next to this one

def load(path=PATH):
    f = open(path, 'rb').read()
    assert struct.unpack_from('<H', f)[0] == 0x8005
    n = struct.unpack_from('<H', f, 2)[0]
    sizes = [struct.unpack_from('<I', f, 4)[0]] + list(struct.unpack_from('<%dH' % (n - 1), f, 8))
    off = 8 + 2 * (n - 1); offs = []
    for s in sizes: offs.append(off); off += s
    assert off == len(f)
    k = f[offs[0]:]
    rows = struct.unpack_from('>H', k, 2)[0]; base = 6 + 7 * 2
    keys = {}  # (cat, idx, type, entry) -> item or value; English and all-language rows only
    for i in range(rows):
        c, x, t, e, lang, _, v = struct.unpack_from('>5BBH', k, base + 8 * i)
        if lang & 0xf0 not in (0, 0x10): continue
        keys[(c, x, t, e)] = v if t in (0x0b, 0x0c) else v & 0x7fff
    item = lambda i: f[offs[i]:offs[i] + sizes[i]]
    enc = keys.get((0, 0, 0x0b, 0), 0) & 0x08
    def text(c, x, e):
        i = keys.get((c, x, 5, e))
        if i is None: return None
        b = item(i); out = []
        for n_, ch in enumerate(b):
            ch = ((ch ^ 0xff) - n_) & 0xff if enc else ch
            if not ch: break
            out.append(ch)
        return bytes(out).decode('latin-1')
    return keys, item, text

def dev_labels(path=PATH):
    """Developer labels (language 0xf, never installed): (category, index) -> name; names the creatures."""
    f = open(path, 'rb').read()
    n = struct.unpack_from('<H', f, 2)[0]
    sizes = [struct.unpack_from('<I', f, 4)[0]] + list(struct.unpack_from('<%dH' % (n - 1), f, 8))
    off = 8 + 2 * (n - 1); offs = []
    for s in sizes: offs.append(off); off += s
    k = f[offs[0]:]; out = {}
    for i in range(struct.unpack_from('>H', k, 2)[0]):
        c, x, t, e, lang, _, v = struct.unpack_from('>5BBH', k, 20 + 8 * i)
        if lang & 0xf0 == 0xf0 and t == 5 and e == 0:
            b = f[offs[v & 0x7fff]:offs[v & 0x7fff] + sizes[v & 0x7fff]]
            out[(c, x)] = bytes(((ch ^ 0xff) - j) & 0xff for j, ch in enumerate(b)).split(b'\0')[0].decode('latin-1').title()
    return out
