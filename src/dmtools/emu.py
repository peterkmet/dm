"""Run functions of a loaded FTL module image in a 68000 emulator (machine68k)."""

import machine68k
from machine68k import Register

RAM_KIB = 8192
RET_ADDR = 0x10000  # trap opcode that ends a call
EXC_ADDR = 0x10100  # vector N points to EXC_ADDR + 2 * N, each a trap opcode
CUSTOM = range(0xDFF000, 0xE00000)
CIA = range(0xBFD000, 0xBFF000)


class EmuError(Exception):
    pass


_machine = None  # machine68k keeps RAM, CPU and traps in global C state: one machine per process
_current = None


def _setup():
    global _machine, _ret_op, _exc_op
    _machine = machine68k.Machine(machine68k.CPUType.M68000, RAM_KIB)
    mem = _machine.mem
    mem.w32(4, RET_ADDR)  # reset PC
    _machine.cpu.pulse_reset()
    _ret_op = 0xA000 | _machine.traps.alloc(lambda op, pc: _machine.create_execute_end("return"))
    _exc_op = 0xA000 | _machine.traps.alloc(_exception)
    mem.set_invalid_func(_invalid)
    # page 0 reads go through Python to catch ExecBase reads at address 4
    r = lambda n, f: lambda a: _page0(a, n, f)
    mem.set_special_range_read_funcs(0, 1, r(1, mem.r8), r(2, mem.r16), r(4, mem.r32))


def _exception(op, pc):
    raise EmuError(f"CPU exception vector {(pc - EXC_ADDR) // 2}, pc={_machine.mem.r32(_machine.cpu.r_reg(Register.A7) + 2):#x}")


def _invalid(mode, width, addr):
    if _current.allow_hw and (addr in CUSTOM or addr in CIA):
        return
    name = "custom chip" if addr in CUSTOM else "CIA" if addr in CIA else "unmapped"
    raise EmuError(f"{name} access {mode}{8 << width} at {addr:#x}, pc={_machine.cpu.r_pc():#x}")


def _page0(addr, size, read):
    if not _current.allow_hw and addr < 8 and addr + size > 4:
        raise EmuError(f"ExecBase read at {addr:#x}, pc={_machine.cpu.r_pc():#x}")
    return read(addr)


class Emu:
    """Creating an Emu replaces the RAM of the previous one; the previous one then raises on use."""

    def __init__(self, image, max_cycles=10_000_000):
        global _current
        if _machine is None:
            _setup()
        if image.base + len(image.data) > RAM_KIB * 1024:
            raise EmuError("image does not fit in RAM")
        self.image = image
        self.max_cycles = max_cycles
        self.allow_hw = False
        self.mem, self.cpu = _machine.mem, _machine.cpu
        self.mem.clear_block(0, RAM_KIB * 1024, 0)
        self.mem.w_block(image.base, image.data)
        _current = self

    def call(self, addr, args=(), allow_hw=False):
        """Call addr with (size, value) args pushed right to left; return D0."""
        if _current is not self:
            raise EmuError("stale Emu: a newer Emu replaced its memory")
        self.allow_hw = allow_hw
        for n in range(2, 64):
            self.mem.w32(4 * n, EXC_ADDR + 2 * n)
            self.mem.w16(EXC_ADDR + 2 * n, _exc_op)
        self.mem.w16(RET_ADDR, _ret_op)
        sp = self.image.base
        for size, value in reversed(args):
            sp -= size
            self.mem.write(size >> 1, sp, value & ((1 << 8 * size) - 1))
        sp -= 4
        self.mem.w32(sp, RET_ADDR)
        for r in range(Register.D0, Register.A7):
            self.cpu.w_reg(r, 0)
        self.cpu.w_reg(Register.A5, self.image.data_base)
        self.cpu.w_reg(Register.A7, sp)
        self.cpu.w_sr(0x2700)
        self.cpu.w_pc(addr)
        if _machine.execute(self.max_cycles).result is None:
            raise EmuError(f"cycle budget {self.max_cycles} exceeded, pc={self.cpu.r_pc():#x}")
        return self.cpu.r_reg(Register.D0)

    def reg(self, name):
        return self.cpu.r_reg(Register[name])

    def read(self, addr, size):
        return self.mem.r_block(addr, size)

    def write(self, addr, data):
        self.mem.w_block(addr, bytes(data))
