import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
// prints the entry of the function containing each address, and every data word pointing at it
public class FnStart extends GhidraScript {
    public void run() throws Exception {
        for (String a : getScriptArgs()) {
            Function f = getFunctionContaining(toAddr(Long.parseLong(a, 16)));
            println("@@@ " + a + " in " + (f == null ? "none" : f.getEntryPoint().toString()));
        }
    }
}
