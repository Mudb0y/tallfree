import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class OpenSites extends GhidraScript {
    public void run() throws Exception {
        long[] fns = {0x800dec60L, 0x800dec78L};
        String[] nm = {"open FUN_800dec60", "write FUN_800dec78"};
        for (int k = 0; k < fns.length; k++) {
            Address fa = toAddr(fns[k]);
            println("===== callers of " + nm[k] + " =====");
            int n = 0;
            for (Reference r : getReferencesTo(fa)) {
                Address from = r.getFromAddress();
                Function cf = getFunctionContaining(from);
                // walk back a little to catch the immediate loaded into r1 (the mode)
                StringBuilder ctx = new StringBuilder();
                for (long a = from.getOffset() - 16; a < from.getOffset(); a += 2) {
                    Instruction in = getInstructionAt(toAddr(a));
                    if (in == null) continue;
                    String m = in.getMnemonicString();
                    if (m.startsWith("mov") || m.startsWith("ldr")) {
                        String ops = in.toString();
                        if (ops.contains("r1") || ops.contains("r2"))
                            ctx.append("[").append(ops).append("] ");
                    }
                }
                println(String.format("  %s in %-22s %s", from,
                        cf == null ? "<none>" : cf.getName(), ctx));
                if (++n > 40) { println("  ..."); break; }
            }
            if (n == 0) println("  (no references)");
            println("");
        }
    }
}
