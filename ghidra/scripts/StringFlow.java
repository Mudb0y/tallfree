import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

// Traces the string argument of every call into the text-drawing wrappers back
// to its source. A caller that forwards one of its own parameters becomes a new
// sink, so wrappers of wrappers are followed to wherever the text originates.
// args: outfile  entry:argslot ...   (argslot counts from 1, as in pcode CALL inputs)
public class StringFlow extends GhidraScript {
    DecompInterface di;
    Map<Function, HighFunction> cache = new HashMap<>();
    Map<Long, Integer> sinks = new LinkedHashMap<>();
    Deque<Long> work = new ArrayDeque<>();
    PrintWriter out;
    Function cur;
    String site;

    HighFunction hf(Function f) {
        if (cache.containsKey(f)) return cache.get(f);
        DecompileResults r = di.decompileFunction(f, 120, monitor);
        HighFunction h = (r != null && r.decompileCompleted()) ? r.getHighFunction() : null;
        cache.put(f, h);
        return h;
    }

    String str(long a) {
        try {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < 200; i++) {
                int b = getByte(toAddr(a + i)) & 0xff;
                if (b == 0) break;
                if (b < 32 || b > 126) return null;
                sb.append((char) b);
            }
            return sb.toString();
        } catch (Exception e) { return null; }
    }

    boolean inImage(long v) { return v >= 0x80000000L && v < 0x80245740L; }

    void emit(String kind, String detail) {
        out.println(cur.getEntryPoint() + "\t" + site + "\t" + kind + "\t" + detail);
    }

    Long constOf(Varnode v, int depth) {
        if (v == null || depth > 8) return null;
        if (v.isConstant()) return v.getOffset();
        if (v.isAddress() && !v.isRegister()) return null;
        PcodeOp d = v.getDef();
        if (d == null) return null;
        switch (d.getOpcode()) {
            case PcodeOp.COPY: case PcodeOp.CAST: case PcodeOp.INT_ZEXT: case PcodeOp.INT_SEXT:
                return constOf(d.getInput(0), depth + 1);
            case PcodeOp.PTRSUB: case PcodeOp.INT_ADD: {
                Long a = constOf(d.getInput(0), depth + 1), b = constOf(d.getInput(1), depth + 1);
                return (a != null && b != null) ? a + b : null;
            }
        }
        return null;
    }

    void table(long base, String how) {
        StringBuilder sb = new StringBuilder();
        int n = 0;
        for (int i = 0; i < 400; i++) {
            long p;
            try { p = getInt(toAddr(base + 4L * i)) & 0xffffffffL; } catch (Exception e) { break; }
            String s = inImage(p) ? str(p) : null;
            if (s == null) break;
            sb.append(n++ == 0 ? "" : " | ").append(s);
        }
        emit("TABLE", String.format("%s base=0x%x n=%d: %s", how, base, n, sb));
    }

    void resolve(Varnode v, int depth, Set<Varnode> seen) {
        if (v == null) { emit("UNKNOWN", "null"); return; }
        if (depth > 12 || !seen.add(v)) return;
        Long c = constOf(v, 0);
        if (c != null) {
            String s = inImage(c) ? str(c) : null;
            if (s != null) emit("LITERAL", String.format("0x%x \"%s\"", c, s));
            else emit("CONST", String.format("0x%x", c));
            return;
        }
        HighVariable hv = v.getHigh();
        if (hv instanceof HighParam && v.getDef() == null) {
            int slot = ((HighParam) hv).getSlot() + 1;
            long e = cur.getEntryPoint().getOffset();
            emit("PARAM", "slot " + slot + " -> caller becomes a sink");
            if (!sinks.containsKey(e)) { sinks.put(e, slot); work.add(e); }
            return;
        }
        PcodeOp d = v.getDef();
        if (d == null) { emit("UNKNOWN", "no def " + v); return; }
        switch (d.getOpcode()) {
            case PcodeOp.COPY: case PcodeOp.CAST: case PcodeOp.INT_ZEXT: case PcodeOp.INT_SEXT:
                resolve(d.getInput(0), depth + 1, seen); return;
            case PcodeOp.MULTIEQUAL:
                for (Varnode in : d.getInputs()) resolve(in, depth + 1, seen);
                return;
            case PcodeOp.INDIRECT:
                resolve(d.getInput(0), depth + 1, seen); return;
            case PcodeOp.LOAD: {
                Varnode addr = d.getInput(1);
                Long ca = constOf(addr, 0);
                if (ca != null) { emit("GLOBAL", String.format("*0x%x", ca)); return; }
                PcodeOp ad = addr.getDef();
                if (ad != null && (ad.getOpcode() == PcodeOp.PTRADD || ad.getOpcode() == PcodeOp.INT_ADD)) {
                    Long b0 = constOf(ad.getInput(0), 0), b1 = constOf(ad.getInput(1), 0);
                    Long base = b0 != null ? b0 : b1;
                    if (base != null && inImage(base)) { table(base, "indexed"); return; }
                }
                emit("LOAD", "from " + (ad == null ? addr.toString() : ad.toString()));
                return;
            }
            case PcodeOp.PTRADD: case PcodeOp.INT_ADD: case PcodeOp.PTRSUB: {
                Varnode a = d.getInput(0), b = d.getInput(1);
                if (a.isRegister() || (a.getHigh() != null && a.getHigh().getName() != null && a.getSpace() == currentProgram.getAddressFactory().getStackSpace().getSpaceID())) {}
                if (d.getOpcode() == PcodeOp.PTRSUB && a.isRegister() || (a.getDef() == null && a.getAddress() != null && a.getAddress().isStackAddress())) {
                    buffer(b.isConstant() ? b.getOffset() : -1); return;
                }
                Long ca = constOf(a, 0), cb = constOf(b, 0);
                if (ca != null && inImage(ca) && d.getOpcode() == PcodeOp.PTRADD) { emit("OFFSET", String.format("0x%x + index*%d \"%s\"", ca, d.getInput(2).getOffset(), str(ca))); return; }
                emit("EXPR", d.toString());
                return;
            }
            case PcodeOp.CALL: {
                Address t = d.getInput(0).getAddress();
                emit("CALLRET", t.toString());
                return;
            }
        }
        emit("OTHER", d.toString());
    }

    // A stack buffer handed to the draw call: collect the constant strings passed
    // to any call that also receives the same buffer, which catches sprintf,
    // strcpy and strcat alike.
    void buffer(long off) {
        HighFunction h = hf(cur);
        Set<String> fm = new LinkedHashSet<>();
        Iterator<PcodeOpAST> it = h.getPcodeOps();
        while (it.hasNext()) {
            PcodeOpAST op = it.next();
            if (op.getOpcode() != PcodeOp.CALL) continue;
            boolean mine = false;
            for (int i = 1; i < op.getNumInputs(); i++) {
                PcodeOp dd = op.getInput(i).getDef();
                if (dd != null && dd.getOpcode() == PcodeOp.PTRSUB && dd.getInput(1).isConstant() && dd.getInput(1).getOffset() == off) mine = true;
            }
            if (!mine) continue;
            for (int i = 1; i < op.getNumInputs(); i++) {
                Long c = constOf(op.getInput(i), 0);
                if (c != null && inImage(c)) { String s = str(c); if (s != null) fm.add(op.getInput(0).getAddress() + ":\"" + s + "\""); }
            }
        }
        emit("BUFFER", String.format("stack 0x%x %s", off, String.join(" ", fm)));
    }

    public void run() throws Exception {
        String[] a = getScriptArgs();
        out = new PrintWriter(new FileWriter(a[0]));
        di = new DecompInterface();
        di.openProgram(currentProgram);
        for (int i = 1; i < a.length; i++) {
            String[] p = a[i].split(":");
            long e = Long.parseLong(p[0], 16);
            sinks.put(e, Integer.parseInt(p[1]));
            work.add(e);
        }
        while (!work.isEmpty()) {
            long e = work.poll();
            int slot = sinks.get(e);
            out.println("#SINK\t" + Long.toHexString(e) + "\t" + slot);
            for (Reference r : getReferencesTo(toAddr(e))) {
                if (!r.getReferenceType().isCall()) continue;
                Function f = getFunctionContaining(r.getFromAddress());
                if (f == null) { out.println("#ORPHAN\t" + r.getFromAddress()); continue; }
                HighFunction h = hf(f);
                cur = f; site = r.getFromAddress().toString();
                if (h == null) { emit("NODECOMP", ""); continue; }
                boolean found = false;
                Iterator<PcodeOpAST> it = h.getPcodeOps(r.getFromAddress());
                while (it.hasNext()) {
                    PcodeOpAST op = it.next();
                    if (op.getOpcode() != PcodeOp.CALL && op.getOpcode() != PcodeOp.CALLIND) continue;
                    found = true;
                    if (op.getNumInputs() <= slot) { emit("NOARG", op.toString()); continue; }
                    resolve(op.getInput(slot), 0, new HashSet<>());
                }
                if (!found) emit("NOCALLOP", "");
            }
            out.flush();
        }
        out.close();
        println("sinks " + sinks.size());
    }
}
