// Script headless de Ghidra: decompila una lista de funciones a un archivo .c.
//
// Lee un archivo de texto con una funcion por linea (formato: "0xADDR nombre"
// o solo el nombre) y escribe el C decompilado en el orden dado, que es el
// orden de dificultad que calcula rank.py.
//
//   analyzeHeadless <proyecto> UMK3 -process UMK3.armv7 -noanalysis \
//       -scriptPath "E:\MK3 PROJECT\OUTPUT\tools" \
//       -postScript DecompileList.java <lista.txt> <salida.c>
//
//@category UMK3

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.CharDataType;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeConflictHandler;
import ghidra.program.model.data.StructureDataType;
import ghidra.program.model.data.DoubleDataType;
import ghidra.program.model.data.FloatDataType;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.ShortDataType;
import ghidra.program.model.data.SignedByteDataType;
import ghidra.program.model.data.UnsignedIntegerDataType;
import ghidra.program.model.data.UnsignedShortDataType;
import ghidra.program.model.data.Undefined4DataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.listing.Variable;
import ghidra.program.model.symbol.SourceType;

public class DecompileList extends GhidraScript {

    /** Structs declarados en structs.txt, por nombre. */
    private final Map<String, DataType> customTypes = new LinkedHashMap<>();

    /** Convierte un nombre de tipo del archivo de firmas en un DataType. */
    private DataType parseType(String t) {
        t = t.trim();
        // arrays: "char[64]"
        int arrLen = 0;
        int lb = t.indexOf('[');
        if (lb >= 0 && t.endsWith("]")) {
            arrLen = Integer.parseInt(t.substring(lb + 1, t.length() - 1).trim());
            t = t.substring(0, lb).trim();
        }
        int stars = 0;
        while (t.endsWith("*")) {
            stars++;
            t = t.substring(0, t.length() - 1).trim();
        }
        DataType base;
        // structs declarados en structs.txt
        if (customTypes.containsKey(t)) {
            base = customTypes.get(t);
            for (int i = 0; i < stars; i++) {
                base = new PointerDataType(base);
            }
            return arrLen > 0 ? new ArrayDataType(base, arrLen, base.getLength()) : base;
        }
        switch (t) {
            case "void":   base = VoidDataType.dataType; break;
            case "float":  base = FloatDataType.dataType; break;
            case "double": base = DoubleDataType.dataType; break;
            case "int":    base = IntegerDataType.dataType; break;
            case "uint":   base = UnsignedIntegerDataType.dataType; break;
            case "short":  base = ShortDataType.dataType; break;
            case "ushort": base = UnsignedShortDataType.dataType; break;
            case "char":   base = CharDataType.dataType; break;
            case "byte":   base = SignedByteDataType.dataType; break;
            default:       base = Undefined4DataType.dataType; break;
        }
        for (int i = 0; i < stars; i++) {
            base = new PointerDataType(base);
        }
        return arrLen > 0 ? new ArrayDataType(base, arrLen, base.getLength()) : base;
    }

    /**
     * Crea los structs declarados en structs.txt y los registra en el programa.
     * Se hace en dos pasadas: primero se crean vacios (para que un struct pueda
     * referenciar a otro declarado despues), luego se rellenan los campos.
     */
    private int applyStructs(String path) throws Exception {
        List<String[]> pending = new ArrayList<>();   // {nombre, offset, tipo, campo}
        StructureDataType cur = null;

        try (BufferedReader br = new BufferedReader(new FileReader(new File(path)))) {
            String line;
            while ((line = br.readLine()) != null) {
                String raw = line;
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                if (line.startsWith("struct ")) {
                    String[] p = line.split("\\s+");
                    int size = Integer.decode(p[2]);
                    cur = new StructureDataType(p[1], size);
                    customTypes.put(p[1], cur);
                } else if (cur != null && raw.startsWith(" ")) {
                    String[] p = line.split("\\s+");
                    if (p.length >= 3) {
                        pending.add(new String[] { cur.getName(), p[0], p[1], p[2] });
                    }
                }
            }
        }

        // segunda pasada: ahora todos los nombres de struct son resolubles
        for (String[] f : pending) {
            StructureDataType s = (StructureDataType) customTypes.get(f[0]);
            int off = Integer.decode(f[1]);
            DataType dt = parseType(f[2]);
            try {
                s.replaceAtOffset(off, dt, dt.getLength(), f[3], null);
            } catch (Exception e) {
                println("  campo rechazado " + f[0] + "." + f[3] + ": " + e.getMessage());
            }
        }

        int n = 0;
        for (Map.Entry<String, DataType> e : customTypes.entrySet()) {
            DataType added = currentProgram.getDataTypeManager()
                    .addDataType(e.getValue(), DataTypeConflictHandler.REPLACE_HANDLER);
            customTypes.put(e.getKey(), added);
            n++;
        }
        return n;
    }

