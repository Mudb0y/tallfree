import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.listing.Function;
import java.util.*;
public class RefCount extends GhidraScript {
    public void run() throws Exception {
        for (String s : getScriptArgs()) {
            int n = 0, orphan = 0; Set<String> fns = new HashSet<>();
            for (Reference r : getReferencesTo(toAddr(Long.parseLong(s, 16)))) {
                n++; Function f = getFunctionContaining(r.getFromAddress());
                if (f == null) orphan++; else fns.add(f.getEntryPoint().toString());
            }
            println(s + " refs " + n + " functions " + fns.size() + " orphan " + orphan);
        }
    }
}
