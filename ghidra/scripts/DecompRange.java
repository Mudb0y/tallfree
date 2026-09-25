import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.*;
import java.util.*;

public class DecompRange extends GhidraScript {
    public void run() throws Exception {
        long[] refs = {0x800ff910L,0x800ff974L,0x800ff984L,0x800ff9c0L,0x800ffbb8L,0x800ffbcaL};
        // disassemble a generous window around the update code
        for (long a = 0x800ff800L; a < 0x80100200L; a += 2) {
            Address ad = toAddr(a);
            if (getInstructionAt(ad) == null) { try { disassemble(ad); } catch (Exception e) {} }
        }
        Set<Function> funcs = new LinkedHashSet<>();
        for (long r : refs) {
            Address ad = toAddr(r);
            Function f = getFunctionContaining(ad);
            if (f == null) {
                // walk back to a plausible prologue and create a function
                for (long a = r; a > r - 0x400; a -= 2) {
                    Address c = toAddr(a);
                    var ins = getInstructionAt(c);
                    if (ins != null && ins.getMnemonicString().startsWith("push")) {
                        try { f = createFunction(c, null); } catch (Exception e) {}
                        if (f != null) break;
                    }
                }
            }
            if (f != null) funcs.add(f);
            else println("could not form a function around " + ad);
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
