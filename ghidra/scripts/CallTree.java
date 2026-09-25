import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.util.*;
// prints callers of a function, up to a depth, with the strings each caller references
public class CallTree extends GhidraScript {
    Set<Function> seen = new HashSet<>();
    String strs(Function f) {
        StringBuilder sb = new StringBuilder();
        for (ghidra.program.model.address.Address a : f.getBody().getAddresses(true)) {
            for (Reference r : getReferencesFrom(a)) {
                ghidra.program.model.listing.Data d = getDataAt(r.getToAddress());
                String s = null;
                try {
                    StringBuilder b = new StringBuilder();
                    for (int i = 0; i < 48; i++) { int c = getByte(r.getToAddress().add(i)) & 0xff; if (c == 0) break; if (c < 32 || c > 126) { b = null; break; } b.append((char) c); }
                    if (b != null && b.length() >= 4) s = b.toString();
                } catch (Exception e) {}
                if (s != null && sb.indexOf(s) < 0) sb.append(" \"").append(s).append("\"");
            }
        }
        return sb.toString();
    }
    void walk(Function f, int depth, String ind) {
        if (depth < 0 || !seen.add(f)) return;
        println(ind + f.getEntryPoint() + strs(f));
        for (Reference r : getReferencesTo(f.getEntryPoint())) {
            Function c = getFunctionContaining(r.getFromAddress());
            if (c != null) walk(c, depth - 1, ind + "  ");
            else println(ind + "  <- " + r.getFromAddress() + " " + r.getReferenceType());
        }
    }
    public void run() throws Exception {
        String[] a = getScriptArgs();
        walk(getFunctionAt(toAddr(Long.parseLong(a[0], 16))), Integer.parseInt(a[1]), "");
    }
}
