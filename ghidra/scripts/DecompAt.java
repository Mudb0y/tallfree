import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.*;
public class DecompAt extends GhidraScript {
    public void run() throws Exception {
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        for (String s : getScriptArgs()) {
            Function f = getFunctionContaining(toAddr(Long.parseLong(s, 16)));
            if (f == null) { println("@@@ " + s + " none"); continue; }
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            println("@@@ " + s + " in " + f.getEntryPoint() + "\n" + (dr.decompileCompleted() ? dr.getDecompiledFunction().getC() : "FAIL"));
        }
    }
}
