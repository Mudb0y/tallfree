import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class Callers2 extends GhidraScript {
    Function form(Address ad) {
        Function f = getFunctionContaining(ad);
        if (f != null) return f;
        for (long a = ad.getOffset(); a > ad.getOffset() - 0x800; a -= 2) {
            Instruction ins = getInstructionAt(toAddr(a));
            if (ins != null && ins.getMnemonicString().startsWith("push")) {
                try { f = createFunction(toAddr(a), null); } catch (Exception e) {}
                if (f != null) return f;
            }
        }
        return null;
    }
    public void run() throws Exception {
        long[] seeds = {0x8005e448L};
        LinkedHashSet<Function> dump = new LinkedHashSet<>();
        ArrayDeque<Long> q = new ArrayDeque<>();
        for (long s : seeds) q.add(s);
        Set<Long> done = new HashSet<>();
        int depth = 0;
        while (!q.isEmpty() && depth < 3) {
            int n = q.size();
            for (int i = 0; i < n; i++) {
                long s = q.poll();
                if (!done.add(s)) continue;
                Address ad = toAddr(s);
                println("=== callers of " + ad);
                for (Reference r : getReferencesTo(ad)) {
                    Function c = form(r.getFromAddress());
                    println("    from " + r.getFromAddress() + " " + r.getReferenceType() +
                            " in " + (c==null?"<none>":c.getName()+" @"+c.getEntryPoint()));
                    if (c != null) { dump.add(c); q.add(c.getEntryPoint().getOffset()); }
                }
            }
            depth++;
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (Function f : dump) {
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
        }
    }
}
