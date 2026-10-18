'use strict';

const fs = require('fs');
const path = require('path');

const TYPEDEF = /typedef\s+struct\s+\w*\s*\{([\s\S]*?)\}\s*(\w+)\s*;/g;
const FIELD = /([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*;/;
const TABLE = /^((?:static\s+)?)((?:const\s+)?)(\w+)\s+(\w+)\s*(\[[^\]]*\])?\s*=\s*\{/gm;

function stripComments(s) {
    return s.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, ' ');
}

function parseStructs(headers) {
    const out = {};
    for (const h of headers) {
        const src = stripComments(fs.readFileSync(h, 'latin1'));
        let m;
        TYPEDEF.lastIndex = 0;
        while ((m = TYPEDEF.exec(src)) !== null) {
            const body = m[1];
            const name = m[2];
            const fields = [];
            for (let decl of body.split(';')) {
                decl = decl.trim();
                if (!decl || decl.includes('(')) continue;
                const f = FIELD.exec(decl + ';');
                if (f) {
                    fields.push([f[1], decl.includes('P32')]);
                }
            }
            if (fields.some(([, p]) => p)) {
                out[name] = fields;
            }
        }
    }
    return out;
}

function splitTop(s) {
    const items = [];
    let depth = 0;
    const cur = [];
    let q = null;
    let i = 0;

    while (i < s.length) {
        const c = s[i];
        if (q) {
            cur.push(c);
            if (c === '\\') {
                cur.push(s[i + 1]);
                i++;
            } else if (c === q) {
                q = null;
            }
        } else if (c === '"' || c === "'") {
            q = c;
            cur.push(c);
        } else if (c === '(' || c === '{' || c === '[') {
            depth++;
            cur.push(c);
        } else if (c === ')' || c === '}' || c === ']') {
            depth--;
            cur.push(c);
        } else if (c === ',' && depth === 0) {
            items.push(cur.join(''));
            cur.length = 0;
        } else {
            cur.push(c);
        }
        i++;
    }
    if (cur.join('').trim()) {
        items.push(cur.join(''));
    }
    return items;
}

function bodyEnd(s, start) {
    let depth = 0;
    let q = null;
    let i = start;

    while (i < s.length) {
        const c = s[i];
        if (q) {
            if (c === '\\') {
                i++;
            } else if (c === q) {
                q = null;
            }
        } else if (c === '"' || c === "'") {
            q = c;
        } else if (c === '/' && s[i + 1] === '*') {
            i = s.indexOf('*/', i) + 1;
        } else if (c === '{') {
            depth++;
        } else if (c === '}') {
            depth--;
            if (depth === 0) return i;
        }
        i++;
    }
    throw new Error('unbalanced braces');
}

function isNull(v) {
    const s = stripComments(v).trim();
    return s === '0' || s === 'NULL' || s === '(void *)0' || s === '';
}

/**
 * Transforms a C source containing static tables with P32 pointers into
 * runtime-initialized tables for 64-bit low32 builds.
 */
function transformLow32Static(headers, srcPath, outPath, fixupName) {
    const types = parseStructs(headers);
    let src = fs.readFileSync(srcPath, 'latin1');
    const out = [];
    const fix = [];
    let pos = 0;

    const names = new Set();
    TABLE.lastIndex = 0;
    let tm;
    while ((tm = TABLE.exec(src)) !== null) {
        if (types[tm[3]]) {
            names.add(tm[4]);
        }
    }

    src = src.replace(/^(\s*extern\s+)const\s+(\w+\s+(\w+)\s*(\[[^\]]*\])?\s*;)/gm, (match, p1, p2, p3) => {
        return names.has(p3) ? p1 + p2 : match;
    });

    TABLE.lastIndex = 0;
    while ((tm = TABLE.exec(src)) !== null) {
        const structType = tm[3];
        if (!types[structType]) continue;

        const fields = types[structType];
        const tableName = tm[4];
        const isArray = Boolean(tm[5]);

        const openAt = tm.index + tm[0].length - 1;
        const closeAt = bodyEnd(src, openAt);
        const body = src.slice(openAt + 1, closeAt);
        const rows = isArray ? splitTop(body) : ['{' + body + '}'];
        const newRows = [];

        for (let r = 0; r < rows.length; r++) {
            const row = rows[r];
            const t = row.trim();
            if (!t.startsWith('{')) {
                newRows.push(row);
                continue;
            }
            const lead = row.slice(0, row.length - row.trimStart().length);
            const inner = t.slice(1, t.lastIndexOf('}'));
            const vals = splitTop(inner);

            for (let f = 0; f < Math.min(fields.length, vals.length); f++) {
                const [fname, p32] = fields[f];
                if (p32 && !isNull(vals[f])) {
                    const target = isArray ? `${tableName}[${r}].${fname}` : `${tableName}.${fname}`;
                    fix.push(`    ${target} = ${stripComments(vals[f]).trim()};`);
                    vals[f] = ' 0';
                }
            }
            newRows.push(lead + '{' + vals.join(',') + '}' + t.slice(t.lastIndexOf('}') + 1));
        }

        out.push(src.slice(pos, tm.index));
        out.push(tm[1] + tm[3] + ' ' + tableName + (tm[5] || '') + ' = {');
        out.push(isArray ? newRows.join(',') : newRows[0].slice(1, -1));
        pos = closeAt;
    }

    out.push(src.slice(pos));
    out.push('\n\n/* Generated by tools/low32_static.py: the 32-bit pointers the tables\n');
    out.push(' * above hold, written at start (clang cannot initialize them). */\n');
    out.push(`void ${fixupName}(void)\n{\n${fix.join('\n')}\n}\n`);

    const result = out.join('');
    if (outPath) {
        fs.mkdirSync(path.dirname(outPath), { recursive: true });
        fs.writeFileSync(outPath, result, 'latin1');
    }
    return { fixupsCount: fix.length, content: result };
}

module.exports = { transformLow32Static };
