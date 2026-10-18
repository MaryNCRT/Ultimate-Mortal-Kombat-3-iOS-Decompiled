"""
Driver del bucle de decompilacion.

Ata las piezas: rank.py ordena las funciones de un archivo fuente por
dificultad, y este script llama a Ghidra headless con DecompileList.java para
volcar el C decompilado a decomp/<subdir>/<Archivo>.c.

Uso:
  python decomp_driver.py Matrix.cpp
  python decomp_driver.py --all-lime
"""

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import umk3paths  # noqa: E402

ROOT = umk3paths.REPO
TOOLS = umk3paths.TOOLS
GHIDRA_SCRIPTS = os.path.join(TOOLS, "ghidra")
PROJNAME = "UMK3"

# Los 9 archivos de src/lime/common, en el orden recomendado del plan.
LIME_COMMON = ["Matrix.cpp", "limeVector.cpp", "RenderMesh.cpp", "RenderScene.cpp",
               "RenderSkinned.cpp", "Events.cpp", "limeFont.cpp",
               "LIMEDS_Misc.cpp", "DS_DebugWin.c"]


def worklist(srcfile):
    """Functions of that source file, ordered from easiest to hardest."""
    from rank import analyze
    return analyze(umk3paths.require_slice(), srcfile)


def run_ghidra(listfile, outfile, sigfile="", structfile=""):
    env = dict(os.environ)
    jdk = os.environ.get("JAVA_HOME")
    if jdk:
        env["PATH"] = os.path.join(jdk, "bin") + os.pathsep + env.get("PATH", "")
    projdir = umk3paths.work_file("ghidra")
    os.makedirs(projdir, exist_ok=True)
    cmd = [umk3paths.analyze_headless(), projdir, PROJNAME,
           "-process", "UMK3.armv7", "-noanalysis",
           "-scriptPath", GHIDRA_SCRIPTS, "-postScript", "DecompileList.java",
           listfile, outfile, sigfile, structfile]
    p = subprocess.run(cmd, env=env, capture_output=True, text=True,
                       errors="replace")
    interesting = []
    for line in (p.stdout or "").splitlines():
        if any(k in line for k in ("solicitadas:", "decompiladas:", "NO ENCONTRADA",
                                   "FALLO", "escrito", "ERROR")):
            interesting.append(line.strip())
    return p.returncode, interesting


def do_file(srcfile, subdir):
    fns = worklist(srcfile)
    if not fns:
        print("!! sin funciones para %s" % srcfile)
        return 0

    # La salida cruda de Ghidra va a _raw/: es material de trabajo, se
    # regenera y no se edita. El C limpio y verificado vive un nivel arriba,
    # en decomp/<subdir>/, y ese si es el producto del proyecto.
    outdir = os.path.join(ROOT, "src", "decomp", subdir, "_raw")
    os.makedirs(outdir, exist_ok=True)
    listdir = umk3paths.work_file("worklist")
    os.makedirs(listdir, exist_ok=True)

    base = os.path.splitext(srcfile)[0]
    listfile = os.path.join(listdir, base + ".txt")
    with open(listfile, "w", encoding="utf-8") as fh:
        fh.write("# %s -- %d funciones, ordenadas de facil a dificil\n"
                 % (srcfile, len(fns)))
        fh.write("# addr      nombre                              score\n")
        for f in fns:
            fh.write("0x%08x %s   # score %.0f, %d ins\n"
                     % (f.addr, f.name, f.score, f.insns))

    outfile = os.path.join(outdir, base + ".c")
    print("\n=== %s: %d funciones -> %s" % (srcfile, len(fns),
                                            os.path.relpath(outfile, ROOT)))
    sigdir = os.path.join(TOOLS, "signatures")
    sigfile = os.path.join(sigdir, "lime.txt")
    structfile = os.path.join(sigdir, "structs.txt")
    rc, lines = run_ghidra(listfile, outfile,
                           sigfile if os.path.exists(sigfile) else "",
                           structfile if os.path.exists(structfile) else "")
    for line in lines:
        print("   " + line)
    return len(fns)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("srcfile", nargs="?")
    ap.add_argument("--all-lime", action="store_true")
    ap.add_argument("--subdir", default="lime")
    args = ap.parse_args()

    total = 0
    if args.all_lime:
        for f in LIME_COMMON:
            total += do_file(f, args.subdir)
    elif args.srcfile:
        total += do_file(args.srcfile, args.subdir)
    else:
        ap.error("indica un archivo fuente o --all-lime")
    print("\ntotal de funciones procesadas: %d" % total)


if __name__ == "__main__":
    main()
