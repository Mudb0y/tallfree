import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class ListRange extends GhidraScript {
    public void run() throws Exception {
        long lo = 0x800de800L, hi = 0x800df200L;
        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            long a = f.getEntryPoint().getOffset();
            if (a >= lo && a < hi)
                println(String.format("%-22s @0x%08x  size %d", f.getName(), a, f.getBody().getNumAddresses()));
        }
    }
}
