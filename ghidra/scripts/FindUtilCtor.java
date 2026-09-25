import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindUtilCtor extends GhidraScript {
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
        long[] vtables = {0x80225200L, 0x80226294L};
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (long vt : vtables) {
            println("=== references to vtable " + toAddr(vt));
            LinkedHashSet<Function> fs = new LinkedHashSet<>();
            for (Reference r : getReferencesTo(toAddr(vt))) {
                Function f = form(r.getFromAddress());
                println("    from " + r.getFromAddress() + " in " +
                        (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                if (f != null) fs.add(f);
            }
            for (Function f : fs) {
                println("//##### " + f.getName() + " @ " + f.getEntryPoint());
                DecompileResults dr = di.decompileFunction(f, 90, monitor);
                if (dr != null && dr.decompileCompleted()) {
                    String c = dr.getDecompiledFunction().getC();
                    for (String line : c.split("\n")) if (line.length() < 200) println("   " + line);
                }
            }
        }
    }
}
