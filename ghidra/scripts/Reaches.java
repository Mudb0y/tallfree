import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.util.*;
// from a root function, walks callees to a depth and prints each path that reaches a target
public class Reaches extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Function root = getFunctionContaining(toAddr(Long.parseLong(a[0], 16)));
        int depth = Integer.parseInt(a[1]);
        Set<Function> targets = new HashSet<>();
        for (int i = 2; i < a.length; i++) {
            Function t = getFunctionContaining(toAddr(Long.parseLong(a[i], 16)));
            if (t != null) targets.add(t); else println("@@@ no function at " + a[i]);
        }
        Map<Function, Function> parent = new HashMap<>();
        Deque<Function> q = new ArrayDeque<>();
        Map<Function, Integer> dist = new HashMap<>();
        q.add(root); dist.put(root, 0);
        while (!q.isEmpty()) {
            Function f = q.poll();
            int d = dist.get(f);
            if (targets.contains(f) && f != root) {
                StringBuilder sb = new StringBuilder();
                for (Function g = f; g != null; g = parent.get(g)) sb.insert(0, g.getEntryPoint() + " ");
                println("@@@ path " + sb);
            }
            if (d >= depth) continue;
            for (Function c : f.getCalledFunctions(monitor)) {
                if (dist.containsKey(c)) continue;
                dist.put(c, d + 1); parent.put(c, f); q.add(c);
            }
        }
        println("@@@ " + root.getEntryPoint() + " reaches " + dist.size() + " functions within " + depth);
    }
}
