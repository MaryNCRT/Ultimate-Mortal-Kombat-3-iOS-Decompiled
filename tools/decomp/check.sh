#!/bin/sh
# check.sh -- syntax-check every C file in the tree, the way it is really built.
#
# A naive `for f in $(find . -name '*.c')` sweep is WRONG here and reported a
# phantom error for a while: it compiled the Linux backend on Windows without
# SDL2 and called the missing header a defect. CMake never builds that file on
# Windows. So this script picks the backend the way CMakeLists.txt does.
#
# The off-platform backend is then linted separately rather than skipped,
# because a file nobody compiles is a file where a typo lives forever. It uses
# a real SDL2 when one is installed and tests/sdl2-lint/SDL.h when not -- and
# says which, because only the first of those is a real check.
#
#   sh tools/check.sh
#
# Exit status is the number of errors, so CI can use it directly.

cd "$(dirname "$0")/../.." || exit 99
CC=${CC:-gcc}
# -O2 -c, NOT -fsyntax-only. Parsing cannot see a whole class of defect: it
# took an optimising build to report "iteration 9 invokes undefined behavior"
# in MatrixIdentity2, which had been walking off the end of SKINMATRIX43.m
# into .t for as long as the file has existed. A checker that cannot detect a
# known positive is not evidence of anything -- so this one compiles.
FLAGS="-std=gnu11 -Wall -Wextra -O2 -c -include src/runtime/savestate.h -D_snprintf=snprintf -D_strdup=strdup -I src/runtime -I src/decomp/lime -I src/decomp/gamecode/logic"
OBJ=${TMPDIR:-/tmp}/umk3-check.o

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) NATIVE=win32_gl.c ; OTHER=sdl_gl.c   ;;
    *)                    NATIVE=sdl_gl.c   ; OTHER=win32_gl.c ;;
esac
# each backend is a window file and an audio file
case "$NATIVE" in
    win32_gl.c) NATIVE_AUDIO=win32_audio.c ; OTHER_AUDIO=sdl_audio.c   ;;
    *)          NATIVE_AUDIO=sdl_audio.c   ; OTHER_AUDIO=win32_audio.c ;;
esac

errors=0
warnings=0

check_one() {
    out=$("$CC" $FLAGS $2 "$1" -o "$OBJ" 2>&1)
    e=$(printf '%s' "$out" | grep -c 'error:')
    w=$(printf '%s' "$out" | grep -c 'warning:')
    errors=$((errors + e))
    warnings=$((warnings + w))
    [ "$e" -gt 0 ] && printf '%s\n' "$out" | grep 'error:'
    return 0
}

echo "=== tree ==="
for dir in src/decomp/lime src/decomp/gamecode src/runtime; do
    de=$errors ; dw=$warnings
    for f in $(find "$dir" -name '*.c' | grep -v '/platform/' | grep -v '/legacy/'); do
        check_one "$f" ""
    done
    printf '  %-22s %d errors, %d warnings\n' \
           "$dir" "$((errors - de))" "$((warnings - dw))"
done

echo
echo "=== platform backend (native: $NATIVE) ==="
if [ "$NATIVE" = "sdl_gl.c" ]; then
    inc=$(pkg-config --cflags sdl2 SDL2_mixer 2>/dev/null)
    if [ -n "$inc" ]; then
        native_extra="$inc"
    else
        echo "  SDL2/SDL2_mixer not fully installed: using tests/sdl2-lint"
        native_extra="-DUMK3_SDL2_LINT -I tests/sdl2-lint"
    fi
else
    native_extra=""
fi
pe=$errors
check_one "src/runtime/platform/$NATIVE" "$native_extra"
printf '  %-22s %d errors\n' "$NATIVE" "$((errors - pe))"
pe=$errors
check_one "src/runtime/platform/$NATIVE_AUDIO" "$native_extra"
printf '  %-22s %d errors\n' "$NATIVE_AUDIO" "$((errors - pe))"

echo
echo "=== platform backend (off-platform: $OTHER) ==="
if [ "$OTHER" = "sdl_gl.c" ]; then
    inc=$(pkg-config --cflags sdl2 2>/dev/null)
    if [ -n "$inc" ]; then
        echo "  using REAL SDL2 -- this is a full check"
        extra="$inc"
    else
        echo "  SDL2 not installed: using tests/sdl2-lint/SDL.h"
        echo "  REDUCED CHECK -- catches our own mistakes, cannot prove the"
        echo "  declarations match real SDL2. Install SDL2 to close that gap."
        extra="-DUMK3_SDL2_LINT -I tests/sdl2-lint"
    fi
else
    echo "  win32 backend needs the Windows SDK headers; skipped on this host"
    extra=""
    OTHER=""
fi

if [ -n "$OTHER" ]; then
    oe=$errors
    check_one "src/runtime/platform/$OTHER" "$extra"
    printf '  %-22s %d errors\n' "$OTHER" "$((errors - oe))"
    oe=$errors
    check_one "src/runtime/platform/$OTHER_AUDIO" "$extra"
    printf '  %-22s %d errors\n' "$OTHER_AUDIO" "$((errors - oe))"
fi

# ---------------------------------------------------------------- stray bytes
#
# Not a compiler check -- a source-hygiene one, and it has caught real damage.
# Writing a file through an UNQUOTED shell heredoc makes the shell interpret
# backslash escapes, so a comment that MENTIONS an escape sequence lands in the
# file as the literal control byte instead. src/decomp/lime/RenderMesh.c carried a
# NUL inside a comment for a long time: the compiler is perfectly happy with it
# and grep just reports the file as binary, so nothing complained.
#
# Write source with a QUOTED heredoc or an editor tool, never an unquoted one.
echo
echo "=== stray control bytes ==="
stray=0
for f in $(LC_ALL=C grep -rlaP '[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]' --include='*.c' --include='*.h' --include='*.py' --include='*.sh' src tools tests 2>/dev/null); do
    printf '  %s has control bytes\n' "$f"
    stray=$((stray + 1))
    errors=$((errors + 1))
done
[ "$stray" = "0" ] && echo "  none"

# Each file is compiled ALONE above, so a prototype that contradicts a
# definition in another file passes every check here. That is how one function
# came to have three prototypes in three files, one of them passing a pointer
# where the definition took an integer, and how two functions that return a
# value were written as `void` -- the callers using the value were in other
# files. Cross-file agreement needs its own pass.
echo
echo "=== prototypes against their definitions ==="
python3 tools/decomp/protos.py --quiet || true
echo "  (tools/decomp/protos.py, without --quiet, says which)"

# A third pass, for a defect neither the compiler nor the prototypes can see.
# mk3_push_handler carries the state-0 refusal with it, so reaching for it from
# a dispatcher state whose token is non-zero refuses every time: it compiles,
# the types agree, and the routine returns -3 where the binary installs. 116 of
# these were live at once.
echo
echo "=== installs that refuse where the binary does not ==="
python3 tools/decomp/instck.py | tail -1

echo
rm -f "$OBJ"
echo "TOTAL: $errors errors, $warnings warnings"
exit $errors
