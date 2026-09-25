import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.util.*;
// prints decompiled lines that assign to each given global object's first word
public class FindStore extends GhidraScript {
    public void run() throws Exception {
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        for (String s : getScriptArgs()) {
            Set<Function> fs = new LinkedHashSet<>();
            for (Reference r : getReferencesTo(toAddr(Long.parseLong(s, 16)))) {
                Function f = getFunctionContaining(r.getFromAddress()); if (f != null) fs.add(f);
            }
            println("@@@ " + s + " refs-in " + fs.size() + " functions");
            for (Function f : fs) {
                DecompileResults dr = di.decompileFunction(f, 60, monitor);
                if (!dr.decompileCompleted()) continue;
                for (String l : dr.getDecompiledFunction().getC().split("\n"))
                    if (l.contains(s.toLowerCase()) && l.contains("= &PTR") || l.contains("_" + s.toLowerCase() + " = ") && l.contains("PTR"))
                        println("    " + f.getEntryPoint() + ": " + l.trim());
            }
        }
    }
}
