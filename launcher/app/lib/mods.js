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
 * of the same name.
 *
 * A mod whose mod.ini says type=skin replaces nothing: it is an OUTFIT that
 * adds, another look for one fighter beside the game's (runtime/mods.c,
 * "skins that add"). skin= is its own name, character= the fighter's file
 * name (SONYA), model= / color1= / color2= / ice= its files when they do not
 * have the game's names. The viewer reads such a mod as the game does
 * (skinRead below): its files first, the game's where it has none. */
'use strict';

const fs = require('fs');
const path = require('path');

const CATEGORIES = ['textures', 'audio', 'music', 'models', 'mixed', 'skin'];
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
        const m = /^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$/.exec(line);
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
/* ---------------------------------------------------- outfits (skins) --
 * The same rules as the game's (runtime/mods.c skin_take, skin_file,
 * skin_tex, skin_guess_colors), so the viewer shows what the fight will. */
function skinOf(base) {
    const info = readIni(path.join(base, 'mod.ini'));
    if (String(info.type || '').toLowerCase() !== 'skin') return null;
    const files = walk(base).filter(p => !isOwn(path.relative(base, p)));
    const byName = new Map(), byStem = new Map();
    for (const p of files) {
        const n = path.basename(p).toLowerCase();
        if (!byName.has(n)) byName.set(n, p);
        if (/\.(png|pvr)$/.test(n) && !byStem.has(n.slice(0, -4))) byStem.set(n.slice(0, -4), p);
    }
    const stem = v => String(v || '').toLowerCase().replace(/\.(png|pvr)$/, '');
    let model = String(info.model || '').toLowerCase();
    if (!model || !byName.has(model)) {
        const all = [...byName.keys()].filter(n => n.endsWith('.skin'));
        model = all.length === 1 ? all[0] : '';
    }
    let character = String(info.character || '').toLowerCase();
    if (!character && model) character = model.split(/[_.]/)[0];
    const color = [stem(info.color1), stem(info.color2), stem(info.ice)];
    /* the sheets by their own names, when mod.ini and the game's names do not say */
    if (!color[0] && !(character && byStem.has(character + '_diffuse'))) {
        const mstem = model.replace(/\..*$/, '');
        for (const s of [...byStem.keys()].sort()) {
            if (mstem && s.startsWith(mstem + '_m')) continue;
            if (/_lite|_low/.test(s)) continue;
            const w = s.includes('ice') ? 2 : s.endsWith('2') ? 1 : 0;
            if (!color[w]) color[w] = s;
        }
    }
    return { title: info.skin || info.name || path.basename(base), character, model, color, byName, byStem };
}

const g_skins = new Map();
function skinCached(root, folder) {
    const key = root + '|' + folder, c = g_skins.get(key);
    if (c && Date.now() - c.at < 3000) return c.k;
    const k = skinOf(path.join(modsDir(root), folder));
    g_skins.set(key, { at: Date.now(), k });
    return k;
}

/* a file of outfit k by the game's name, or null */
function skinFile(k, n) {
    let hit = k.byName.get(n);
    if (!hit && k.model) {
        const m = /\.skin(\..*)?$/.exec(n);
        if (m) hit = k.byName.get(k.model + (m[1] || ''));
    }
    return hit || null;
}
function skinTex(k, key) {
    const i = key.lastIndexOf('_diffuse');
    if (i < 0) return k.byStem.get(key) || null;
    const suf = key.slice(i + 8), base = key.slice(0, i);
    const which = ['', '_low', '_lite'].includes(suf) ? 0
        : ['2', '2_low', '2_lite', '_lite2', '_low2'].includes(suf) ? 1 : suf === '_ice' ? 2 : -1;
    if (which < 0) return k.byStem.get(key) || null;
    const conv = ['_diffuse', '_diffuse2', '_diffuse_ice'];
    return k.byStem.get(key) || k.byStem.get(base + conv[which])
        || (k.color[which] && k.byStem.get(k.color[which]))
        || (which ? k.byStem.get(base + conv[0]) || (k.color[0] && k.byStem.get(k.color[0])) : null)
        || null;
}
function skinRead(k, rel) {
    const n = path.posix.basename(rel).toLowerCase();
    let hit = skinFile(k, n);
    if (!hit && /\.(png|pvr)$/.test(n)) hit = skinTex(k, n.slice(0, -4));
    return hit;
}

/* A new OUTFIT for a fighter: a mod of type=skin that starts as a copy of
 * what the fighter wears now (the game's, under the mods that are on) --
 * its colour sheets, and its model when a mod replaced it -- so it can be
 * changed without touching the original. `texBase` the fighter's file name
 * (SONYA), `skinFile` its model (SONYA_STANDARD.skin). */
