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
 * version, sha256): the installed version and UPDATE come from there. An
 * install or update writes the mod's folder whole (an update replaces it)
 * and keeps its place and on/off in mods.ini; a new one goes to the top, on. */
'use strict';

const fs = require('fs');
const path = require('path');
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

/* each listed mod with what this folder has of it: installed (its version,
 * or null), and update when the hub's is newer. A HUB mod whose folder was
 * deleted counts as not installed. */
function status(root, list) {
    const inst = readInstalled(root);
    return list.map(m => {
        const i = inst[m.id];
        const here = i && safeName(i.folder) && fs.existsSync(path.join(modsDir(root), i.folder)) ? i : null;
        return Object.assign({}, m, {
            installed: here ? String(here.version || '') : null,
            update: !!here && upd.compareVersions(m.version, here.version) > 0,
        });
    });
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

module.exports = { REPO, INDEX, parseIndex, status, fetchIndex, install, register, readInstalled };
