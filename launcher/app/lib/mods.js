/* The MODS section's file side: the mods in <game>/mods/, their order and
 * which are on (mods/mods.ini, which the game reads at start: runtime/mods.c),
 * the game's own files for the viewer (res/, read only), and the exports
 * (<game>/exports/). Nothing here reaches outside the game folder.
 *
 * A mod is a folder; its mod.ini, optional, describes it:
 *
 *     name=HD Kano
 *     author=...
 *     version=1.0
 *     category=textures          textures, audio, music, models, mixed
 *     description=...
 *
 * cover.png (or .jpg; preview.* from before), optional, is its picture in
 * the list, and data.ini the numbers it changes in the fight engine (the
 * CHARACTER EDITOR; runtime/mods.c). Every other file replaces the game file
 * of the same name. */
'use strict';

const fs = require('fs');
const path = require('path');

const CATEGORIES = ['textures', 'audio', 'music', 'models', 'mixed'];
const KIND_OF = {
    '.png': 'textures', '.pvr': 'textures',
    '.wav': 'audio', '.mp3': 'music',
    '.meshset': 'models', '.skin': 'models', '.bones': 'models', '.skinanim': 'models',
    '.scene': 'models', '.events': 'models', '.lighting': 'models', '.lights': 'models', '.offsets': 'models',
};

const modsDir = root => path.join(root, 'mods');
const resDir = root => path.join(root, 'res');

