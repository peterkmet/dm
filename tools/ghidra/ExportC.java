// Decompile every function of the current program and write the C code to a file.
// Usage: -postScript ExportC.java <output.c>
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class ExportC extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(getScriptArgs()[0])) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                DecompileResults r = decomp.decompileFunction(f, 60, monitor);
                out.println("/* " + f.getEntryPoint() + " */");
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "/* failed: " + r.getErrorMessage() + " */");
            }
        }
        decomp.dispose();
    }
}
