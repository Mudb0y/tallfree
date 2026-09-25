import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class UartRx extends GhidraScript {
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
        // every function that mentions the LPUART3 base or its data register
        long[] probes = {0x4018c000L, 0x4018c01cL, 0x4018c014L, 0x4018c018L};
        LinkedHashSet<Function> fs = new LinkedHashSet<>();
        for (long p : probes) {
            for (Reference r : getReferencesTo(toAddr(p))) {
                Function f = form(r.getFromAddress());
                println("ref to " + toAddr(p) + " from " + r.getFromAddress() + " in " +
                        (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                if (f != null) fs.add(f);
            }
        }
        // also scan instructions for the literal value
        Listing l = currentProgram.getListing();
        InstructionIterator it = l.getInstructions(true);
        int n=0;
        while (it.hasNext() && n < 4000) {
            Instruction i = it.next();
            String s = i.toString();
            if (s.contains("4018c0") || s.contains("0x4018c000")) {
                Function f = form(i.getAddress());
                if (f != null && fs.add(f)) println("literal use in " + f.getName() + " @" + f.getEntryPoint());
                n++;
            }
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (Function f : fs) {
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
        }
    }
}