/* a mod's folder name: no separators, no "..", nothing hidden */
function safeName(name) {
    if (typeof name !== 'string') return null;
    const n = name.trim();
    if (!n || n.length > 100 || /[\\/:*?"<>|]/.test(n) || n.startsWith('.') || n.includes('..')) return null;
    return n;
}

/* a path inside a mod or the exports: "Characters/Kano/KANO_DIFFUSE.png",
 * every part a safe name, at most four deep */
function safePath(rel) {
    if (typeof rel !== 'string') return null;
    const parts = rel.split(/[\\/]+/).filter(Boolean);
    if (!parts.length || parts.length > 4) return null;
    const ok = parts.map(safeName);
    return ok.every(Boolean) ? ok : null;
}

function readIni(file) {
    const out = {};
    let text;
    try { text = fs.readFileSync(file, 'utf8'); } catch { return out; }
    for (const line of text.split(/\r?\n/)) {
        const m = /^\s*([A-Za-z_]+)\s*=\s*(.*?)\s*$/.exec(line);
        if (m) out[m[1].toLowerCase()] = m[2];
    }
    return out;
}

function walk(dir, depth = 0) {
    let out = [];
    if (depth > 8) return out;
    let list;
    try { list = fs.readdirSync(dir, { withFileTypes: true }); } catch { return out; }
    for (const e of list) {
        if (e.name.startsWith('.')) continue;
        const p = path.join(dir, e.name);
        if (e.isDirectory()) out = out.concat(walk(p, depth + 1));
        else out.push(p);
    }
    return out;
}

/* mods.ini: on=/off= lines, highest priority first */
function readOrder(root) {
    const out = [];
    let text = '';
    try { text = fs.readFileSync(path.join(modsDir(root), 'mods.ini'), 'utf8'); } catch { /* none yet */ }
    for (const line of text.split(/\r?\n/)) {
        const m = /^(on|off)=(.+)$/.exec(line.trim());
        if (m && safeName(m[2])) out.push({ folder: m[2].trim(), on: m[1] === 'on' });
    }
    return out;
}

function writeOrder(root, order) {
    fs.mkdirSync(modsDir(root), { recursive: true });
    const lines = ['; The MOD MANAGER\'s list (UMK3 launcher). Highest priority first: when two',
        '; mods have a file of the same name, the higher one wins. on= is used, off= is not.'];
    for (const m of order) if (safeName(m.folder)) lines.push((m.on ? 'on=' : 'off=') + m.folder);
    fs.writeFileSync(path.join(modsDir(root), 'mods.ini'), lines.join('\n') + '\n');
}

/* every game file, relative to res/ with forward slashes, by lower case */
let g_resCache = null;
function resIndex(root) {
    if (g_resCache && g_resCache.root === root) return g_resCache;
    const base = resDir(root);
    const files = walk(base).map(p => path.relative(base, p).split(path.sep).join('/'));
    const byLower = new Map(files.map(f => [path.posix.basename(f).toLowerCase(), f]));
    g_resCache = { root, files, byLower };
    return g_resCache;
}

/* The list, in priority order: the named mods first as mods.ini has them,
 * then any folder it does not name (on, as the game takes them). */
/* a mod's own files at its top: never laid over the game */
const isOwn = rel => /^(mod\.ini|data\.ini|(cover|preview)\.(png|jpe?g))$/i.test(rel);

function list(root) {
    const dir = modsDir(root);
    fs.mkdirSync(dir, { recursive: true });
    const folders = fs.readdirSync(dir, { withFileTypes: true })
        .filter(e => e.isDirectory() && !e.name.startsWith('.')).map(e => e.name);
    const order = readOrder(root).filter(o => folders.includes(o.folder));
    for (const f of folders) if (!order.some(o => o.folder === f)) order.push({ folder: f, on: true });

    const res = resIndex(root);
    const claimed = new Map();              /* file name -> the mod that wins it */
    const mods = order.map(o => {
        const base = path.join(dir, o.folder);
        const info = readIni(path.join(base, 'mod.ini'));
        const files = walk(base).filter(p => {
            const rel = path.relative(base, p);
            return !isOwn(rel);
        });
        const kinds = {};
        let unknown = 0, lost = 0;
        const names = [];
        for (const p of files) {
            const n = path.basename(p).toLowerCase();
            const ext = path.extname(n);
            const kind = KIND_OF[ext] || 'other';
            kinds[kind] = (kinds[kind] || 0) + 1;
            /* the game file it replaces: by name, or a texture by stem */
            let target = res.byLower.get(n);
            if (!target && kind === 'textures') {
                const stem = n.slice(0, -ext.length);
                target = res.byLower.get(stem + '.pvr') || res.byLower.get(stem + '.png') || res.byLower.get(stem + '_low.pvr');
            }
            if (!target) unknown++;
            if (o.on) {
                if (claimed.has(n)) lost++;
                else claimed.set(n, o.folder);
            }
            names.push({ file: path.relative(base, p).split(path.sep).join('/'), replaces: target || null });
        }
        const kindList = Object.keys(kinds).filter(k => k !== 'other');
        const category = CATEGORIES.includes(info.category) ? info.category
            : kindList.length === 1 ? kindList[0] : kindList.length ? 'mixed' : 'mixed';
        let preview = null;
        for (const p of ['cover.png', 'cover.jpg', 'cover.jpeg', 'preview.png', 'preview.jpg']) {
            const f = path.join(base, p);
            if (fs.existsSync(f) && fs.statSync(f).size < 4 << 20) {
                preview = 'data:image/' + (p.endsWith('png') ? 'png' : 'jpeg') + ';base64,' + fs.readFileSync(f).toString('base64');
                break;
            }
        }
        return {
            folder: o.folder, on: o.on,
            name: info.name || o.folder, author: info.author || '', version: info.version || '',
            description: info.description || '', category, kinds,
            files: names, count: files.length, unknown, lost, preview,
        };
    });
    return { dir, mods };
}

function save(root, order) {
    if (!Array.isArray(order)) return false;
    writeOrder(root, order.map(o => ({ folder: String(o.folder), on: !!o.on })));
    return true;
}

function create(root, fields) {
    const name = safeName(fields && fields.name);
    if (!name) return { ok: false, error: 'name' };
    const base = path.join(modsDir(root), name);
    if (fs.existsSync(base)) return { ok: false, error: 'exists' };
    fs.mkdirSync(base, { recursive: true });
    setInfo(root, name, fields);
    /* a new mod goes to the top, on */
    const order = readOrder(root).filter(o => o.folder !== name);
    order.unshift({ folder: name, on: true });
    writeOrder(root, order);
    return { ok: true, folder: name };
}

function setInfo(root, folder, fields) {
    const name = safeName(folder);
    if (!name) return false;
    const file = path.join(modsDir(root), name, 'mod.ini');
    const cur = readIni(file);
    for (const k of ['name', 'author', 'version', 'category', 'description'])
        if (fields && typeof fields[k] === 'string') cur[k] = fields[k].replace(/[\r\n]+/g, ' ').slice(0, 400);
    if (cur.category && !CATEGORIES.includes(cur.category)) delete cur.category;
    const text = Object.entries(cur).map(([k, v]) => k + '=' + v).join('\n') + '\n';
    fs.writeFileSync(file, text);
    return true;
}

/* files the player picked, copied into a mod */
function addFiles(root, folder, paths) {
    const name = safeName(folder);
    if (!name || !Array.isArray(paths)) return 0;
    const base = path.join(modsDir(root), name);
    fs.mkdirSync(base, { recursive: true });
    let n = 0;
    for (const p of paths) {
        try {
            fs.copyFileSync(p, path.join(base, path.basename(p)));
            n++;
        } catch { /* skip */ }
    }
    return n;
}

/* bytes the window made (a texture exported to edit), into a mod, at
 * `file` -- a name, or a path in the mod's folders ("Stages/The Pit III/
 * PIT_FLOOR.png"). The game finds a mod's files by name in any folder
 * (runtime/mods.c), so a copy of the same name elsewhere in the mod is
 * removed: one file a name. */
function putFile(root, folder, file, bytes) {
    const name = safeName(folder), parts = safePath(file);
    if (!name || !parts || !(bytes instanceof Uint8Array)) return false;
    const base = path.join(modsDir(root), name);
    const dest = path.join(base, ...parts);
    const lower = parts[parts.length - 1].toLowerCase();
    for (const p of walk(base))
        if (p !== dest && path.basename(p).toLowerCase() === lower && !isOwn(path.relative(base, p)))
            fs.rmSync(p, { force: true });
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    fs.writeFileSync(dest, bytes);
    return true;
}

function modPath(root, folder) {
    const name = folder ? safeName(folder) : '';
    if (folder && !name) return null;
    return path.join(modsDir(root), name || '');
}

/* the picture the player picked, as the mod's cover: PNG bytes, which the
 * window made of it (any format, no wider than 640) */
function setCover(root, folder, png) {
    const name = safeName(folder);
    if (!name || !(png instanceof Uint8Array) || png[0] !== 0x89 || png[1] !== 0x50) return false;
    const base = path.join(modsDir(root), name);
    if (!fs.existsSync(base)) return false;
    for (const old of ['cover.png', 'cover.jpg', 'cover.jpeg', 'preview.png', 'preview.jpg'])
        fs.rmSync(path.join(base, old), { force: true });
    fs.writeFileSync(path.join(base, 'cover.png'), png);
    return true;
}

/* ------------------------------------------------------------ data.ini --
 * `table[word] = value` lines, the CHARACTER EDITOR's changes, as a map. */
function dataRead(root, folder) {
    const name = safeName(folder);
    const out = {};
    if (!name) return out;
    let text = '';
    try { text = fs.readFileSync(path.join(modsDir(root), name, 'data.ini'), 'utf8'); } catch { return out; }
    for (const line of text.split(/\r?\n/)) {
        const m = /^\s*([A-Za-z_]\w*)\[(\d+)\]\s*=\s*(0x[0-9a-f]+|-?\d+)/i.exec(line);
        if (m) out[m[1] + '[' + m[2] + ']'] = Number(m[3]) >>> 0;
    }
    return out;
}

function dataWrite(root, folder, entries) {
    const name = safeName(folder);
    if (!name || !entries || typeof entries !== 'object') return false;
    const base = path.join(modsDir(root), name);
    fs.mkdirSync(base, { recursive: true });
    const lines = ['; The CHARACTER EDITOR\'s numbers (UMK3 launcher): table[word] = value,',
        '; applied by the game at start over its fight tables.'];
    for (const [k, v] of Object.entries(entries))
        if (/^[A-Za-z_]\w*\[\d+\]$/.test(k) && Number.isFinite(v))
            lines.push(k + ' = 0x' + (v >>> 0).toString(16));
    fs.writeFileSync(path.join(base, 'data.ini'), lines.join('\n') + '\n');
    return true;
}

/* ------------------------------------------------------- game files -- */

/* The mods over res/ for the viewer, as the game lays them: every mod that
 * is on (layer '*') or one mod (its folder), by file name and pictures by
 * stem. */
function modIndex(root, layer) {
    const folders = layer === '*'
        ? list(root).mods.filter(m => m.on).map(m => m.folder)
        : [safeName(layer)].filter(Boolean);
    const byName = new Map(), byStem = new Map();
    for (const folder of folders) {
        const base = path.join(modsDir(root), folder);
        for (const p of walk(base)) {
            if (isOwn(path.relative(base, p))) continue;
            const n = path.basename(p).toLowerCase();
            if (!byName.has(n)) byName.set(n, p);
            if (/\.(png|pvr)$/.test(n) && !byStem.has(n.slice(0, -4))) byStem.set(n.slice(0, -4), p);
        }
    }
    return { byName, byStem };
}

function resFiles(root) {
    g_resCache = null;
    return resIndex(root).files;
}

/* one file of res/, or null; nothing outside it. With a layer, a mod's file
 * of that name comes first (a picture by stem, so a .png stands for a .pvr:
 * the window tells them apart by their bytes). */
let g_layer = null;
function resRead(root, rel, layer) {
    if (typeof rel !== 'string') return null;
    if (layer) {
        if (!g_layer || g_layer.key !== root + '|' + layer || Date.now() - g_layer.at > 3000)
            g_layer = { key: root + '|' + layer, at: Date.now(), ...modIndex(root, layer) };
        const n = path.posix.basename(rel).toLowerCase();
        let hit = g_layer.byName.get(n);
        if (!hit && /\.(png|pvr)$/.test(n)) {
            const stem = n.slice(0, -4);
            /* the smaller copies, as the game finds them (runtime/mods.c
             * mods_find_texture): X_LOW, X_LITE by X; X_LITE2, X_LOW2 by X2 */
            const full = stem.replace(/_(low|lite)$/, '').replace(/_(low|lite)2$/, '2');
            hit = g_layer.byStem.get(stem) || (full !== stem && g_layer.byStem.get(full));
        }
        if (hit) try { return new Uint8Array(fs.readFileSync(hit)); } catch { /* the game's, then */ }
    }
    const base = path.resolve(resDir(root));
    const p = path.resolve(base, rel);
    if (!p.startsWith(base + path.sep)) return null;
    try { return new Uint8Array(fs.readFileSync(p)); } catch { return null; }
}

/* The fight engine's tables as the build extracted them from the player's
 * binary (build/logic_tables.c): every word table by name, each word a
 * number or the name of what it points at ("stk_kano_hikick", "x+0x10"). */
function gameTables(root) {
    let text;
    try { text = fs.readFileSync(path.join(root, 'build', 'logic_tables.c'), 'utf8'); } catch { return null; }
    const tables = {};
    const re = /^uintptr_t (\w+)\[(\d+)\] UMK3_LOGIC[^{]*= \{\r?\n([\s\S]*?)\r?\n\};/gm;
    let m;
    while ((m = re.exec(text))) {
        const body = m[3].replace(/UMK3_REL\((\w+), (0x[0-9a-f]+)\)/g, (_, t, o) => '@' + t + (o === '0x0' ? '' : '+' + o));
        tables[m[1]] = body.split(',').map(t => t.trim()).filter(Boolean)
            .map(t => t[0] === '@' ? t.slice(1) : Number(t));
    }
    return tables;
}

/* --------------------------------------------------------- exports -- */

/* into exports\<sub>, where sub is a folder path: "Characters/Kano/FBX" */
function exportSave(root, sub, files) {
    const parts = safePath(sub);
    if (!parts || !files || typeof files !== 'object') return null;
    const dir = path.join(root, 'exports', ...parts);
    fs.mkdirSync(dir, { recursive: true });
    for (const [f, bytes] of Object.entries(files)) {
        const fname = safeName(f);
        if (fname && bytes instanceof Uint8Array) fs.writeFileSync(path.join(dir, fname), bytes);
    }
    return dir;
}

module.exports = { list, save, create, setInfo, addFiles, putFile, modPath, setCover, dataRead, dataWrite,
    resFiles, resRead, gameTables, exportSave, CATEGORIES };
