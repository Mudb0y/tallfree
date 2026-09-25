import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class DrawStrings extends GhidraScript {
    String strAt(Address a) {
        if (a == null) return null;
        StringBuilder sb = new StringBuilder();
        try {
            for (int i = 0; i < 64; i++) {
                byte b = getByte(a.add(i));
                if (b == 0) break;
                if (b < 32 || b > 126) return null;
                sb.append((char) b);
            }
        } catch (Exception e) { return null; }
        return sb.length() >= 1 ? sb.toString() : null;
    }
    public void run() throws Exception {
        long[] fns = {0x800c2b08L, 0x800c2558L, 0x800c2028L};
        String[] nm = {"draw-string", "page-title", "third"};
        TreeSet<String> found = new TreeSet<>();
        for (int k = 0; k < fns.length; k++) {
            int n = 0;
            for (Reference r : getReferencesTo(toAddr(fns[k]))) {
                Address from = r.getFromAddress();
                n++;
                for (long a = from.getOffset() - 40; a < from.getOffset(); a += 2) {
                    Instruction in = getInstructionAt(toAddr(a));
                    if (in == null) continue;
                    for (int op = 0; op < in.getNumOperands(); op++)
                        for (Object o : in.getOpObjects(op))
                            if (o instanceof Scalar) {
                                long v = ((Scalar) o).getUnsignedValue();
                                if (v > 0x80000000L && v < 0x80300000L) {
                                    String s = strAt(toAddr(v));
                                    if (s != null && s.length() >= 2) found.add(s);
                                }
                            }
                    for (Reference rr : in.getReferencesFrom()) {
                        String s = strAt(rr.getToAddress());
                        if (s != null && s.length() >= 2) found.add(s);
                    }
                }
            }
            println("// " + nm[k] + " 0x" + Long.toHexString(fns[k]) + ": " + n + " call sites");
        }
        println("// distinct strings found: " + found.size());
        for (String s : found) println(s);
    }
}