    /**
     * Aplica las firmas del archivo dado. Es el paso que convierte
     * `undefined4 *param_1` en `float *m`: sin el, el decompilador no sabe que
     * los datos son coma flotante y emite manipulacion de bits ilegible.
     */
    private int applySignatures(String path, Map<String, Function> byName) throws Exception {
        int applied = 0;
        try (BufferedReader br = new BufferedReader(new FileReader(new File(path)))) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                String[] parts = line.split("\\|");
                if (parts.length < 2) {
                    continue;
                }
                String fname = parts[0].trim();
                Function f = byName.get(fname);
                if (f == null) {
                    println("  firma sin funcion: " + fname);
                    continue;
                }
                DataType ret = parseType(parts[1]);
                List<Variable> params = new ArrayList<>();
                if (parts.length > 2 && !parts[2].trim().isEmpty()) {
                    for (String p : parts[2].split(",")) {
                        p = p.trim();
                        if (p.isEmpty()) {
                            continue;
                        }
                        int sp = Math.max(p.lastIndexOf(' '), p.lastIndexOf('*'));
                        String tname = p.substring(0, sp + 1).trim();
                        String pname = p.substring(sp + 1).trim();
                        if (tname.isEmpty()) {
                            tname = p;
                            pname = "arg" + (params.size() + 1);
                        }
                        params.add(new ParameterImpl(pname, parseType(tname), currentProgram));
                    }
                }
                f.updateFunction(null, new ReturnParameterImpl(ret, currentProgram),
                        params,
                        Function.FunctionUpdateType.DYNAMIC_STORAGE_FORMAL_PARAMS,
                        true, SourceType.USER_DEFINED);
                applied++;
            }
        }
        return applied;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("uso: DecompileList <lista.txt> <salida.c>");
            return;
        }

        // indice de funciones por nombre y por direccion
        Map<String, Function> byName = new LinkedHashMap<>();
        Map<Long, Function> byAddr = new LinkedHashMap<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            byName.put(f.getName(), f);
            byAddr.put(f.getEntryPoint().getOffset(), f);
        }

        List<Function> targets = new ArrayList<>();
        List<String> missing = new ArrayList<>();
        try (BufferedReader br = new BufferedReader(new FileReader(new File(args[0])))) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                String[] parts = line.split("\\s+");
                Function f = null;
                if (parts[0].startsWith("0x")) {
                    long a = Long.parseLong(parts[0].substring(2), 16);
                    f = byAddr.get(a);
                    if (f == null) {
                        // la funcion puede no estar definida todavia: crearla
                        Address addr = currentProgram.getAddressFactory()
                                .getDefaultAddressSpace().getAddress(a);
                        f = createFunction(addr, parts.length > 1 ? parts[1] : null);
                    }
                }
                if (f == null && parts.length > 1) {
                    f = byName.get(parts[1]);
                }
                if (f == null) {
                    f = byName.get(parts[0]);
                }
                if (f != null) {
                    targets.add(f);
                } else {
                    missing.add(line);
                }
            }
        }

        println("solicitadas: " + targets.size() + "  no encontradas: " + missing.size());
        for (String s : missing) {
            println("  NO ENCONTRADA: " + s);
        }

        // Los structs van primero: las firmas pueden referenciarlos.
        if (args.length >= 4 && !args[3].isEmpty() && new File(args[3]).exists()) {
            println("structs creados: " + applyStructs(args[3]));
        }
        if (args.length >= 3 && !args[2].isEmpty() && new File(args[2]).exists()) {
            println("firmas aplicadas: " + applySignatures(args[2], byName));
        }

        DecompInterface decomp = new DecompInterface();
        // Opciones por defecto: grabFromToolAndProgram() necesita un Tool, que
        // en modo headless no existe.
        DecompileOptions opts = new DecompileOptions();
        opts.setMaxWidth(100);
        decomp.setOptions(opts);
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(true);
        decomp.setSimplificationStyle("decompile");
        decomp.openProgram(currentProgram);

        int ok = 0, fail = 0;
        try (PrintWriter out = new PrintWriter(args[1], "UTF-8")) {
            out.println("/*");
            out.println(" * Decompilado de UMK3 iOS (slice armv7) con Ghidra.");
            out.println(" * Generado por OUTPUT/tools/DecompileList.java.");
            out.println(" * Punto de partida: hay que tipar y renombrar a mano.");
            out.println(" */");
            out.println();
            for (Function f : targets) {
                if (monitor.isCancelled()) {
                    break;
                }
                DecompileResults res = decomp.decompileFunction(f, 180, monitor);
                out.println("/* ================================================== */");
                out.println("/* " + f.getName() + "  @ " + f.getEntryPoint()
                        + "   (" + f.getBody().getNumAddresses() + " bytes) */");
                out.println("/* ================================================== */");
                if (res.decompileCompleted() && res.getDecompiledFunction() != null) {
                    out.println(res.getDecompiledFunction().getC());
                    ok++;
                    println("  ok   " + f.getName());
                } else {
                    out.println("/* FALLO: " + res.getErrorMessage() + " */");
                    fail++;
                    println("  FALLO " + f.getName() + ": " + res.getErrorMessage());
                }
                out.println();
            }
        } finally {
            decomp.dispose();
        }
        println("decompiladas: " + ok + "   fallidas: " + fail);
        println("escrito " + args[1]);
    }
}
