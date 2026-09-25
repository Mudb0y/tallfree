import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindWav extends GhidraScript {
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
        String[] t = {"A:/ROLAND/sinsin10Hz2s.wav","A:/ROLAND/saw512smpl.wav",
                      "A:/ROLAND/2sinsaw15.wav","A:/ROLAND/sawsaw1Hz.wav",
                      "A:/ROLAND/saw128smpl.wav"};
        LinkedHashSet<Function> fs = new LinkedHashSet<>();
        for (String s : t) {
            Address a = find(null, s.getBytes());
            if (a == null) { println("NOTFOUND: " + s); continue; }
            println("=== \"" + s + "\" @ " + a);
            int n=0;
            for (Reference r : getReferencesTo(a)) {
                Function f = form(r.getFromAddress());
                println("    ref " + r.getFromAddress() + " in " +
                        (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                if (f!=null) fs.add(f); n++;
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
