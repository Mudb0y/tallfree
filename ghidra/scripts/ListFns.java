import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
public class ListFns extends GhidraScript {
    public void run() throws Exception {
        PrintWriter w = new PrintWriter(getScriptArgs()[0]);
        for (Function f : currentProgram.getFunctionManager().getFunctions(true))
            w.println(f.getEntryPoint() + " " + f.getBody().getMaxAddress());
        w.close();
    }
}
