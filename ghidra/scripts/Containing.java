import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.app.decompiler.*;
public class Containing extends GhidraScript {
    public void run() throws Exception {
        long[] probes = {0x80065800L, 0x8006581cL, 0x800658e0L};
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        java.util.LinkedHashSet<Function> seen = new java.util.LinkedHashSet<>();
        for (long p : probes) {
            Function f = getFunctionContaining(toAddr(p));
            println("probe 0x" + Long.toHexString(p) + " -> " +
                    (f==null? "<no function>" : f.getName()+" @"+f.getEntryPoint()));
            if (f != null) seen.add(f);
        }
        for (Function f : seen) {
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 200, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
        }
    }
}
