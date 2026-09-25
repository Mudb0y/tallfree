import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindUpdDetect extends GhidraScript {
    Function form(Address ad) {
        Function f = getFunctionContaining(ad);
        if (f != null) return f;
        for (long a = ad.getOffset(); a > ad.getOffset() - 0x1000; a -= 2) {
            Instruction ins = getInstructionAt(toAddr(a));
            if (ins != null && ins.getMnemonicString().startsWith("push")) {
                try { f = createFunction(toAddr(a), null); } catch (Exception e) {}
                if (f != null) return f;
            }
        }
        return null;
    }
    public void run() throws Exception {
        String[] targets = {"A:/SP404MKII_APP1.bin","a:/SP404MKII_APP1.bin",
                            "A:/SP404MKII_APP0.bin","a:/SP404MKII_APP1U.bin"};
        LinkedHashSet<Function> fs = new LinkedHashSet<>();
        for (String t : targets) {
            Address a = find(null, t.getBytes());
            if (a == null) { println("NOTFOUND: " + t); continue; }
            println("=== \"" + t + "\" @ " + a);
            int n=0;
            for (Reference r : getReferencesTo(a)) {
                Function f = form(r.getFromAddress());
                println("    ref " + r.getFromAddress() + " in " +
                        (f == null ? "<none>" : f.getName() + " @" + f.getEntryPoint()));
                if (f != null) fs.add(f); n++;
            }
            if (n==0) println("    (none)");
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (Function f : fs) {
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 200, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
        }
    }
}
