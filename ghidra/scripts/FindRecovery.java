import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindRecovery extends GhidraScript {
    Function formFunction(Address ad) {
        Function f = getFunctionContaining(ad);
        if (f != null) return f;
        for (long a = ad.getOffset(); a > ad.getOffset() - 0x600; a -= 2) {
            Address c = toAddr(a);
            Instruction ins = getInstructionAt(c);
            if (ins != null && ins.getMnemonicString().startsWith("push")) {
                try { f = createFunction(c, null); } catch (Exception e) {}
                if (f != null) return f;
            }
        }
        return null;
    }
    public void run() throws Exception {
        String[] targets = {"QSPI.bin", "%s/QSPI.bin", "QSPI.txt", "QSPI Data Version",
                            "i.MX ReBoot Menu", "MODE:StandAlone", "MODE:eFuse", "MODE:Debug",
                            "ENT:iMX Reboot", "Debug Menu", "TEST MENU  [Ent]", "Debug TestMode"};
        LinkedHashSet<Function> funcs = new LinkedHashSet<>();
        for (String t : targets) {
            Address a = find(null, t.getBytes());
            if (a == null) { println("NOT FOUND: " + t); continue; }
            println("=== \"" + t + "\" @ " + a);
            for (Reference r : getReferencesTo(a)) {
                Function f = formFunction(r.getFromAddress());
                println("    ref " + r.getFromAddress() + " -> " +
                        (f == null ? "<none>" : f.getName() + " @" + f.getEntryPoint()));
                if (f != null) funcs.add(f);
            }
        }
        // references to the debug menu table itself
        Address tbl = toAddr(0x801ae428L);
        println("");
        println("=== debug menu table @ 801ae428");
        for (Reference r : getReferencesTo(tbl)) {
            Function f = formFunction(r.getFromAddress());
            println("    ref " + r.getFromAddress() + " -> " + (f == null ? "<none>" : f.getName()));
            if (f != null) funcs.add(f);
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (Function f : funcs) {
            println("");
            println("//########## " + f.getName() + " @ " + f.getEntryPoint() + " ##########");
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr != null && dr.decompileCompleted()) println(dr.getDecompiledFunction().getC());
            else println("// decompile failed");
        }
    }
}
