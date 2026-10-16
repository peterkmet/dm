"""Match the functions of the CSB engine (game-csb/APPB.FTL) to DM KAOS.FTL.

Usage: uv run python tools/csb_match.py [--csv OUT] [--report OUT] [--diff KAOS_ADDR]

The two modules come from different compilers: CSB keeps locals on the stack,
DM keeps them in registers. A byte or instruction compare does not work, so each
function becomes code-generation independent features (Musashi disassembly):

- calls: ordered call targets (jump table and BSR/JSR)
- consts: ordered immediates of compare, logic, arithmetic and move instructions
- args: stack argument offsets that the function reads
- globals: A5 offsets that the function reads or writes (directly or as a table base)
- branches: count of conditional branches

Matching: global alignment of the two function lists (the linkers keep the source
file order), then call-graph propagation. Classes per pair:

- identical: same mapped calls, same set of non-zero consts, same args
- constants: same mapped calls; the consts or the args differ
- logic: the mapped calls differ (runtime helper calls are ignored)
- csb_only, dm_only: no partner

The classes are a heuristic. A "logic" pair can still be the same source code.
"""

import argparse
import csv
import difflib
import re
from collections import Counter, defaultdict

import machine68k

from dmtools import ftl

KAOS_FTL, KAOS_CSV = "game/KAOS.FTL", "docs/reference/kaos-symbols.csv"
CSB_FTL, CSB_C = "game-csb/APPB.FTL", "reference/decomp/csb-a/APPB.c"
BASE = 0x100000

A5_RE = re.compile(r"\((-?)\$([0-9a-f]+),A5\)")
AN_RE = re.compile(r"\(-\$([0-9a-f]+),A[0-4]\)")
ABS_RE = re.compile(r"\$([0-9a-f]{5,8})\b")
ARG_RE = re.compile(r"\(\$([0-9a-f]+),A6\)")
IMM_RE = re.compile(r"#(-?)\$?([0-9a-f]+)")
STACK_RE = re.compile(r"^(addq|subq|add|sub|lea)\S* .*A7$")
COND_RE = re.compile(r"^(b(?!ra|sr)|db)")


