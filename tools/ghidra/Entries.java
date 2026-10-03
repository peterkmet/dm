// Mark entry points before auto-analysis: the given addresses plus every LINK A5/A6 that follows an RTS.
// An argument REG=<value> (for example A5=0x104160) sets that register for the whole program.
// Usage: -preScript Entries.java [REG=<value>] <addr> [<addr> ...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import java.math.BigInteger;
import ghidra.program.model.mem.Memory;

public class Entries extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        for (String a : getScriptArgs()) {
            if (a.contains("=")) {
                String[] kv = a.split("=");
                Register reg = currentProgram.getRegister(kv[0]);
                currentProgram.getProgramContext().setValue(reg, mem.getMinAddress(), mem.getMaxAddress(),
                        BigInteger.valueOf(Long.decode(kv[1])));
            } else {
                mark(toAddr(Long.decode(a)));
            }
        }
        Address start = mem.getMinAddress();
        long size = mem.getMaxAddress().subtract(start) + 1;
        for (long off = 2; off + 2 <= size; off += 2) {
            Address a = start.add(off);
            short op = mem.getShort(a);
            if ((op == 0x4e55 || op == 0x4e56) && mem.getShort(a.subtract(2)) == 0x4e75) {
                mark(a);
            }
        }
    }

    private void mark(Address addr) {
        currentProgram.getSymbolTable().addExternalEntryPoint(addr);
        disassemble(addr);
        createFunction(addr, null);
    }
}
