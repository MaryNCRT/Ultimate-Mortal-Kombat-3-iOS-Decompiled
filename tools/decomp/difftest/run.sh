#!/bin/sh
# run.sh <stem> <rc_root> [-n scenarios] [-v] [function ...]
#
# Build and run the differential test for one decomp file.
#
# <rc_root> holds one directory per recompiled file (rc_root/<stem>/recompiled.c
# and .h, as made by tools/armrecomp/recomp.py --file). ALL of them are linked
# in: the oracle of a function needs the oracle of everything it calls.
#
# Needs the 32-bit clang toolchain (i686-w64-mingw32-gcc) and UMK3_SLICE, the
# armv7 slice of the binary (tools/umk3paths.py prints it).
set -e
STEM="$1"; RC="$2"; shift 2
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
TC="$HOME/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin"
CC="$TC/i686-w64-mingw32-gcc.exe"
OUT="${DIFFTEST_OUT:-$REPO/work/difftest_$STEM}"
OBJ="${DIFFTEST_OBJ:-$REPO/work/difftest_obj}"
mkdir -p "$OUT" "$OBJ"
[ -n "$UMK3_SLICE" ] || UMK3_SLICE="$(python -c "import sys; sys.path.insert(0,'$REPO/tools/decomp'); import umk3paths; print(umk3paths.require_slice())")"
export UMK3_SLICE

FNS=""; ARGS=""
while [ $# -gt 0 ]; do
    case "$1" in -n) ARGS="$ARGS -n $2"; shift 2;; -v) ARGS="$ARGS -v"; shift;; -d) ARGS="$ARGS -d"; shift;; *) FNS="$FNS $1"; shift;; esac
done

python "$HERE/gen.py" "$STEM" "$RC" "$OUT" $FNS

CFLAGS="-O0 -w -std=gnu11 -fno-strict-aliasing -I $REPO/src/runtime -I $HERE"

# the oracle: every recompiled file, compiled once
OBJS=""
for d in "$RC"/*/; do
    n="$(basename "$d")"
    [ -f "$d/recompiled.c" ] || continue
    if [ ! -f "$OBJ/$n.o" ] || [ "$d/recompiled.c" -nt "$OBJ/$n.o" ]; then
        python "$HERE/patch_blx.py" "$d/recompiled.c" "$OBJ/patched_$n.c" > /dev/null
        "$CC" $CFLAGS -I "$d" -c "$OBJ/patched_$n.c" -o "$OBJ/$n.o" &
    fi
    OBJS="$OBJS $OBJ/$n.o"
done
wait
if [ ! -f "$OBJ/arm_runtime.o" ] || [ "$REPO/src/runtime/arm_runtime.c" -nt "$OBJ/arm_runtime.o" ]; then
    "$CC" $CFLAGS -c "$REPO/src/runtime/arm_runtime.c" -o "$OBJ/arm_runtime.o"
fi
# the generated stubs abort; stubs_extra.c (linked first) gives the few that run
"$CC" $CFLAGS -c "$HERE/stubs_extra.c" -o "$OBJ/stubs_extra.o"
SHIMOBJS=""
for d in "$RC"/*/; do
    n="$(basename "$d")"
    [ -f "$d/recompiled_shims.c" ] || continue
    "$CC" $CFLAGS -I "$d" -c "$d/recompiled_shims.c" -o "$OBJ/shims_$n.o"
    SHIMOBJS="$SHIMOBJS $OBJ/shims_$n.o"
done

"$CC" $CFLAGS -I "$RC/$STEM" -I "$REPO/src/decomp/gamecode/logic" \
    "$HERE/harness.c" "$OUT/gen_tests.c" "$OUT/gen_shims.c" "$OUT/gen_addrmap.c" "$OUT/gen_vars.c" "$OUT/gen_abs.c" "$OUT/gen_oracle.c" \
    "$REPO/src/decomp/gamecode/logic/$STEM.c" "$OBJ/stubs_extra.o" $OBJS "$OBJ/arm_runtime.o" $SHIMOBJS \
    -Wl,--allow-multiple-definition -Wl,--image-base=0x10000000 -o "$OUT/difftest.exe"
"$OUT/difftest.exe" $ARGS $FNS
