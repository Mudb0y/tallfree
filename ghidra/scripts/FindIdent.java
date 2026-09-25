import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.lang.OperandType;
import java.util.*;

public class FindIdent extends GhidraScript {
    public void run() throws Exception {
        // constants that a universal-SysEx identity handler must touch
        long[] want = {0xF0, 0xF7, 0x7E, 0x7F, 0x41, 0x0408};
        Map<Function, Set<Long>> hit = new LinkedHashMap<>();
        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        int nf = 0;
        while (fi.hasNext() && !monitor.isCancelled()) {
            Function f = fi.next();
            nf++;
            Set<Long> s = new TreeSet<>();
            InstructionIterator ii =
                currentProgram.getListing().getInstructions(f.getBody(), true);
            while (ii.hasNext()) {
                Instruction in = ii.next();
                for (int op = 0; op < in.getNumOperands(); op++) {
                    Object[] objs = in.getOpObjects(op);
                    for (Object o : objs) {
                        if (o instanceof Scalar) {
                            long v = ((Scalar) o).getUnsignedValue();
                            for (long w : want) if (v == w) s.add(v);
                        }
                    }
                }
            }
            if (s.contains(0x7EL) && s.contains(0xF0L)) hit.put(f, s);
            if (s.contains(0x408L)) hit.put(f, s);
        }
        println("scanned functions: " + nf);
        println("=== functions touching both 0xF0 and 0x7E, or the family code 0x408 ===");
        for (Map.Entry<Function, Set<Long>> e : hit.entrySet()) {
            StringBuilder sb = new StringBuilder();
            for (Long v : e.getValue()) sb.append(String.format("0x%X ", v));
            println(String.format("%-40s @%s  consts: %s",
                e.getKey().getName(), e.getKey().getEntryPoint(), sb.toString()));
        }
        println("total: " + hit.size());
    }
}
