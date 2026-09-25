import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.nio.file.*;
import java.util.*;

public class MkMissing extends GhidraScript {
    long orphans() {
        long n = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) { Instruction i = it.next(); if (getFunctionContaining(i.getAddress()) == null) n++; }
        return n;
    }
    boolean mk(Address a) {
        if (getFunctionAt(a) != null) return false;
        if (getInstructionAt(a) == null) return false;
        return new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
    }
    public void run() throws Exception {
        println("orphan instructions before: " + orphans());
        Set<Address> calls = new TreeSet<>();
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferenceIterator(toAddr(0x80000000L));
        while (ri.hasNext()) { Reference r = ri.next(); if (r.getReferenceType().isCall()) calls.add(r.getToAddress()); }
        int c = 0; for (Address a : calls) if (mk(a)) c++;
        println("created from calls: " + c);
        int p = 0;
        for (String s : Files.readAllLines(Paths.get(getScriptArgs()[0]))) if (mk(toAddr(Long.parseLong(s.trim(), 16)))) p++;
        println("created from pointers: " + p);
        println("orphan instructions after: " + orphans());
    }
}
