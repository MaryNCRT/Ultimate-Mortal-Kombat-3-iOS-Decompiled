#!/usr/bin/env python3
"""build.py -- unified multi-platform build script from the user's UMK3 .ipa.

Examples:
    python3 build.py                                  # auto-detects 1.2.59 .ipa in root
    python3 build.py Ultimate_Mortal_Kombat_3.ipa     # builds native target for current OS
    python3 build.py UMK3.ipa --platform windows      # builds Windows .exe (native or cross-compiled via MinGW)
    python3 build.py UMK3.ipa --target headless       # builds & runs 32-bit headless fight engine verification
"""

import argparse
import glob
import os
import platform
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "tools", "build"))

import unpack_ipa  # noqa: E402


def find_ipa():
    """Find a 1.2.59 .ipa in the repository root, or return the first .ipa found."""
    ipas = sorted(glob.glob(os.path.join(ROOT, "*.ipa")))
    if not ipas:
        return None
    for p in ipas:
        if "1.2.59" in os.path.basename(p):
            return p
    return ipas[0]


def can_compile_m32(cc, need_sdl2=False):
    """Check if `cc -m32` can compile and link a minimal 32-bit C program (and optionally -lSDL2)."""
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        c_file = os.path.join(tmp, "t.c")
        exe_file = os.path.join(tmp, "t")
        with open(c_file, "w") as f:
            f.write("int main(void) { return 0; }\n")
        cmd = [cc, "-m32", c_file, "-o", exe_file]
        if need_sdl2:
            cmd.append("-lSDL2")
        r = subprocess.run(cmd, capture_output=True)
        return r.returncode == 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Build UMK3 native port from a legally owned UMK3 1.2.59 .ipa")
    ap.add_argument("ipa", nargs="?", help="Path to UMK3 1.2.59 .ipa (auto-detected in repo root if omitted)")
    ap.add_argument("--platform", choices=["native", "windows", "linux", "macos"], default="native",
                    help="Target platform (default: native host OS)")
    ap.add_argument("--arch", choices=["x86", "x64"], default="x86",
                    help="Target architecture (default: x86 32-bit)")
    ap.add_argument("--build-dir", default=None,
                    help="CMake build directory (default: build-<platform>-<arch>)")
    ap.add_argument("--out-dir", default=None,
                    help="Output directory for the playable game folder (default: dist/<platform>-<arch>)")
    ap.add_argument("--target", choices=["game", "headless", "all"], default="game",
                    help="Which target to build: 'game' (umk3-game), 'headless' (umk3-fight-headless + ctest), or 'all'")
    ap.add_argument("--allow-mismatch", action="store_true",
                    help="Allow building with a non-1.2.59 .ipa (for testing only)")
    args = ap.parse_args(argv)

    ipa_path = args.ipa or find_ipa()
    if not ipa_path or not os.path.isfile(ipa_path):
        print("error: no .ipa specified and none found in %s" % ROOT, file=sys.stderr)
        print("Usage: python3 build.py <path/to/UMK3_1.2.59.ipa>", file=sys.stderr)
        return 1
    ipa_path = os.path.abspath(ipa_path)

    host_sys = platform.system().lower()
    target_plat = args.platform
    need_sdl2 = args.target in ("game", "all")
    if target_plat == "native":
        if host_sys == "windows":
            target_plat = "windows"
        elif host_sys == "darwin":
            target_plat = "macos"
        else:
            cc = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
            if args.arch == "x86" and platform.machine() in ("x86_64", "amd64") and not can_compile_m32(cc, need_sdl2=need_sdl2):
                if shutil.which("i686-w64-mingw32-gcc") or shutil.which("i686-w64-mingw32-clang"):
                    print("note: 32-bit Linux SDL2 (lib32-sdl2 / libsdl2-dev:i386) not installed; using i686-w64-mingw32 cross-compiler (Windows x86 build)")
                    target_plat = "windows"
                else:
                    target_plat = "linux"
            else:
                target_plat = "linux"

    build_dir = os.path.abspath(args.build_dir or os.path.join(ROOT, "build-%s-%s" % (target_plat, args.arch)))
    out_dir = os.path.abspath(args.out_dir or os.path.join(ROOT, "dist", "%s-%s" % (target_plat, args.arch)))
    os.makedirs(build_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    print("== 1/3 Unpacking and verifying .ipa: %s" % os.path.basename(ipa_path))
    bin_path = os.path.join(build_dir, "UMK3.bin")
    unpack_args = [ipa_path, "--bin", bin_path, "--dest", out_dir]
    if args.allow_mismatch:
        unpack_args.append("--allow-mismatch")
    if unpack_ipa.main(unpack_args) != 0:
        return 1

    print("== 2/3 Configuring CMake for %s (%s)" % (target_plat, args.arch))
    cmake_cmd = ["cmake", "-S", ROOT, "-B", build_dir]
    if shutil.which("ninja"):
        cmake_cmd += ["-G", "Ninja"]

    cmake_cmd += [
        "-DUMK3_BINARY=%s" % bin_path,
        "-DUMK3_ASSET_DEST=%s" % out_dir,
    ]

    if target_plat == "windows":
        triple = "x86_64-w64-mingw32" if args.arch == "x64" else "i686-w64-mingw32"
        cc = shutil.which("%s-clang" % triple) or shutil.which("%s-gcc" % triple)
        rc = shutil.which("%s-windres" % triple)
        if not cc:
            print("error: cross-compiler %s-gcc or %s-clang not found on PATH." % (triple, triple), file=sys.stderr)
            return 1
        cmake_cmd += [
            "-DCMAKE_SYSTEM_NAME=Windows",
            "-DCMAKE_C_COMPILER=%s" % cc,
            "-DUMK3_BACKEND=win32",
        ]
        if rc:
            cmake_cmd.append("-DCMAKE_RC_COMPILER=%s" % rc)
    else:
        # Linux / macOS with SDL2
        cc = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
        if args.arch == "x86" and platform.machine() in ("x86_64", "amd64"):
            if can_compile_m32(cc):
                cmake_cmd += [
                    "-DCMAKE_C_COMPILER=%s" % cc,
                    "-DCMAKE_C_FLAGS=-m32",
                    "-DCMAKE_EXE_LINKER_FLAGS=-m32",
                ]
            else:
                print(
                    "error: 32-bit C runtime (gcc-multilib / glibc-devel.i686) is not installed for %s -m32.\n"
                    "  - On Debian/Ubuntu: sudo apt install gcc-multilib libsdl2-dev:i386 libsdl2-mixer-dev:i386\n"
                    "  - Or cross-compile the Windows build right now (works with Wine/Proton):\n"
                    "        python3 build.py %s --platform windows"
                    % (cc, os.path.basename(ipa_path)),
                    file=sys.stderr,
                )
                return 1

    r = subprocess.run(cmake_cmd)
    if r.returncode != 0:
        return r.returncode

    print("== 3/3 Compiling target (%s)" % args.target)
    targets = []
    if args.target == "game":
        targets = ["umk3-game"]
    elif args.target == "headless":
        targets = ["umk3-fight-headless"]
    else:
        targets = ["umk3-fight-headless", "umk3-game"]

    for t in targets:
        r = subprocess.run(["cmake", "--build", build_dir, "--target", t, "--parallel"])
        if r.returncode != 0:
            return r.returncode

    # Copy built executable to out_dir alongside res/ and umk3.ini
    for exe_name in ("umk3-game.exe", "umk3-game"):
        built_exe = os.path.join(build_dir, exe_name)
        if os.path.isfile(built_exe):
            dst_exe = os.path.join(out_dir, exe_name)
            shutil.copy2(built_exe, dst_exe)
            print("\nBuild complete! Playable folder ready at:\n  %s\n  Executable: %s" % (out_dir, dst_exe))
            break

    if args.target in ("headless", "all"):
        print("\n== Running verification tests (ctest)")
        subprocess.run(["ctest", "--test-dir", build_dir, "--output-on-failure"])

    return 0


if __name__ == "__main__":
    sys.exit(main())
