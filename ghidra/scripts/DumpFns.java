import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.app.decompiler.*;

public class DumpFns extends GhidraScript {
    public void run() throws Exception {
        long[] fns = {0x80021e00L, 0x800d93f0L};
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (long a : fns) {
            Function f = getFunctionAt(toAddr(a));
            if (f == null) { try { f = createFunction(toAddr(a), null); } catch (Exception e) {} }
            if (f == null) { println("no function at " + toAddr(a)); continue; }
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
        }
    }
}
