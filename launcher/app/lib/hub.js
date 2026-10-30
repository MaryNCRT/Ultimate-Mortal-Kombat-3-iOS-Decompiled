/* The MOD HUB: mods published in the companion repository
 * MaryNCRT/UMK3-IOS-Mod-Hub, listed, downloaded into <game>/mods/ and
 * updated. Only mods, never game data.
 *
 * The repository holds index.json on its main branch; each mod's files are
 * one .zip attached to a release of that repository. Only its owner and the
 * collaborators with write access can make releases or change index.json,
 * and fetchIndex() takes no download from anywhere else, so the HUB shows
 * only what they published.
 *
 *     { "format": 1, "mods": [ { "id": "rain", "folder": "Rain",
 *       "name": "Rain", "author": "...", "version": "1.0",
 *       "category": "textures", "description": "...",
 *       "thumbnail": "mods/rain/cover.png",
 *       "url": "https://github.com/MaryNCRT/UMK3-IOS-Mod-Hub/releases/download/rain-v1.0/rain-1.0.zip",
 *       "size": 1234, "sha256": "<64 hex>" } ] }
 *
 * What is installed from the HUB is kept in mods/hub.json (id -> folder,
 * version, sha256). A mod counts as installed when hub.json names it or
 * when mods/<folder> is already there (a mod copied by hand, as Mary had
 * hers). A new install downloads the whole zip; a mod already there is
 * compared file by file with the zip's central directory (size, CRC-32),
 * read with HTTP ranges, and UPDATE downloads only the files that differ
 * and deletes the ones the zip no longer has. Either way it keeps its place
 * and on/off in mods.ini; a new one goes to the top, on. */
'use strict';

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { spawnSync } = require('child_process');
const upd = require('./update');

const REPO = 'MaryNCRT/UMK3-IOS-Mod-Hub';
const RAW = 'https://raw.githubusercontent.com/' + REPO + '/main/';
const INDEX = RAW + 'index.json';
const DL = 'https://github.com/' + REPO + '/releases/download/';
const CATEGORIES = ['textures', 'audio', 'music', 'models', 'mixed'];
const LINUX = process.platform === 'linux';

const modsDir = root => path.join(root, 'mods');

