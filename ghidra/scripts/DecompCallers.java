import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.io.*;
import java.util.*;

// args: outdir hexaddr... ; decompiles each target and all its callers
public class DecompCallers extends GhidraScript {
    Function form(Address ad) {
        Function f = getFunctionContaining(ad);
        if (f != null) return f;
        for (long a = ad.getOffset(); a > ad.getOffset() - 0x1000; a -= 2) {
            Instruction ins = getInstructionAt(toAddr(a));
            if (ins == null) { try { disassemble(toAddr(a)); } catch (Exception e) {} ins = getInstructionAt(toAddr(a)); }
            if (ins != null && ins.getMnemonicString().startsWith("push") && ins.toString().contains("lr")) {
                try { f = createFunction(toAddr(a), null); } catch (Exception e) {}
                if (f != null && f.getBody().contains(ad)) return f;
                return getFunctionContaining(ad);
            }
        }
        return null;
    }
    public void run() throws Exception {
        String[] a = getScriptArgs();
        File out = new File(a[0]);
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Function> todo = new LinkedHashSet<>();
        PrintWriter idx = new PrintWriter(new File(out, "index.txt"));
        for (int i = 1; i < a.length; i++) {
            Address t = toAddr(Long.parseLong(a[i], 16));
            Function tf = getFunctionAt(t);
            if (tf != null) todo.add(tf);
            for (Reference r : getReferencesTo(t)) {
                Function f = form(r.getFromAddress());
                idx.println(a[i] + " " + r.getFromAddress() + " " + (f == null ? "none" : f.getEntryPoint()));
                if (f != null) todo.add(f);
            }
        }
        idx.close();
        for (Function f : todo) {
            DecompileResults dr = di.decompileFunction(f, 120, monitor);
            if (dr == null || !dr.decompileCompleted()) { println("FAIL " + f.getEntryPoint()); continue; }
            PrintWriter w = new PrintWriter(new File(out, f.getEntryPoint().toString() + ".c"));
            w.print(dr.getDecompiledFunction().getC());
            w.close();
        }
        println("decompiled " + todo.size());
    }
}
