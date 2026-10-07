// Set stack parameters from frame accesses: the compiler passes each argument in a 2-byte (word) or 4-byte (long) slot.
// For each function that starts with LINK A5/A6, every d16(An) access with d16 >= 8 is an argument access.
// Usage: -postScript InferParams.java (after auto-analysis, before ExportC.java)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.SourceType;
import java.util.*;

public class InferParams extends GhidraScript {
    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        int count = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            Instruction first = listing.getInstructionAt(f.getEntryPoint());
            if (first == null || !first.getMnemonicString().startsWith("link")) {
                continue;
            }
            Register fp = first.getRegister(0);
            if (fp == null || !(fp.getName().equals("A5") || fp.getName().equals("A6"))) {
                continue;
            }
            TreeMap<Integer, Integer> sizes = new TreeMap<>();
            for (Instruction in : listing.getInstructions(f.getBody(), true)) {
                int size = accessSize(in.getMnemonicString());
                for (int i = 0; i < in.getNumOperands(); i++) {
                    Object[] objs = in.getOpObjects(i);
                    List<Object> l = Arrays.asList(objs);
                    if (objs.length == 2 && l.contains(fp)) {
                        for (Object o : objs) {
                            if (o instanceof Scalar s && s.getSignedValue() >= 8) {
                                sizes.merge((int) s.getSignedValue(), size, Math::max);
                            }
                        }
                    }
                }
            }
            f.updateFunction(Function.DEFAULT_CALLING_CONVENTION_STRING, f.getReturn(), buildParams(f, sizes),
                    FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.ANALYSIS);
            count++;
        }
        println("InferParams: " + count + " functions");
    }

    // .b = 1, .l = 4, everything else (.w, lea, pea) = 2.
    private static int accessSize(String mnemonic) {
        if (mnemonic.endsWith(".b")) {
            return 1;
        }
        return mnemonic.endsWith(".l") ? 4 : 2;
    }

    // Frame offset 8 is stack offset 4: the return address is at stack offset 0 on entry.
    private List<Variable> buildParams(Function f, TreeMap<Integer, Integer> sizes) throws Exception {
        List<Variable> params = new ArrayList<>();
        for (Parameter p : f.getParameters()) {
            if (p.isRegisterVariable()) {
                params.add(p);
            }
        }
        int next = 8;
        for (Map.Entry<Integer, Integer> e : sizes.entrySet()) {
            int off = e.getKey();
            int size = (off & 1) == 1 ? 1 : e.getValue();
            int slot = off & ~1;
            if (slot < next) {
                continue;
            }
            for (; next < slot; next += 2) {
                params.add(param(next, Undefined2DataType.dataType, next));
            }
            DataType dt = size == 1 ? CharDataType.dataType : size == 2 ? ShortDataType.dataType : Undefined4DataType.dataType;
            params.add(param(slot, dt, off));
            next = slot + Math.max(size, 2);
        }
        return params;
    }

    private Parameter param(int slot, DataType dt, int off) throws Exception {
        String name = "arg_" + Integer.toHexString(slot);
        return new ParameterImpl(name, dt, new VariableStorage(currentProgram, off - 4, dt.getLength()),
                currentProgram, SourceType.ANALYSIS);
    }
}
