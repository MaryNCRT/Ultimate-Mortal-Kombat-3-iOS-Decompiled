"""instck.py -- is this install allowed to refuse?

`mk3_push_handler` is the whole state-0 shape: it REFUSES with -3 when the
token slot is not clear, and only then replaces the handler. That is right for
a routine whose one state is entry -- those write the guard inline and the
binary really does test the slot. It is wrong for the later states of a
dispatcher, whose token is non-zero by construction: there the binary stores
the handler with no test at all, so a guard we emit is a refusal the game
never makes.

The audit is source-level on purpose. It cannot prove a site wrong -- only the
disassembly does that -- but it points at the sites worth re-reading, and the
class it finds is one no compiler and no prototype check can see.
"""
import re
import sys
import glob
import os

DEF = re.compile(r"^(?:static\s+)?(?:long|void|int32_t|uint32_t|float)\s+\**\w+\(")
TOKEN_EQ = re.compile(r"\btoken\s*==\s*(0x[0-9a-fA-F]+|\d+)")
TOKEN_NE = re.compile(r"\btoken\s*!=\s*(0x[0-9a-fA-F]+|\d+)")
TOKEN_DECL = re.compile(r"\btoken\s*=[^=]")
# The condition may not contain ; { or }, or an `if (a) {` several statements
# earlier pairs with THIS `return -3;` and swallows the real guard whole. That
# is what hid 99 of these behind "no token test at all".
REFUSE = re.compile(r"if\s*\(([^;{}]*?)\)\s*return\s+-3\s*;", re.S)


def functions(text):
    """Yield (signature, [(lineno, line), ...]) for each definition."""
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        if DEF.match(lines[i]) and not lines[i].rstrip().endswith(";"):
            start = i
            while i < len(lines) and lines[i].strip() != "{":
                i += 1
            depth, body = 0, []
            while i < len(lines):
                depth += lines[i].count("{") - lines[i].count("}")
                body.append((i + 1, lines[i]))
                i += 1
                if depth == 0:
                    break
            yield lines[start].strip(), body
        else:
            i += 1


def token_at(body):
    """For each line, the innermost `if (token == N)` in force, or None.

    Depth is counted BEFORE the line's own braces, so the `if` line itself is
    outside the block it opens and the closing brace is outside it too.
    """
    marks = {}
    stack = []                  # (depth the block's body sits at, token value)
    depth = 0
    pending = None              # a brace-less `if (token == N)` still in force
    for lineno, line in body:
        while stack and stack[-1][0] > depth:
            stack.pop()
        marks[lineno] = pending if pending is not None else (
            stack[-1][1] if stack else None)
        if pending is not None and ";" in line:
            pending = None
        m = TOKEN_EQ.search(line) if "if" in line else None
        opens = line.count("{")
        depth += opens - line.count("}")
        if m:
            if opens:
                stack.append((depth, int(m.group(1), 0)))
            elif ";" not in line:
                # `if (token == N)` with the statement on the following lines
                pending = int(m.group(1), 0)
    return marks


def entry_guard(body):
    """The set the opening refusal leaves, or None if there is no refusal.

    `if (token != 0 && token != 0x9b1 && token != 0x9bd) return -3;` says the
    token is one of those three from there on. When 0 is not among them, an
    install that no `token == N` narrows further is reached with a non-zero
    token -- the same fault, one level up.
    """
    text = "\n".join(l for _, l in body)
    for m in REFUSE.finditer(text):
        cond = m.group(1)
        vals = TOKEN_NE.findall(cond)
        if vals and "==" not in cond:
            return {int(v, 0) for v in vals}
    return None


def audit(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for sig, body in functions(text):
        if not any("mk3_push_handler" in l for _, l in body):
            continue
        # A single-state routine writes its guard inline and installs, which is
        # exactly what mk3_push_handler is. Only a routine that keeps the token
        # in a variable has states to get wrong.
        if not any(TOKEN_DECL.search(l) for _, l in body):
            continue
        marks = token_at(body)
        guard = entry_guard(body)
        for lineno, line in body:
            if "mk3_push_handler" not in line:
                continue
            tok = marks[lineno]
            if tok is not None:
                if tok != 0:
                    out.append((lineno, sig, "reached with token %#x" % tok))
            elif guard is None:
                out.append((lineno, sig, "no token test at all"))
            elif 0 not in guard:
                out.append((lineno, sig, "guard leaves token in {%s}"
                            % ", ".join("%#x" % v for v in sorted(guard))))
    return out


def main():
    files = sys.argv[1:] or sorted(glob.glob("src/decomp/gamecode/logic/*.c"))
    bad = 0
    for p in files:
        for lineno, sig, why in audit(p):
            bad += 1
            print("%s:%d  %s  <- %s"
                  % (os.path.basename(p), lineno, sig.split("(")[0], why))
    print("instck: %d guarded installs a dispatcher state can reach" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
