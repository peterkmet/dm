import os
import subprocess
from pathlib import Path

from dmtest.markers import ROOT, needs_ghidra


@needs_ghidra
def test_params_and_symbols(tmp_path):
    caller = bytes.fromhex(
        "4e560000"  # 100000: link a6,#0
        "2f3c12345678"  # move.l #$12345678,-(sp)
        "3f3c0005"  # move.w #5,-(sp)
        "3f3c0003"  # move.w #3,-(sp)
        "3f3c0002"  # move.w #2,-(sp)
        "4eb900100024"  # jsr $100024
        "4fef000c"  # lea 12(sp),sp
        "4e5e4e75"  # unlk a6; rts
    )
    callee = bytes.fromhex(
        "4e550000"  # 100024: link a5,#0
        "302d0008"  # move.w 8(a5),d0
        "d06d000a"  # add.w 10(a5),d0
        "d02d000d"  # add.b 13(a5),d0
        "d0ad000e"  # add.l 14(a5),d0
        "4e5d4e75"  # unlk a5; rts
    )
    orphan = bytes.fromhex(
        "303900100044"  # 10003c: move.w $100044,d0 (no caller, no LINK)
        "4e75"  # rts
        "1234"  # 100044: global word
    )
    image = tmp_path / "t.bin"
    image.write_bytes(caller + callee + orphan)
    symbols = tmp_path / "t.csv"
    symbols.write_text(
        "address,name,signature,subsystem\n"
        "0x100024,add4,,test\n"
        '0x10003c,get_val,"short get_val(void)",test\n'
        "0x100044,g_val,,test\n"
    )
    out = tmp_path / "t.c"
    subprocess.run(
        [ROOT / "tools/ghidra/decompile.sh", image, "0x100000", out, "0x100000"],
        env={**os.environ, "SYMBOLS": str(symbols)},
        check=True,
    )
    c = out.read_text()
    caller_c = c.split("/* 00100024 */")[0]
    assert "add4(short arg_8,short arg_a,char arg_c,int arg_e)" in c
    assert "add4(2,3,'\\x05',0x12345678)" in caller_c
    assert ">> 0x10" not in caller_c
    assert "get_val(void)" in c
    assert "g_val;" in c
    assert "SCRIPT ERROR" not in Path(f"{out}.log").read_text()
