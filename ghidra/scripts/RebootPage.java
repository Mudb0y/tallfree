import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.*;

public class RebootPage extends GhidraScript {
    public void run() throws Exception {
        String[] addrs = {"80142c20"};
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String a : addrs) {
            long off = Long.parseLong(a,16);
            Function f = getFunctionAt(toAddr(off));
            if (f == null) { try { f = createFunction(toAddr(off), null); } catch(Exception e){} }
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
