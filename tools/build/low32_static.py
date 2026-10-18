"""low32_static.py -- the 64-bit build's copy of a C file whose static tables
fill 32-bit pointer fields (P32, runtime/savestate.h).

    python tools/low32_static.py <header>... -- <in.c> <out.c> <fixup name>

In the 64-bit build (UMK3_LOW32) a P32 field is clang's 32-bit pointer, and
clang cannot put an address into one in a static initializer. So the copy
keeps every table, with each such value replaced by 0 and `const` dropped
from the table, and ends with `void <fixup name>(void)`, which writes the
values in at start, before anything reads them. The structures are read
from the headers given (`typedef struct X { ... } NAME;`); a table is a
definition `[static] [const] NAME name[...] = { ... };` of one of the
structures that has a P32 field. Port, not in the binary.
"""
import re
import sys

TYPEDEF = re.compile(r'typedef\s+struct\s+\w*\s*\{(.*?)\}\s*(\w+)\s*;', re.S)
FIELD = re.compile(r'([A-Za-z_]\w*)\s*(\[[^]]*\])?\s*;')
TABLE = re.compile(r'^((?:static\s+)?)((?:const\s+)?)(\w+)\s+(\w+)\s*(\[[^]]*\])?\s*=\s*\{', re.M)


def strip_comments(s):
    return re.sub(r'/\*.*?\*/|//[^\n]*', ' ', s, flags=re.S)


def structs(headers):
    out = {}
    for h in headers:
        src = strip_comments(open(h, encoding='latin-1').read())
        for m in TYPEDEF.finditer(src):
            fields = []
            for decl in m.group(1).split(';'):
                decl = decl.strip()
                if not decl or '(' in decl:
                    continue
                f = FIELD.search(decl + ';')
                if f:
                    fields.append((f.group(1), 'P32' in decl))
            if any(p for _, p in fields):
                out[m.group(2)] = fields
    return out


def split_top(s):
    """Top-level comma-separated items of an initializer body."""
    items, depth, cur, q = [], 0, [], None
    i = 0
    while i < len(s):
        c = s[i]
        if q:
            cur.append(c)
            if c == '\\':
                cur.append(s[i + 1])
                i += 1
            elif c == q:
                q = None
        elif c in '"\'':
            q = c
            cur.append(c)
        elif c in '({[':
            depth += 1
            cur.append(c)
        elif c in ')}]':
            depth -= 1
            cur.append(c)
        elif c == ',' and depth == 0:
            items.append(''.join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    if ''.join(cur).strip():
        items.append(''.join(cur))
    return items


def body_end(s, start):
    """Index of the brace closing the one at `start`."""
    depth, q, i = 0, None, start
    while i < len(s):
        c = s[i]
        if q:
            if c == '\\':
                i += 1
            elif c == q:
                q = None
        elif c in '"\'':
            q = c
        elif c == '/' and s[i + 1] == '*':
            i = s.index('*/', i) + 1
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise SystemExit('unbalanced braces')


def is_null(v):
    v = strip_comments(v).strip()
    return v in ('0', 'NULL', '(void *)0', '')


def main(argv):
    k = argv.index('--')
    headers, (src_path, out_path, fixup) = argv[:k], argv[k + 1:]
    types = structs(headers)
    src = open(src_path, encoding='latin-1', newline='').read()
    out, fix, pos = [], [], 0
    names = set(m.group(4) for m in TABLE.finditer(src) if m.group(3) in types)
    # the file's own `extern const NAME table[];` would clash with the table
    # without its const
    src = re.sub(r'^(\s*extern\s+)const\s+(\w+\s+(\w+)\s*(\[[^]]*\])?\s*;)',
                 lambda m: m.group(1) + m.group(2) if m.group(3) in names else m.group(0),
                 src, flags=re.M)
    for m in TABLE.finditer(src):
        if m.group(3) not in types:
            continue
        fields = types[m.group(3)]
        name, is_array = m.group(4), m.group(5) is not None
        open_at = m.end() - 1
        close_at = body_end(src, open_at)
        body = src[open_at + 1:close_at]
        rows = split_top(body) if is_array else ['{' + body + '}']
        new_rows = []
        for r, row in enumerate(rows):
            t = row.strip()
            if not t.startswith('{'):
                new_rows.append(row)
                continue
            lead = row[:len(row) - len(row.lstrip())]
            inner = t[1:t.rindex('}')]
            vals = split_top(inner)
            for f, (fname, p32) in enumerate(fields[:len(vals)]):
                if p32 and not is_null(vals[f]):
                    target = '%s[%d].%s' % (name, r, fname) if is_array else '%s.%s' % (name, fname)
                    fix.append('    %s = %s;' % (target, strip_comments(vals[f]).strip()))
                    vals[f] = ' 0'
            new_rows.append(lead + '{' + ','.join(vals) + '}' + t[t.rindex('}') + 1:])
        out.append(src[pos:m.start()])
        out.append(m.group(1) + m.group(3) + ' ' + name + (m.group(5) or '') + ' = {')
        out.append(','.join(new_rows) if is_array else new_rows[0][1:-1])
        pos = close_at
    out.append(src[pos:])
    out.append('\n\n/* Generated by tools/low32_static.py: the 32-bit pointers the tables\n'
               ' * above hold, written at start (clang cannot initialize them). */\n')
    out.append('void %s(void)\n{\n%s\n}\n' % (fixup, '\n'.join(fix)))
    open(out_path, 'w', encoding='latin-1', newline='\n').write(''.join(out))
    print('low32_static: %d values -> %s' % (len(fix), out_path))


if __name__ == '__main__':
    main(sys.argv[1:])
