// Name functions and globals from a CSV file with the columns address,name,signature,subsystem (first line is a header).
// A row with a signature, or at an instruction, names a function (created if missing). Other rows label a global.
// The script does not apply the signature text. If the file does not exist, the script does nothing.
// Usage: -postScript ApplySymbols.java <symbols.csv>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.util.List;

public class ApplySymbols extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0 || args[0].isEmpty() || !Files.isRegularFile(Path.of(args[0]))) {
            return;
        }
        List<String> lines = Files.readAllLines(Path.of(args[0]));
        for (String line : lines.subList(Math.min(1, lines.size()), lines.size())) {
            String[] cols = line.split(",", 3);
            if (cols.length < 2 || cols[1].isBlank()) {
                continue;
            }
            Address addr = toAddr(Long.decode(cols[0].trim()));
            String name = cols[1].trim();
            Function f = getFunctionAt(addr);
            boolean signature = cols.length > 2 && !cols[2].isEmpty() && !cols[2].startsWith(",");
            boolean code = getInstructionAt(addr) != null || signature;
            if (f == null && code) {
                disassemble(addr);
                f = createFunction(addr, name);
            }
            if (f != null) {
                f.setName(name, SourceType.USER_DEFINED);
            } else {
                createLabel(addr, name, true, SourceType.USER_DEFINED);
            }
        }
        println("ApplySymbols: " + Math.max(0, lines.size() - 1) + " rows from " + args[0]);
    }
}
