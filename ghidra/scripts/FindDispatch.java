import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindDispatch extends GhidraScript {
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
        LinkedHashSet<Function> dump = new LinkedHashSet<>();
        for (long probe = 0x8023b4fcL; probe <= 0x8023b508L; probe += 4) {
            for (Reference r : getReferencesTo(toAddr(probe))) {
                Function f = form(r.getFromAddress());
                println("ref to " + toAddr(probe) + " from " + r.getFromAddress() + " in " +
                        (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                if (f != null) dump.add(f);
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