function skinCreate(root, fields) {
    const texBase = String(fields && fields.texBase || '').toUpperCase();
    const skinFileName = String(fields && fields.skinFile || '');
    const title = String(fields && fields.title || '').replace(/[\r\n=]+/g, ' ').trim().slice(0, 40);
    if (!/^[A-Z0-9_]+$/.test(texBase) || !title) return { ok: false, error: 'name' };
    let folder = safeName(title + ' (' + texBase.charAt(0) + texBase.slice(1).toLowerCase() + ' outfit)');
    if (!folder) folder = safeName(texBase + ' outfit');
    if (!folder) return { ok: false, error: 'name' };
    for (let i = 2; fs.existsSync(path.join(modsDir(root), folder)); i++)
        folder = safeName(title + ' (' + texBase + ' outfit ' + i + ')');
    const base = path.join(modsDir(root), folder);
    fs.mkdirSync(base, { recursive: true });
    fs.writeFileSync(path.join(base, 'mod.ini'), ['name=' + title, 'type=skin', 'skin=' + title,
        'character=' + texBase, 'category=skin', 'version=1.0', 'author=',
        'description=' + title + ': another outfit for ' + texBase + ', beside the original', ''].join('\n'));
    /* the default, copied: the sheets as the game shows them now */
    const res = resIndex(root), on = modIndex(root, '*');
    const game = n => res.byLower.get(n) && path.join(resDir(root), res.byLower.get(n));
    for (const suf of ['_DIFFUSE', '_DIFFUSE2', '_DIFFUSE_ICE']) {
        const stem = texBase + suf, lo = stem.toLowerCase();
        const from = on.byStem.get(lo) || game(lo + '.pvr') || game(lo + '.png');
        if (from) try { fs.copyFileSync(from, path.join(base, stem + path.extname(from).toLowerCase())); } catch { /* without it */ }
    }
    /* a model a mod put on the fighter goes with it; the game's is found anyway */
    if (/^[A-Za-z0-9_]+\.skin$/.test(skinFileName)) {
        const idx = modIndex(root, '*');
        for (const ext of ['', '.mats', '.bones']) {
            const p = idx.byName.get((skinFileName + ext).toLowerCase());
            if (p) try { fs.copyFileSync(p, path.join(base, skinFileName + ext)); } catch { /* skip */ }
        }
    }
    const order = readOrder(root).filter(o => o.folder !== folder);
    order.unshift({ folder, on: true });
    writeOrder(root, order);
    writeMeta(root, folder);
    g_skins.clear();
    return { ok: true, folder };
}

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
/* the mod's own files, not the game's: its settings, its cover, its
 * mod.meta.xml, and what the windows keep to edit again (a model's import
 * settings, an animation's corrective layers, the animation before them) */
const isOwn = rel => /^(mod\.ini|data\.ini|mod\.meta\.xml|(cover|preview)\.(png|jpe?g))$/i.test(rel)
    || /\.(import|layers)\.json$|\.orig$|\.source$/i.test(rel);

/* mod.meta.xml: what the mod is and every file in it, with the game file
 * each replaces -- rewritten whenever the mod changes, for other tools and
 * for sharing the mod */
const xmlEsc = v => String(v).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
function writeMeta(root, folder) {
    const name = safeName(folder);
    if (!name) return;
    const base = path.join(modsDir(root), name);
    if (!fs.existsSync(base)) return;
    const info = readIni(path.join(base, 'mod.ini'));
    const res = resIndex(root);
    const files = walk(base).map(p => path.relative(base, p).split(path.sep).join('/'))
        .filter(rel => !/^mod\.meta\.xml$/i.test(rel)).sort();
    const lines = ['<?xml version="1.0" encoding="UTF-8"?>', '<mod format="1">',
        '  <name>' + xmlEsc(info.name || name) + '</name>',
        '  <folder>' + xmlEsc(name) + '</folder>',
        '  <author>' + xmlEsc(info.author || '') + '</author>',
        '  <version>' + xmlEsc(info.version || '') + '</version>',
        '  <category>' + xmlEsc(info.category || '') + '</category>',
        '  <description>' + xmlEsc(info.description || '') + '</description>',
        '  <game>Ultimate Mortal Kombat 3, PC port</game>',
        '  <updated>' + new Date().toISOString() + '</updated>',
        '  <files count="' + files.length + '">'];
    for (const rel of files) {
        const n = path.basename(rel).toLowerCase();
        const own = isOwn(rel) || isOwn(path.basename(rel));
        const target = own ? null : res.byLower.get(n);
        lines.push('    <file path="' + xmlEsc(rel) + '"' + (own ? ' own="1"' : '') + (target ? ' replaces="' + xmlEsc(target) + '"' : '') + ' size="' + fs.statSync(path.join(base, rel)).size + '"/>');
    }
    lines.push('  </files>', '</mod>', '');
    fs.writeFileSync(path.join(base, 'mod.meta.xml'), lines.join('\n'));
}

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
            if (o.on && String(info.type || '').toLowerCase() !== 'skin') {
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
        const sk = skinOf(base);
        return {
            folder: o.folder, on: o.on,
            skin: sk ? { title: sk.title, character: sk.character } : null,
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
    for (const k of ['name', 'author', 'version', 'category', 'description', 'skin'])
        if (fields && typeof fields[k] === 'string') cur[k] = fields[k].replace(/[\r\n]+/g, ' ').slice(0, 400);
    if (cur.category && !CATEGORIES.includes(cur.category)) delete cur.category;
    const text = Object.entries(cur).map(([k, v]) => k + '=' + v).join('\n') + '\n';
    fs.writeFileSync(file, text);
    writeMeta(root, name);
    g_skins.clear();
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
    writeMeta(root, name);
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
    writeMeta(root, name);
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
        ? list(root).mods.filter(m => m.on && !m.skin).map(m => m.folder)
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
    /* an OUTFIT (type=skin): its files by the game's rules, then what the
     * game shows without it (the mods that are on, the game) */
    const sk = layer && layer !== '*' && safeName(layer) ? skinCached(root, safeName(layer)) : null;
    if (sk) {
        const hit = skinRead(sk, rel);
        if (hit) try { return new Uint8Array(fs.readFileSync(hit)); } catch { /* then the rest */ }
        layer = '*';
    }
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

module.exports = { list, save, create, setInfo, addFiles, putFile, modPath, setCover, dataRead, dataWrite, skinCreate,
    resFiles, resRead, gameTables, exportSave, CATEGORIES };
