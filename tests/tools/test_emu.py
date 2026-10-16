# SPDX-License-Identifier: GPL-3.0-or-later
import pytest

from dmtest.markers import GAME
from dmtools.emu import Emu, EmuError
from dmtools.ftl import Image, load_image, parse

KAOS = GAME / "KAOS.FTL"
BASE = 0x100000


def tiny(code):
    return Emu(Image(bytes(code), BASE, BASE + 0x100, BASE, [BASE]), max_cycles=10_000)


def test_word_args():
    # move.w 4(sp),d0; add.w 6(sp),d0; rts
    emu = tiny(bytes.fromhex("302F0004D06F00064E75"))
    assert emu.call(BASE, [(2, 1234), (2, 4321)]) & 0xFFFF == 5555


def test_word_push_order_and_negative():
    # move.w 4(sp),d0; sub.w 6(sp),d0; rts
    emu = tiny(bytes.fromhex("302F0004906F00064E75"))
    assert emu.call(BASE, [(2, 10), (2, 3)]) & 0xFFFF == 7
    assert emu.call(BASE, [(2, -3), (2, 4)]) & 0xFFFF == -7 & 0xFFFF


def test_mixed_word_long_args():
    # move.w 4(sp),d0; ext.l d0; add.l 6(sp),d0; rts
    emu = tiny(bytes.fromhex("302F000448C0D0AF00064E75"))
    assert emu.call(BASE, [(2, -5), (4, 100000)]) == 99995


def test_a5_is_data_base():
    emu = tiny(bytes.fromhex("200D4E75"))  # move.l a5,d0; rts
    assert emu.call(BASE) == BASE + 0x100


def test_registers_reset_between_calls():
    # moveq #5,d1; rts; move.l d1,d0; rts
    emu = tiny(bytes.fromhex("72054E7520014E75"))
    emu.call(BASE)
    assert emu.call(BASE + 4) == 0


def test_stale_emu_raises():
    old = tiny(bytes.fromhex("4E75"))
    tiny(bytes.fromhex("4E75"))
    with pytest.raises(EmuError, match="stale"):
        old.call(BASE)


def test_cia_access_raises():
    emu = tiny(bytes.fromhex("103900BFE0014E75"))  # move.b $bfe001,d0; rts
    with pytest.raises(EmuError, match="CIA"):
        emu.call(BASE)


def test_unmapped_access_raises():
    emu = tiny(bytes.fromhex("203900900000" "4E75"))  # move.l $900000,d0; rts
    with pytest.raises(EmuError, match="unmapped"):
        emu.call(BASE)


def test_illegal_instruction_raises():
    emu = tiny(bytes.fromhex("4AFC"))
    with pytest.raises(EmuError, match="CPU exception vector 4"):
        emu.call(BASE)


def test_custom_chip_access_raises():
    # move.w $dff006,d0; rts
    emu = tiny(bytes.fromhex("303900DFF0064E75"))
    with pytest.raises(EmuError, match="custom chip"):
        emu.call(BASE)
    emu.call(BASE, allow_hw=True)


def test_execbase_read_raises():
    # movea.l 4.w,a6; rts
    emu = tiny(bytes.fromhex("2C7800044E75"))
    with pytest.raises(EmuError, match="ExecBase"):
        emu.call(BASE)


def test_cycle_budget():
    emu = tiny(bytes.fromhex("60FE"))  # bra.s *
    with pytest.raises(EmuError, match="budget"):
        emu.call(BASE)


def test_kaos_runtime_mul_div():
    if not KAOS.exists():
        pytest.skip("game/KAOS.FTL missing")
    emu = Emu(load_image(parse(KAOS.read_bytes()), BASE))
    mul, udiv, umod, sdiv, smod = 0x104F3E, 0x104F74, 0x104F94, 0x104FB6, 0x104FD6
    s32 = lambda v: v - (1 << 32) if v & 0x80000000 else v
    for a, b in [(7, 6), (123456, 789), (-5, 3), (0x12345678, 0x9ABC), (-100000, -7), (100000, -7)]:
        args = [(4, a), (4, b)]
        assert emu.call(mul, args) == (a * b) & 0xFFFFFFFF
        q = abs(a) // abs(b) * (1 if (a < 0) == (b < 0) else -1)
        assert s32(emu.call(sdiv, args)) == q
        assert s32(emu.call(smod, args)) == a - q * b
        ua, ub = a & 0xFFFFFFFF, b & 0xFFFFFFFF
        assert emu.call(udiv, args) == ua // ub
        assert emu.call(umod, args) == ua % ub
    with pytest.raises(EmuError, match="CPU exception vector 5"):
        emu.call(sdiv, [(4, 7), (4, 0)])