/* a mod's folder name, as lib/mods.js takes it */
function safeName(name) {
    if (typeof name !== 'string') return null;
    const n = name.trim();
    if (!n || n.length > 100 || /[\\/:*?"<>|]/.test(n) || n.startsWith('.') || n.includes('..')) return null;
    return n;
}

const str = (v, max) => typeof v === 'string' ? v.replace(/[\r\n]+/g, ' ').trim().slice(0, max) : '';

/* index.json's text -> the mods it lists that are well formed; an entry
 * whose download is not one of this repository's releases is dropped */
function parseIndex(text) {
    let j;
    try { j = typeof text === 'string' ? JSON.parse(text) : text; } catch { throw new Error('index.json is not valid JSON'); }
    if (!j || !Array.isArray(j.mods)) throw new Error('index.json has no mods list');
    const seen = new Set();
    const out = [];
    for (const m of j.mods) {
        if (!m || typeof m !== 'object') continue;
        const id = str(m.id, 60).toLowerCase();
        if (!/^[a-z0-9][a-z0-9-]*$/.test(id) || seen.has(id)) continue;
        const folder = safeName(m.folder || m.name);
        const version = str(m.version, 20);
        const url = str(m.url, 400);
        const file = url.slice(url.lastIndexOf('/') + 1);
        if (!folder || !version || !url.startsWith(DL) || !/^[\w.\-]+\.zip$/i.test(file)) continue;
        const sha256 = str(m.sha256, 64).toLowerCase();
        const thumb = str(m.thumbnail, 200);
        seen.add(id);
        out.push({
            id, folder, version, url, file,
            name: str(m.name, 60) || folder,
            author: str(m.author, 60),
            category: CATEGORIES.includes(m.category) ? m.category : 'mixed',
            description: str(m.description, 400),
            thumbnail: /^[\w\-/. ]+\.(png|jpe?g)$/i.test(thumb) && !thumb.includes('..') ? thumb : '',
            size: Number.isFinite(m.size) && m.size > 0 ? m.size : 0,
            sha256: /^[0-9a-f]{64}$/.test(sha256) ? sha256 : '',
        });
    }
    return out;
}

function readInstalled(root) {
    try {
        const j = JSON.parse(fs.readFileSync(path.join(modsDir(root), 'hub.json'), 'utf8'));
        return j && typeof j === 'object' ? j : {};
    } catch { return {}; }
}

function writeInstalled(root, inst) {
    fs.mkdirSync(modsDir(root), { recursive: true });
    fs.writeFileSync(path.join(modsDir(root), 'hub.json'), JSON.stringify(inst, null, 2) + '\n');
}

/* where a listed mod is in this folder: the folder hub.json gave it, else
 * mods/<its folder> when that exists; null when it is not here */
function folderOf(root, inst, m) {
    const i = inst[m.id];
    if (i && safeName(i.folder) && fs.existsSync(path.join(modsDir(root), i.folder))) return i.folder;
    return fs.existsSync(path.join(modsDir(root), m.folder)) ? m.folder : null;
}

/* each listed mod with what this folder has of it: installed (its version:
 * hub.json's, else its mod.ini's; null when it is not here), and update
 * when the hub's version is newer (a folder whose mod.ini has no version
 * is told only by its files). A HUB mod whose folder was deleted
 * counts as not installed. check() adds the files that differ. */
function status(root, list) {
    const inst = readInstalled(root);
    return list.map(m => {
        const folder = folderOf(root, inst, m);
        let installed = null;
        if (folder) {
            const i = inst[m.id];
            installed = i && i.folder === folder ? String(i.version || '')
                : (readIni(path.join(modsDir(root), folder, 'mod.ini')).version || '');
        }
        return Object.assign({}, m, {
            installed, here: folder,
            update: !!installed && upd.compareVersions(m.version, installed) > 0,
        });
    });
}

function readIni(file) {
    const out = {};
    let text = '';
    try { text = fs.readFileSync(file, 'utf8'); } catch { return out; }
    for (const line of text.split(/\r?\n/)) {
        const mm = /^\s*([A-Za-z_]+)\s*=\s*(.*?)\s*$/.exec(line);
        if (mm) out[mm[1].toLowerCase()] = mm[2];
    }
    return out;
}

/* ------------------------------------------------ the zip, by ranges ---- */

/* bytes [a, b] of the mod's zip; throws when the server answers with the
 * whole file instead (no range support) */
async function range(fetchFn, mod, a, b) {
    const r = await fetchFn(mod.url, { headers: { 'User-Agent': 'UMK3-Launcher', Range: 'bytes=' + a + '-' + b } });
    if (r.status !== 206) throw new Error('no range reads (HTTP ' + r.status + ')');
    return Buffer.from(await r.arrayBuffer());
}

/* the zip's files from its central directory: { path inside the mod
 * folder, crc, csize, usize, method, off }. The zip holds the mod's folder,
 * so its first part goes; folders and unsafe names are left out. */
async function zipFiles(fetchFn, mod) {
    if (!mod.size) throw new Error('the index has no size for this mod');
    const tailAt = Math.max(0, mod.size - 65557);
    const tail = await range(fetchFn, mod, tailAt, mod.size - 1);
    const e = tail.lastIndexOf(Buffer.from([0x50, 0x4b, 5, 6]));
    if (e < 0) throw new Error('not a zip');
    const cdSize = tail.readUInt32LE(e + 12), cdOff = tail.readUInt32LE(e + 16);
    if (cdOff === 0xffffffff || cdOff + cdSize > mod.size) throw new Error('zip64 is not supported');
    const cd = cdOff >= tailAt ? tail.subarray(cdOff - tailAt, cdOff - tailAt + cdSize) : await range(fetchFn, mod, cdOff, cdOff + cdSize - 1);
    const out = [];
    for (let p = 0; p + 46 <= cd.length && cd.readUInt32LE(p) === 0x02014b50;) {
        const nl = cd.readUInt16LE(p + 28), el = cd.readUInt16LE(p + 30), kl = cd.readUInt16LE(p + 32);
        const name = cd.toString('utf8', p + 46, p + 46 + nl).replace(/\\/g, '/');
        const ent = { crc: cd.readUInt32LE(p + 16), csize: cd.readUInt32LE(p + 20), usize: cd.readUInt32LE(p + 24),
            method: cd.readUInt16LE(p + 10), off: cd.readUInt32LE(p + 42) };
        p += 46 + nl + el + kl;
        if (name.endsWith('/')) continue;
        const parts = name.split('/').filter(Boolean);
        if (parts.length < 2 || parts.some(s => s === '..' || s === '.' || /[:*?"<>|]/.test(s))) continue;
        ent.path = parts.slice(1).join('/');
        out.push(ent);
    }
    return out;
}

let g_crcTable = null;
function crc32(buf) {
    if (zlib.crc32) return zlib.crc32(buf) >>> 0;
    if (!g_crcTable) {
        g_crcTable = new Uint32Array(256);
        for (let n = 0; n < 256; n++) {
            let c = n;
            for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
            g_crcTable[n] = c >>> 0;
        }
    }
    let c = 0xffffffff;
    for (let i = 0; i < buf.length; i++) c = g_crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
}

/* the mod's own files the launcher writes, never counted as changes */
const isOwn = rel => /^mod\.meta\.xml$/i.test(rel);

function localFiles(base, rel = '', out = []) {
    let list;
    try { list = fs.readdirSync(path.join(base, rel), { withFileTypes: true }); } catch { return out; }
    for (const e of list) {
        const r = rel ? rel + '/' + e.name : e.name;
        if (e.isDirectory()) localFiles(base, r, out);
        else out.push(r);
    }
    return out;
}

/* what differs between mods/<folder> and the zip: { get: the zip's files
 * missing or different here, drop: files here the zip does not have }.
 * deep compares CRC-32 too, otherwise sizes only (fast, for the list). */
function diff(root, folder, files, deep) {
    const base = path.join(modsDir(root), folder);
    const want = new Map(files.map(f => [f.path.toLowerCase(), f]));
    const get = [];
    for (const f of files) {
        const p = path.join(base, f.path);
        let st;
        try { st = fs.statSync(p); } catch { st = null; }
        if (!st || st.size !== f.usize || (deep && crc32(fs.readFileSync(p)) !== f.crc)) get.push(f);
    }
    const drop = localFiles(base).filter(r => !isOwn(r) && !want.has(r.toLowerCase()));
    return { get, drop };
}

/* each installed mod's files that differ by size (one or two range reads
 * of its zip's end); changes stays null when the zip cannot be read so */
async function check(fetchFn, root, list) {
    const st = status(root, list);
    await Promise.all(st.map(async m => {
        m.changes = null;
        if (!m.here) return;
        try {
            const d = diff(root, m.here, await zipFiles(fetchFn, m), false);
            m.changes = d.get.length + d.drop.length;
            m.changeBytes = d.get.reduce((s, f) => s + f.csize, 0);
        } catch { /* left unknown */ }
    }));
    return st;
}

/* One file out of the zip by its range: the local header's lengths are
 * not known before reading it, so the read takes some room for them. */
async function fetchEntry(fetchFn, mod, f) {
    let buf = await range(fetchFn, mod, f.off, Math.min(mod.size - 1, f.off + 30 + 1024 + f.csize - 1));
    if (buf.readUInt32LE(0) !== 0x04034b50) throw new Error('bad zip entry ' + f.path);
    const start = 30 + buf.readUInt16LE(26) + buf.readUInt16LE(28);
    if (buf.length < start + f.csize) buf = await range(fetchFn, mod, f.off, f.off + start + f.csize - 1);
    const raw = buf.subarray(start, start + f.csize);
    const data = f.method === 8 ? zlib.inflateRawSync(raw) : f.method === 0 ? raw : null;
    if (!data) throw new Error('unsupported zip method ' + f.method);
    if (data.length !== f.usize || crc32(data) !== f.crc) throw new Error('CRC does not match: ' + f.path);
    return data;
}

/* UPDATE of a mod already here: only the files that differ (CRC-32), each
 * downloaded and checked, written over; the ones the zip no longer has
 * deleted. Returns how many were fetched and dropped. */
async function sync(fetchFn, root, mod, onProgress) {
    const inst = readInstalled(root);
    const folder = folderOf(root, inst, mod);
    if (!folder) throw new Error('not installed');
    const files = await zipFiles(fetchFn, mod);
    const d = diff(root, folder, files, true);
    const base = path.join(modsDir(root), folder);
    const total = d.get.reduce((s, f) => s + f.csize, 0);
    let got = 0;
    if (onProgress) onProgress(0, total);
    for (const f of d.get) {
        const data = await fetchEntry(fetchFn, mod, f);
        const p = path.join(base, f.path);
        fs.mkdirSync(path.dirname(p), { recursive: true });
        fs.writeFileSync(p + '.part', data);
        fs.renameSync(p + '.part', p);
        got += f.csize;
        if (onProgress) onProgress(got, total);
    }
    for (const r of d.drop) fs.rmSync(path.join(base, r), { force: true });
    register(root, folder);
    inst[mod.id] = { folder, version: mod.version, sha256: mod.sha256 };
    writeInstalled(root, inst);
    return { ok: true, folder, fetched: d.get.length, dropped: d.drop.length };
}

const headers = { 'User-Agent': 'UMK3-Launcher', 'Cache-Control': 'no-cache' };

/* the index, with every mod's picture as a data: URL (the page loads
 * nothing from outside) */
async function fetchIndex(fetchFn) {
    const r = await fetchFn(INDEX + '?t=' + Date.now(), { headers });
    if (!r.ok) throw new Error('GitHub ' + r.status);
    const list = parseIndex(await r.text());
    await Promise.all(list.map(async m => {
        m.preview = null;
        if (!m.thumbnail) return;
        try {
            const p = await fetchFn(RAW + m.thumbnail.split('/').map(encodeURIComponent).join('/'), { headers });
            if (!p.ok) return;
            const buf = Buffer.from(await p.arrayBuffer());
            if (buf.length < 4 << 20)
                m.preview = 'data:image/' + (/\.png$/i.test(m.thumbnail) ? 'png' : 'jpeg') + ';base64,' + buf.toString('base64');
        } catch { /* no picture */ }
    }));
    return list;
}

function unzip(zip, dest) {
    const tar = LINUX ? 'tar' : path.join(process.env.SystemRoot || 'C:\\Windows', 'System32', 'tar.exe');
    let r = LINUX || fs.existsSync(tar) ? spawnSync(tar, ['-xf', zip, '-C', dest], { windowsHide: true }) : { status: 1 };
    if (r.status !== 0 && !LINUX)
        r = spawnSync('powershell.exe', ['-NoProfile', '-Command',
            'Expand-Archive -LiteralPath ' + "'" + zip.replace(/'/g, "''") + "'" + ' -DestinationPath ' + "'" + dest.replace(/'/g, "''") + "'" + ' -Force'],
            { windowsHide: true });
    if (r.status !== 0) throw new Error('could not unpack the mod');
}

/* the unpacked mod: the zip's own top when it has mod files there, else the
 * one folder it holds */
function modRoot(dest) {
    const list = fs.readdirSync(dest, { withFileTypes: true }).filter(e => !e.name.startsWith('.') && e.name !== '__MACOSX');
    if (list.length === 1 && list[0].isDirectory()) return path.join(dest, list[0].name);
    return dest;
}

/* mods.ini kept as it is, with `folder` added at the top, on, when it is
 * not there yet (the same file lib/mods.js writes) */
function register(root, folder) {
    const file = path.join(modsDir(root), 'mods.ini');
    let text = '';
    try { text = fs.readFileSync(file, 'utf8'); } catch { /* none yet */ }
    const lines = text.split(/\r?\n/).filter(l => l !== '');
    if (lines.some(l => { const m = /^(on|off)=(.+)$/.exec(l.trim()); return m && m[2].trim() === folder; }))
        return;
    if (!lines.length)
        lines.push('; The MOD MANAGER\'s list (UMK3 launcher). Highest priority first: when two',
            '; mods have a file of the same name, the higher one wins. on= is used, off= is not.');
    const at = lines.findIndex(l => !l.trim().startsWith(';'));
    lines.splice(at < 0 ? lines.length : at, 0, 'on=' + folder);
    fs.writeFileSync(file, lines.join('\n') + '\n');
}

/* Download, check, unpack and put the mod in mods/<folder>; an update
 * replaces the folder it was installed in. */
async function install(fetchFn, root, mod, onProgress) {
    if (!mod || !mod.sha256) throw new Error('the index has no SHA-256 for this mod');
    const inst = readInstalled(root);
    const prev = inst[mod.id];
    const folder = (prev && safeName(prev.folder)) || mod.folder;
    const dir = modsDir(root);
    const work = path.join(dir, '.hub');
    fs.rmSync(work, { recursive: true, force: true });
    fs.mkdirSync(work, { recursive: true });
    try {
        const zip = await upd.download(fetchFn, mod, work, onProgress);
        const out = path.join(work, 'out');
        fs.mkdirSync(out);
        unzip(zip, out);
        const src = modRoot(out);
        const dest = path.join(dir, folder);
        const old = path.join(work, 'old');
        if (fs.existsSync(dest)) fs.renameSync(dest, old);
        try {
            fs.renameSync(src, dest);
        } catch (e) {
            if (fs.existsSync(old) && !fs.existsSync(dest)) fs.renameSync(old, dest);
            throw e;
        }
        register(root, folder);
        inst[mod.id] = { folder, version: mod.version, sha256: mod.sha256 };
        writeInstalled(root, inst);
        return { ok: true, folder };
    } finally {
        fs.rmSync(work, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
    }
}

/* DOWNLOAD or UPDATE: the whole zip for a mod not here, only what differs
 * for one that is */
function get(fetchFn, root, mod, onProgress) {
    return folderOf(root, readInstalled(root), mod) ? sync(fetchFn, root, mod, onProgress) : install(fetchFn, root, mod, onProgress);
}

module.exports = { REPO, INDEX, parseIndex, status, check, fetchIndex, install, sync, get, register, readInstalled, zipFiles, crc32 };
