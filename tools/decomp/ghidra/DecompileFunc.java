// Script headless de Ghidra: decompila a C las funciones cuyo nombre contenga
// la subcadena dada y escribe el resultado en un archivo.
//
//   analyzeHeadless <proyecto> UMK3 -process UMK3.armv7 -noanalysis \
//       -scriptPath "E:\MK3 PROJECT\OUTPUT\tools" \
//       -postScript DecompileFunc.java <subcadena> <archivo_salida>
//
//@category UMK3

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class DecompileFunc extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("uso: DecompileFunc <subcadena> <archivo_salida>");
            return;
        }
        String needle = args[0];
        String outPath = args[1];

        List<Function> targets = new ArrayList<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.getName().contains(needle)) {
                targets.add(f);
            }
        }
        println("funciones que coinciden con '" + needle + "': " + targets.size());

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(outPath, "UTF-8")) {
            for (Function f : targets) {
                if (monitor.isCancelled()) {
                    break;
                }
                out.println("// ===================================================");
                out.println("// " + f.getName() + "  @ " + f.getEntryPoint());
                out.println("// ===================================================");
                DecompileResults res = decomp.decompileFunction(f, 120, monitor);
                if (res.decompileCompleted() && res.getDecompiledFunction() != null) {
                    out.println(res.getDecompiledFunction().getC());
                } else {
                    out.println("// fallo al decompilar: " + res.getErrorMessage());
                }
                out.println();
                println("  decompilada: " + f.getName());
            }
        } finally {
            decomp.dispose();
        }
        println("escrito " + outPath);
    }
}
