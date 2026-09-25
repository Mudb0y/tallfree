import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.app.decompiler.*;

public class UpdTask extends GhidraScript {
    public void run() throws Exception {
        String[] addrs = {"800da598"};
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String a : addrs) {
            long off = Long.parseLong(a,16);
            Function f = getFunctionAt(toAddr(off));
            if (f == null) f = getFunctionContaining(toAddr(off));
            if (f == null) { try { f = createFunction(toAddr(off), null); } catch(Exception e){} }
            if (f == null) { println("//no function at " + a); continue; }
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " (asked for " + a + ") ##########");
            DecompileResults dr = di.decompileFunction(f, 200, monitor);
            if (dr != null && dr.decompileCompleted())
                println(dr.getDecompiledFunction().getC());
            else println("//decompile failed");
        }
    }
}
