import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.*;
import java.util.*;
import java.util.regex.*;

public class MapPages extends GhidraScript {
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
        // FUN_800c2558 is the page-title setter, as seen in the iMX reboot page
        Address titleFn = toAddr(0x800c2558L);
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Pattern p = Pattern.compile("FUN_800c2558\\(([^,]+),([^,]+),");
        Set<Long> seen = new HashSet<>();
        int n=0;
        for (Reference r : getReferencesTo(titleFn)) {
            Function f = form(r.getFromAddress());
            if (f == null) continue;
            if (!seen.add(f.getEntryPoint().getOffset())) continue;
            DecompileResults dr = di.decompileFunction(f, 60, monitor);
            if (dr == null || !dr.decompileCompleted()) continue;
            String c = dr.getDecompiledFunction().getC();
            Matcher m = p.matcher(c);
            while (m.find()) {
                println(f.getEntryPoint() + "  title=" + m.group(2).trim());
                n++;
            }
        }
        println("total title-setting sites: " + n);
    }
}
