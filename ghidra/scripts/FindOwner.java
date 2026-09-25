import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.util.*;

public class FindOwner extends GhidraScript {
    public void run() throws Exception {
        long[] targets = {0x800c4eb8L, 0x8021d070L, 0x800732f8L};
        String[] names = {"ctor FUN_800C4EB8", "vtable 0x8021D070", "PTR_LAB_800732F8"};
        for (int k=0;k<targets.length;k++) {
            Address a = toAddr(targets[k]);
            println("===== references to " + names[k] + " =====");
            int n=0;
            for (Reference r : getReferencesTo(a)) {
                Function f = getFunctionContaining(r.getFromAddress());
                println("  from " + r.getFromAddress() + "  type=" + r.getReferenceType() +
                        "  in " + (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                if (++n>25) { println("  ..."); break; }
            }
            if (n==0) println("  (none)");
            println("");
        }
    }
}
