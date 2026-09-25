import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.*;

public class DecompList extends GhidraScript {
    public void run() throws Exception {
        String[] addrs = {"80025ea8","800da720","80186bc0","8018ae68","80015c70"};
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String a : addrs) {
            Function f = getFunctionAt(toAddr(Long.parseLong(a,16)));
            if (f == null) { println("//no function at " + a); continue; }
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 180, monitor);
            if (dr != null && dr.decompileCompleted())
                println(dr.getDecompiledFunction().getC());
            else println("//decompile failed");
        }
    }
}
