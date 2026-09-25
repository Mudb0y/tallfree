import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class Callers extends GhidraScript {
    public void run() throws Exception {
        long[] seeds = {0x800ff8f0L, 0x80113880L, 0x80113b10L, 0x8017d218L, 0x80142a48L};
        LinkedHashSet<Function> toDump = new LinkedHashSet<>();
        for (long s : seeds) {
            Address ad = toAddr(s);
            Function f = getFunctionAt(ad);
            println("=== callers of " + (f==null?("addr "+ad):(f.getName()+" @"+ad)));
            Reference[] rs = getReferencesTo(ad);
            if (rs.length == 0) println("    (none recorded)");
            for (Reference r : rs) {
                Function c = getFunctionContaining(r.getFromAddress());
                println("    from " + r.getFromAddress() + " " + r.getReferenceType() +
                        " in " + (c==null?"<none>":c.getName()+" @"+c.getEntryPoint()));
                if (c != null) toDump.add(c);
            }
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (Function f : toDump) {
            println("");
            println("//########## CALLER " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
        }
    }
}