class Module:
    def __init__(self, path):
        mod = ftl.parse(open(path, "rb").read())
        img = ftl.load_image(mod, BASE)
        self.mem, self.db, self.cb = img.data, img.data_base, img.code_base
        self.end = BASE + len(img.data)
        self.jt = (img.data_base + mod.jt_offset, img.data_base + mod.jt_offset + 8 * len(img.entries))
        self.entries = img.entries
        self.cpu = machine68k.CPU(machine68k.CPUType.M68000)

    def jt_target(self, addr):
        if self.jt[0] <= addr < self.jt[1]:
            return self.entries[(addr - self.jt[0]) // 8]
        return None

    def disasm(self, start, end):
        out, pc = [], start
        while pc < end:
            n, text = self.cpu.disassemble_raw(pc, self.mem[pc - BASE : pc - BASE + 16])
            if n <= 0:
                break
            out.append((pc, n, " ".join(text.split())))
            pc += n
        return out


def kaos_names(m):
    return {int(r["address"], 16): r for r in csv.DictReader(open(KAOS_CSV)) if int(r["address"], 16) >= m.cb and r["signature"]}


def csb_starts(m):
    """Ghidra function starts, exports, LINK after RTS, and direct call targets."""
    starts = {int(x, 16) for x in re.findall(r"^/\* ([0-9a-f]{8}) \*/$", open(CSB_C).read(), re.M)}
    starts = {a for a in starts if m.cb <= a < m.end} | set(m.entries)
    for p in range(m.cb - BASE, len(m.mem) - 4, 2):
        if m.mem[p : p + 2] in (b"\x4e\x56", b"\x4e\x55") and m.mem[p - 2 : p] in (b"\x4e\x75", b"\x4e\x73"):
            starts.add(BASE + p)
    while True:
        s, new = sorted(starts), set()
        for i, a in enumerate(s):
            for _, _, t in m.disasm(a, s[i + 1] if i + 1 < len(s) else m.end):
                if t.startswith(("bsr", "jsr")):
                    new |= {int(x, 16) for x in ABS_RE.findall(t) if m.cb <= int(x, 16) < m.end} - starts
        if not new:
            return starts
        starts |= new


class Func:
    def __init__(self, m, start, end):
        self.start, self.end = start, end
        self.calls, self.consts, self.globs, self.args = [], [], [], set()
        self.branches = self.size = 0
        for pc, n, t in m.disasm(start, end):
            self.size += 1
            op = t.split()[0]
            if op.startswith(("link", "movem", "unlk")) or STACK_RE.match(t):
                continue
            for neg, off in A5_RE.findall(t):
                addr = m.db + int(off, 16) * (-1 if neg else 1)
                tgt = m.jt_target(addr)
                (self.calls.append(tgt) if tgt is not None else self.globs.append(addr))
            if op.startswith(("bsr", "jsr", "jmp")) and "A5" not in t:
                self.calls += [int(x, 16) for x in ABS_RE.findall(t) if not start <= int(x, 16) < end]
            if COND_RE.match(op):
                self.branches += 1
            self.globs += [m.db - int(x, 16) for x in AN_RE.findall(t) if int(x, 16) >= 0x80]
            self.args |= {int(x, 16) for x in ARG_RE.findall(t) if int(x, 16) >= 8}
            vals = [-int(v, 16) if neg else int(v, 16) for neg, v in IMM_RE.findall(t)]
            if op == "pea" and "(" not in t:
                vals.append(int(t.split("$")[1].split(".")[0], 16))
            for v in vals:
                # Compare 16-bit words: DM pushes two words as one long, and the sign extension differs.
                v = v & 0xFFFF if -0x8000 <= v < 0 else v & 0xFFFFFFFF
                self.consts += [v >> 16, v & 0xFFFF] if v > 0xFFFF else [v]
        self.calls = [c for c in self.calls if c is not None]


def build(m, starts):
    s = sorted(starts)
    return {a: Func(m, a, s[i + 1] if i + 1 < len(s) else m.end) for i, a in enumerate(s)}


def seq_ratio(a, b):
    if not a and not b:
        return 1.0
    return difflib.SequenceMatcher(None, a, b, autojunk=False).ratio()


def num_sim(x, y):
    return 1.0 if x == y else min(x, y) / max(x, y)


def sim(a, b, pairs=None):
    calls = seq_ratio([pairs.get(c, -c) for c in a.calls], b.calls) if pairs else num_sim(len(a.calls) + 1, len(b.calls) + 1)
    return (0.3 * seq_ratio(a.consts, b.consts) + 0.3 * calls + 0.15 * (a.args == b.args)
            + 0.15 * num_sim(a.branches + 1, b.branches + 1) + 0.1 * num_sim(a.size, b.size))


def align(kf, cf, pairs=None):
    """Needleman-Wunsch over the address-ordered function lists."""
    ks, cs = sorted(kf), sorted(cf)
    n, m = len(ks), len(cs)
    score = [[0.0] * (m + 1) for _ in range(n + 1)]
    back = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        back[i][0] = 1
    for j in range(1, m + 1):
        back[0][j] = 2
    for i in range(1, n + 1):
        fa, row, prev = kf[ks[i - 1]], score[i], score[i - 1]
        for j in range(1, m + 1):
            d = prev[j - 1] + sim(fa, cf[cs[j - 1]], pairs) - 0.5
            u, l = prev[j], row[j - 1]
            if d >= u and d >= l:
                row[j], back[i][j] = d, 0
            elif u >= l:
                row[j], back[i][j] = u, 1
            else:
                row[j], back[i][j] = l, 2
    out, i, j = {}, n, m
    while i > 0 or j > 0:
        b = back[i][j]
        if b == 0:
            out[ks[i - 1]] = (cs[j - 1], sim(kf[ks[i - 1]], cf[cs[j - 1]], pairs))
            i, j = i - 1, j - 1
        elif b == 1:
            i -= 1
        else:
            j -= 1
    return out


# Pairs found by reading the code (CSB address -> KAOS name). The matcher misses them
# because the CSB code is longer or the neighbours shifted.
MANUAL_PAIRS = {
    0x10be12: "rt_longjmp",
    0x1286f2: "save_file_write",
    0x128720: "save_block_xor",
    0x128752: "save_block_checksum",
    0x1287c2: "save_write_checked_block",
    0x128810: "save_read_summed_block",
    0x128856: "save_write_summed_block",
    0x12889c: "build_save_prompt",
    0x1289c4: "save_read_header",
    0x128a42: "save_write_header",
    0x127ae2: "dialog_get_choice",
    0x127e28: "dialog_draw",
    0x12a116: "entrance_draw_dungeon_view",
    0x12a1b6: "entrance_blit_work_bitmap",
    0x12a1cc: "entrance_open_doors",
    0x12a320: "entrance_draw",
    0x12a44e: "process_entrance",
    0x12a672: "entrance_show_credits",
    0x12a8d0: "decompress_dungeon",
    0x12a92a: "decompress_get_bits",
    0x12aa18: "graphic_get_bitmap_byte_count",
    0x12aa4c: "layout_alloc_graphic_bitmap",
    0x12aa8c: "layout_share_bitmap",
    0x12aaaa: "init_graphic_data",
    0x12b44a: "init_flipped_wall_bitmaps",
    0x12b8f4: "os_cd_input_handler",
    0x12b916: "os_raw_key_convert",
    0x127abe: "dialog_draw_viewport",
    0x118b54: "find_video_driver",
}

# CSB functions without a DM partner: (name, subsystem). "protect" = copy protection.
CSB_NAMES = {
    0x1097e6: ("protect_disk_access", "protect"),
    0x10b32e: ("protect_check_10b32e", "protect"),
    0x10c2b0: ("protect_check_10c2b0", "protect"),
    0x10c37c: ("make_jmp_stub", "protect"),
    0x113f22: ("protect_check_113f22", "protect"),
    0x11af34: ("protect_counter_11af34", "protect"),
    0x11e202: ("protect_trampoline", "protect"),
    0x11e248: ("protect_install_hook", "protect"),
    0x120b4e: ("protect_checksum_120b4e", "protect"),
    0x1244f8: ("protect_compare_track_bits", "protect"),
    0x1245b4: ("protect_checksum_1245b4", "protect"),
    0x128676: ("protect_checksum_128676", "protect"),
    0x124d10: ("protect_dummy_124d10", "protect"),
    0x125180: ("protect_dummy_125180", "protect"),
    0x128656: ("protect_dummy_128656", "protect"),
    0x128ca0: ("max16_local", "math"),
    0x129580: ("min16_local", "math"),
    0x111080: ("is_magic_map_in_hand", "magic_map"),
    0x1110e0: ("magic_map_draw_cell", "magic_map"),
    0x111140: ("magic_map_draw_square", "magic_map"),
    0x1116a0: ("scroll_text_insert_creature_name", "magic_map"),
    0x1285ee: ("quit_game_to_ending", "startend"),
    0x12a6e2: ("show_title", "startend"),
    0x12b532: ("compute_graphics_memory", "memory"),
    0x128b12: ("save_disk_prompt_dialog", "loadsave"),
    0x10653d: ("data_in_code_10653d", "data"),
    0x106592: ("alert_stub_106592", "system"),
    0x106595: ("alert_stub_106595", "system"),
    0x1065a8: ("show_alert_decimal_csb", "system"),
    0x1076fe: ("str_helper_1076fe", "string"),
    0x107752: ("str_helper_107752", "string"),
    0x1091ce: ("sound_load_samples", "sound"),
    0x109d90: ("text_draw_glyph", "text"),
    0x10a756: ("vblank_update_display", "interrupt"),
    0x10a920: ("vblank_set_colors", "interrupt"),
    0x10b4da: ("display_init_view", "video"),
    0x10b534: ("display_load_view", "video"),
    0x10b5d8: ("display_wait", "video"),
    0x10bc06: ("fill_screen_rect_os", "graphics"),
    0x11713a: ("graphics_set_buffers", "graphics"),
    0x117754: ("heap_init", "memory"),
    0x117814: ("heap_setup_regions", "memory"),
    0x118a8c: ("video_call_driver", "video"),
    0x118aa0: ("video_open", "video"),
    0x118b06: ("video_close", "video"),
    0x118b88: ("display_switch_view", "video"),
    0x12833c: ("palette_color_step_down", "palette"),
    0x12bea0: ("os_open_resource", "os_stub"),
    0x12c10c: ("os_gfx_stub_12c10c", "os_stub"),
}

# Globals whose CSB type differs from DM.
CSB_SIGNATURES = {
    "g_spells": "Spell[29] (8 bytes each)",
    "g_action_sets": "uint8_t[44][6]",
}

# CSB-only globals verified in the code.
CSB_GLOBALS = {
    0x100182: ("g_fade_display_on_exit", "int16_t", "startend"),
    0x103738: ("g_protect_state", "int16_t", "protect"),
    0x100198: ("g_game_time", "int32_t", "main"),
    0x1001a6: ("g_last_load_game_time", "int32_t", "loadsave"),
}


def lvo_key(m, f):
    """LVO offsets of the JSR (-d,A6) calls of an OS stub."""
    return tuple(re.findall(r"jsr \(-\$([0-9a-f]+),A6\)", " ".join(t for _, _, t in m.disasm(f.start, f.end))))


def match(kf, cf):
    first = align(kf, cf)
    pairs = {k: c for k, (c, s) in first.items() if s >= 0.6}
    second = align(kf, cf, pairs)
    pairs = {k: c for k, (c, s) in second.items() if s >= 0.55}
    # Call-graph propagation for leftovers: similar callee, or the same replacement in three callers.
    votes = Counter()
    for _ in range(3):
        used = set(pairs.values())
        for ka, ca in list(pairs.items()):
            kc, cc = kf[ka].calls, cf[ca].calls
            for op, i1, i2, j1, j2 in difflib.SequenceMatcher(None, [pairs.get(c, -1) for c in kc], cc, autojunk=False).get_opcodes():
                if op == "replace" and i2 - i1 == j2 - j1:
                    for x, y in zip(kc[i1:i2], cc[j1:j2]):
                        if x in kf and y in cf and x not in pairs and y not in used:
                            votes[(x, y)] += 1
                            if sim(kf[x], cf[y], pairs) >= 0.5 or votes[(x, y)] >= 3:
                                pairs[x] = y
                                used.add(y)
    return pairs


def global_map(kf, cf, pairs):
    """Globals at the same position in paired functions; at least 2 votes and 80 % agreement."""
    votes = defaultdict(Counter)
    for k, c in pairs.items():
        a, b = kf[k].globs, cf[c].globs
        if len(a) == len(b):
            for x, y in zip(a, b):
                votes[x][y] += 1
    out = {}
    for x, v in votes.items():
        y, n = v.most_common(1)[0]
        if n >= 2 and n >= 0.8 * sum(v.values()):
            out[x] = y
    return out


def data_map(km, cm):
    """Map initialized-data addresses through equal byte blocks of the two data sections."""
    sm = difflib.SequenceMatcher(None, km.mem[: km.db - BASE], cm.mem[: cm.db - BASE], autojunk=False)
    return [(BASE + b.a, BASE + b.b, b.size) for b in sm.get_matching_blocks() if b.size >= 4]


def map_data(blocks, addr, size=1):
    """CSB address of a KAOS data range, or None when the range is not inside one equal block."""
    for ka, ca, n in blocks:
        if ka <= addr and addr + size <= ka + n:
            return ca + addr - ka
    return None


def classify(a, b, pairs, gmap, rt):
    """Compare mapped calls (without runtime helpers) and the set of non-zero constants."""
    kc = [pairs.get(x, -1) for x in a.calls if x not in rt]
    cc = [y for y in b.calls if y not in rt.values()]
    if kc != cc:
        return "logic"
    if set(a.consts) - {0} == set(b.consts) - {0} and a.args == b.args:
        return "identical"
    return "constants"


def apply_fixed_pairs(km, cm, names, kf, cf, pairs):
    """OS stubs pair by their LVO; then the manual pairs. Both replace conflicting pairs."""
    by_name = {r["name"]: a for a, r in names.items()}
    fixed = {}
    kl = defaultdict(list)
    for a, r in names.items():
        if r["subsystem"] == "os_stub":
            kl[lvo_key(km, kf[a])].append(a)
    cl = defaultdict(list)
    for a, f in cf.items():
        if f.size <= 50:
            k = lvo_key(cm, f)
            if k:
                cl[k].append(a)
    for k, ka in kl.items():
        if k and len(ka) == 1 and len(cl.get(k, [])) == 1:
            fixed[ka[0]] = cl[k][0]
    for c, n in MANUAL_PAIRS.items():
        fixed[by_name[n]] = c
    for k, c in fixed.items():
        for k2 in [x for x, y in pairs.items() if y == c]:
            del pairs[k2]
        pairs[k] = c


def run():
    km, cm = Module(KAOS_FTL), Module(CSB_FTL)
    names = kaos_names(km)
    kf, cf = build(km, names), build(cm, csb_starts(cm))
    pairs = match(kf, cf)
    apply_fixed_pairs(km, cm, names, kf, cf, pairs)
    gmap = global_map(kf, cf, pairs)
    rt = {k: c for k, c in pairs.items() if names[k]["subsystem"] in ("runtime", "math")}
    cls = {k: classify(kf[k], cf[c], pairs, gmap, rt) for k, c in pairs.items()}
    return km, cm, names, kf, cf, pairs, gmap, cls


def global_rows(km, cm, gmap):
    """KAOS globals with a CSB address: equal data bytes first, then code references."""
    blocks = data_map(km, cm)
    rows = [r for r in csv.DictReader(open(KAOS_CSV)) if int(r["address"], 16) < km.cb]
    starts = sorted(int(r["address"], 16) for r in rows)
    out = {}
    for r in rows:
        a = int(r["address"], 16)
        if r["subsystem"] == "jump_table":
            continue
        nxt = next((x for x in starts if x > a), km.db)
        n = min(nxt - a, 16)
        nonzero = any(km.mem[a - BASE : a - BASE + n])
        # Equal zero bytes prove nothing; such globals map through code references only.
        y = map_data(blocks, a, n) if a < km.db and nonzero else None
        if y is None:
            y = gmap.get(a)
        if y is not None and y not in out:
            out[y] = r
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", default="docs/reference/csb-appb-symbols.csv")
    ap.add_argument("--report", help="write one line per function and the global map here")
    args = ap.parse_args()
    km, cm, names, kf, cf, pairs, gmap, cls = run()
    globs = global_rows(km, cm, gmap)
    rev = {c: k for k, c in pairs.items()}
    counts = Counter(cls.values())
    counts["csb_only"] = sum(c not in rev for c in cf)
    counts["dm_only"] = sum(k not in pairs for k in kf)
    print(f"kaos={len(kf)} csb={len(cf)} " + " ".join(f"{k}={v}" for k, v in sorted(counts.items())))
    with open(args.csv, "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["address", "name", "signature", "subsystem"])
        for c in sorted(cf):
            k = rev.get(c)
            if k is not None:
                r = names[k]
            else:
                n, sub = CSB_NAMES.get(c, (f"csb_fun_{c:06x}", "csb_only"))
                r = {"name": n, "signature": "", "subsystem": sub}
            w.writerow([f"{c:#x}", r["name"], r["signature"], r["subsystem"]])
        rows = {y: (r["name"], CSB_SIGNATURES.get(r["name"], r["signature"]), r["subsystem"]) for y, r in globs.items()}
        rows.update(CSB_GLOBALS)
        for y, (n, sig, sub) in sorted(rows.items()):
            w.writerow([f"{y:#x}", n, sig, sub])
    if args.report:
        with open(args.report, "w") as f:
            for c in sorted(cf):
                k = rev.get(c)
                if k is None:
                    f.write(f"CSB_ONLY {c:#x} n={cf[c].size} calls={len(cf[c].calls)}\n")
                else:
                    f.write(f"{cls[k].upper()} {c:#x} {k:#x} {names[k]['name']} n={kf[k].size}/{cf[c].size} s={sim(kf[k], cf[c], pairs):.2f}\n")
            for k in sorted(kf):
                if k not in pairs:
                    f.write(f"DM_ONLY {k:#x} {names[k]['name']} n={kf[k].size}\n")
            f.write("GLOBALS\n")
            for x, y in sorted(gmap.items()):
                f.write(f"{x:#x} {y:#x}\n")


if __name__ == "__main__":
    main()
