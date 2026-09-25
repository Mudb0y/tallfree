import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import java.util.*;

public class FindUpdate extends GhidraScript {
    public void run() throws Exception {
        String[] targets = {
            "UPDATE SURE?", "NO UPDATER", "UPDATE COMPLETE!", "DO NOT POWER OFF!",
            "a:/SP404MKII_APP1.bin", "A:/SP404MKII_APP0.bin", "a:/SP404MKII_APP1U.bin",
            "APP0 UPDATER", "QSPI.bin", "MODE:StandAlone", "i.MX ReBoot Menu"
        };
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Function> seen = new HashSet<>();
        for (String t : targets) {
            Address a = find(null, t.getBytes());
            if (a == null) { println("NOT FOUND: " + t); continue; }
            println("=== \"" + t + "\" at " + a);
            Reference[] refs = getReferencesTo(a);
            if (refs.length == 0) { println("    (no references recorded)"); continue; }
            for (Reference r : refs) {
                Function f = getFunctionContaining(r.getFromAddress());
                println("    ref from " + r.getFromAddress() + "  in " +
                        (f == null ? "<no function>" : f.getName() + " @" + f.getEntryPoint()));
                if (f != null) seen.add(f);
            }
        }
        println("");
        println("############ DECOMPILED CANDIDATE FUNCTIONS ############");
        for (Function f : seen) {
            println("");
            println("//===== " + f.getName() + " @ " + f.getEntryPoint() + " =====");
            DecompileResults dr = di.decompileFunction(f, 90, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
            else println("// decompilation failed");
        }
    }
}
