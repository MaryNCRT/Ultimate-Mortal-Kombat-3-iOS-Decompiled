"""
symcheck.py — structural check for decompiled C.

A differential test compares OUTPUTS. It cannot see that a function you wrote
calls something that does not exist in the original binary: the test supplies
the same value through the invented call and passes.

That happened once already (`proc_switch_counter` in other.c). This closes it.

Every function name called from decomp/ must exist in the binary's symbol
table, or be a known-legitimate exception (C library, our own helpers, macros).

Usage:
    python tools/symcheck.py decomp/ OUTPUT/symbols.txt
    python tools/symcheck.py decomp/lime/Matrix.c OUTPUT/symbols.txt --verbose
"""

import os
import re
import sys

# Calls that are legitimately not in the binary's symbol table.
ALLOW = {
    # C standard library
    "memcpy", "memset", "memmove", "memcmp", "strlen", "strcpy", "strncpy",
    "strcmp", "strncmp", "strcat", "strstr", "strchr", "sprintf", "snprintf",
    "printf", "fprintf", "puts", "malloc", "calloc", "realloc", "free",
    "abs", "labs", "fabs", "fabsf", "sqrt", "sqrtf", "sin", "sinf", "cos",
    "cosf", "tan", "tanf", "atan", "atan2", "atan2f", "pow", "powf", "floor",
    "floorf", "ceil", "ceilf", "fmod", "fmodf", "exp", "log", "rand", "srand",
    "qsort", "bsearch", "assert", "exit", "abort",
    # C keywords that look like calls
    "if", "for", "while", "switch", "return", "sizeof", "do", "else",
    # our own test/runtime helpers — extend as needed
    "guest_malloc", "guest_free", "arm_load_image",
    # NOTE: `lime_load_file` used to sit here with a justification. It was not a
    # helper -- LIME_LoadMeshSet calls `_limeLoadFile` at armv7 0x0005ea66, and
    # the decompilation had renamed it to snake_case. Allowing it here meant the
    # C no longer named the function the binary calls, and the check that exists
    # to catch exactly that was the thing hiding it. The call sites were renamed
    # back instead; nothing needs allowing.
}

CALL_RE = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
DEF_RE = re.compile(r'^\s*(?:static\s+)?[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;]*\)\s*\{', re.M)
MACRO_RE = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)\b', re.M)
# An Itanium C++ function symbol starts with `_Z` (this Mach-O uses `__Z`)
# followed by the decimal byte length of its identifier. Only identifiers
# recovered by this exact length encoding are accepted as binary names.
MANGLED_RE = re.compile(r'^_+Z(\d+)')


def load_symbols(path):
    """Symbol names from OUTPUT/symbols.txt, with and without leading underscore."""
    syms = set()
    with open(path, "r", errors="replace") as f:
        for line in f:
            for tok in line.split():
                if not re.fullmatch(r"_*[A-Za-z_]\w*", tok):
                    continue
                syms.add(tok)
                syms.add(tok.lstrip("_"))

                # Itanium C++ mangling: _Z <length> <name> <arg encoding>.
                # Recover exactly the declared component; never add a
                # heuristic substring as an alias.
                m = MANGLED_RE.match(tok)
                if m:
                    n = int(m.group(1))
                    begin = m.end(1)
                    name = tok[begin:begin + n]
                    if len(name) == n and re.fullmatch(r"[A-Za-z_]\w*", name):
                        syms.add(name)
    return syms


def strip_comments_and_strings(src):
    src = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
    src = re.sub(r'//[^\n]*', ' ', src)
    src = re.sub(r'"(?:\\.|[^"\\])*"', '""', src)
    src = re.sub(r"'(?:\\.|[^'\\])*'", "''", src)
    return src


# C keywords, which the call regex picks up whenever one is followed by a
# parenthesis. `typedef void (*fn)(void *)` reads as a call to `void()`; so do
# `sizeof (T)`, `return (x)`, `if (` and every cast. None of them can ever be a
# callee, so filtering them is not silencing a finding -- unlike ALLOW, which
# is for real names that are deliberately not in the binary.
C_KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline",
    "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned",
    "void", "volatile", "while", "_Bool", "_Complex", "_Imaginary",
}


def check_file(path, syms, verbose=False):
    src = strip_comments_and_strings(open(path, "r", errors="replace").read())
    local = set(DEF_RE.findall(src)) | set(MACRO_RE.findall(src))
    bad = []
    for name in sorted(set(CALL_RE.findall(src))):
        if name in C_KEYWORDS or name in ALLOW or name in local:
            continue
        if name in syms or ("_" + name) in syms:
            if verbose:
                print("    ok   %s" % name)
            continue
        bad.append(name)
    return bad, local


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    target, symfile = sys.argv[1], sys.argv[2]
    verbose = "--verbose" in sys.argv

    syms = load_symbols(symfile)
    print("symbols loaded: %d" % len(syms))

    files = []
    if os.path.isdir(target):
        for root, _dirs, names in os.walk(target):
            if "_raw" in root:
                continue
            files += [os.path.join(root, n) for n in names if n.endswith((".c", ".h"))]
    else:
        files = [target]

    total_bad = 0
    for fn in sorted(files):
        bad, local = check_file(fn, syms, verbose)
        if bad:
            total_bad += len(bad)
            print("\n%s" % fn)
            for name in bad:
                print("  NOT IN BINARY:  %s()" % name)

    print("\n%s" % ("-" * 52))
    print("files checked:      %4d" % len(files))
    print("unknown callees:    %4d" % total_bad)
    if total_bad:
        print("\nEach one is either a real invention, or a name that belongs in ALLOW.")
        print("Do not silence it without deciding which.")
    return 1 if total_bad else 0


if __name__ == "__main__":
    sys.exit(main())
