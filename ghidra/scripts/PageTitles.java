import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class PageTitles extends GhidraScript {
    String strAt(Address a) {
        if (a == null) return null;
        StringBuilder sb = new StringBuilder();
        try {
            for (int i = 0; i < 48; i++) {
                byte b = getByte(a.add(i));
                if (b == 0) break;
                if (b < 32 || b > 126) return null;
                sb.append((char) b);
            }
        } catch (Exception e) { return null; }
        return sb.length() > 1 ? sb.toString() : null;
    }
    public void run() throws Exception {
        Address setter = toAddr(0x800c2558L);
        println("call sites of the page-title setter 0x800C2558:");
        for (Reference r : getReferencesTo(setter)) {
            Address from = r.getFromAddress();
            Function cf = getFunctionContaining(from);
            // look back for the register load that supplies the title pointer
            String title = null;
            for (long a = from.getOffset() - 24; a < from.getOffset(); a += 2) {
                Instruction in = getInstructionAt(toAddr(a));
                if (in == null) continue;
                for (int op = 0; op < in.getNumOperands(); op++)
                    for (Object o : in.getOpObjects(op)) {
                        if (o instanceof Scalar) {
                            long v = ((Scalar) o).getUnsignedValue();
                            if (v > 0x80000000L && v < 0x80245740L) {
                                String s = strAt(toAddr(v));
                                if (s != null) title = s;
                            }
                        }
                    }
                for (Reference rr : in.getReferencesFrom()) {
                    String s = strAt(rr.getToAddress());
                    if (s != null) title = s;
                }
            }
            println(String.format("  %-28s title=%-26s at %s",
                    cf == null ? "<none>" : cf.getName() + " @" + cf.getEntryPoint(),
                    title == null ? "?" : "\"" + title + "\"", from));
        }
    }
}
