import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;

public class FindSrc extends GhidraScript {
    public void run() throws Exception {
        // ITCM image: literal 0x400D8000 lives at file 0xc648 -> address 0xBF58
        long[] litAddrs = {0xBF58L};
        LinkedHashSet<Function> fs = new LinkedHashSet<>();
        for (long la : litAddrs) {
            Address ad = toAddr(la);
            println("=== references to literal at " + ad);
            for (Reference r : getReferencesTo(ad)) {
                Function f = getFunctionContaining(r.getFromAddress());
                println("    from " + r.getFromAddress() + " in " +
                        (f==null?"<none>":f.getName()+" @"+f.getEntryPoint()));
                if (f != null) fs.add(f);
            }
        }
        // also: any function whose body references 0x400D8xxx
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
