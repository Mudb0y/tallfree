import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class FindSysExParser extends GhidraScript {
    public void run() throws Exception {
        long[] want = {0xF0, 0xF7, 0x41, 0x11, 0x12, 0x7A, 0x7C};
        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        List<String> out = new ArrayList<>();
        while (fi.hasNext() && !monitor.isCancelled()) {
            Function f = fi.next();
            long size = f.getBody().getNumAddresses();
            if (size > 2500) continue;                 // parsers are small
            Set<Long> s = new TreeSet<>();
            InstructionIterator ii =
                currentProgram.getListing().getInstructions(f.getBody(), true);
            while (ii.hasNext()) {
                Instruction in = ii.next();
                for (int op = 0; op < in.getNumOperands(); op++)
                    for (Object o : in.getOpObjects(op))
                        if (o instanceof Scalar) {
                            long v = ((Scalar) o).getUnsignedValue();
                            for (long w : want) if (v == w) s.add(v);
                        }
            }
            // a SysEx handler must know both frame bytes and the Roland id
            if (s.contains(0xF0L) && s.contains(0x41L) &&
                (s.contains(0x12L) || s.contains(0x11L) || s.contains(0xF7L))) {
                StringBuilder sb = new StringBuilder();
                for (Long v : s) sb.append(String.format("0x%X ", v));
                out.add(String.format("%-22s @%s  size %4d  consts: %s",
                        f.getName(), f.getEntryPoint(), size, sb));
            }
        }
        println("=== candidate SysEx parsers ===");
        for (String s : out) println(s);
        println("count: " + out.size());
    }
}
