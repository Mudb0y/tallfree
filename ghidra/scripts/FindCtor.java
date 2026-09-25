import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindCtor extends GhidraScript {
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
        // vtables containing the debug-menu handler and the update screen.
        // Scan backwards from the known slot to the start of the pointer run.
        long[] slots = {0x80221660L, 0x8021f620L, 0x80225fccL};
        LinkedHashSet<Function> dump = new LinkedHashSet<>();
        for (long slot : slots) {
            long base = slot;
            while (true) {
                long prev = base - 4;
                int v = getInt(toAddr(prev));
                long uv = ((long)v) & 0xffffffffL;
                if (uv >= 0x80000000L && uv < 0x80245740L) base = prev; else break;
                if (slot - base > 0x200) break;
            }
            println("=== vtable run starting near " + toAddr(base) + " (slot " + toAddr(slot) + ")");
            for (long probe = base; probe <= base + 8; probe += 4) {
                Reference[] rs = getReferencesTo(toAddr(probe));
                for (Reference r : rs) {
                    Function f = form(r.getFromAddress());
                    println("    vtable ref from " + r.getFromAddress() + " in " +
                            (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                    if (f != null) dump.add(f);
                }
            }
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
