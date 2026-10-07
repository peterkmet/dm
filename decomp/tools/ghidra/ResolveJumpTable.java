// Turn JSR/JMP d16(A5) through a module jump table slot (JMP abs.l) into a direct call to the slot target.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;
import java.math.BigInteger;

public class ResolveJumpTable extends GhidraScript {
    @Override
    public void run() throws Exception {
        Register a5 = currentProgram.getRegister("A5");
        Memory mem = currentProgram.getMemory();
        int count = 0;
        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            String m = ins.getMnemonicString();
            if (!m.equals("jsr") && !m.equals("jmp")) continue;
            Object[] objs = ins.getOpObjects(0);
            if (objs.length != 2 || !(objs[0] instanceof Scalar) || !a5.equals(objs[1])) continue;
            BigInteger base = currentProgram.getProgramContext().getValue(a5, ins.getAddress(), false);
            if (base == null) continue;
            Address slot = toAddr(base.longValue() + ((Scalar) objs[0]).getSignedValue());
            if (!mem.contains(slot) || (mem.getShort(slot) & 0xffff) != 0x4ef9) continue;
            Address target = toAddr(mem.getInt(slot.add(2)) & 0xffffffffL);
            RefType type = m.equals("jsr") ? RefType.CALL_OVERRIDE_UNCONDITIONAL : RefType.JUMP_OVERRIDE_UNCONDITIONAL;
            for (var old : ins.getOperandReferences(0)) {
                currentProgram.getReferenceManager().delete(old);
            }
            var ref = currentProgram.getReferenceManager().addMemoryReference(ins.getAddress(), target, type,
                    SourceType.USER_DEFINED, 0);
            currentProgram.getReferenceManager().setPrimary(ref, true);
            count++;
        }
        println("ResolveJumpTable: " + count + " calls");
    }
}
