import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
public class ListRange2 extends GhidraScript {
    public void run() throws Exception {
        long lo = 0x8001d600L, hi = 0x8001da80L;
        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            long a = f.getEntryPoint().getOffset();
            if (a >= lo && a < hi)
                println(String.format("%-22s @0x%08x  size %d", f.getName(), a, f.getBody().getNumAddresses()));
        }
    }
}
