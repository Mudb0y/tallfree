import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.app.decompiler.*;

public class DumpModule extends GhidraScript {
    public void run() throws Exception {
        long lo = 0x8017c000L, hi = 0x80180000L;
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int n=0;
        while (it.hasNext()) {
            Function f = it.next();
            long e = f.getEntryPoint().getOffset();
            if (e < lo || e >= hi) continue;
            n++;
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
            else println("// decompile failed");
        }
        println("// functions in range: " + n);
    }
}
