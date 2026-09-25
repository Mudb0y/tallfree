import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
public class RefsInto extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address lo = toAddr(Long.parseLong(a[0], 16)), hi = toAddr(Long.parseLong(a[1], 16));
        ReferenceManager rm = currentProgram.getReferenceManager();
        AddressIterator it = rm.getReferenceDestinationIterator(new AddressSet(lo, hi), true);
        while (it.hasNext()) {
            Address to = it.next();
            for (Reference r : getReferencesTo(to)) {
                Function f = getFunctionContaining(r.getFromAddress());
                println(to + " <- " + r.getFromAddress() + " " + (f == null ? "-" : f.getEntryPoint().toString()) + " " + r.getReferenceType());
            }
        }
    }
}
