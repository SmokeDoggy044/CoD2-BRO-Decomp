// Writes every function's address and name to tools/data/default_xbe_symbols.txt,
// for readable loader backtraces. Re-run after renaming functions in Ghidra.
// Optional script argument: output path.
//@category CodBro

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportSymbols extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = args.length > 0 ? new File(args[0]) : askFile("Symbol file", "Save");
        int count = 0;
        try (PrintWriter writer = new PrintWriter(out)) {
            writer.println("# " + currentProgram.getName() + " functions, exported from Ghidra");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                Function f = functions.next();
                if (f.isExternal()) continue;
                writer.printf("%08x %s%n", f.getEntryPoint().getOffset(), f.getName(true).replace(' ', '_'));
                count++;
            }
        }
        println("Wrote " + count + " symbols to " + out);
    }
}
