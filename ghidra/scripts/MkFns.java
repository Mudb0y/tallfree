import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.app.cmd.function.CreateFunctionCmd;

public class MkFns extends GhidraScript {
    public void run() throws Exception {
        for (String s : getScriptArgs()) {
            Address a = toAddr(Long.parseLong(s, 16));
            disassemble(a);
            Function f = getFunctionAt(a);
            if (f == null) {
                CreateFunctionCmd c = new CreateFunctionCmd(a);
                boolean ok = c.applyTo(currentProgram, monitor);
                f = getFunctionAt(a);
                println(s + (f != null ? " created size " + f.getBody().getNumAddresses() : " FAILED " + c.getStatusMsg()));
            } else println(s + " exists");
        }
    }
}
