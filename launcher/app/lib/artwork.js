/* The Steam artwork library: SteamGridDB's images for this game, downloaded
 * into the game folder, one of each kind chosen, and the choice copied into
 * Steam (lib/steam.js).
 *
 * SteamGridDB API v2 (https://www.steamgriddb.com/api/v2, the endpoints its
 * own client `steamgriddb` on npm calls):
 *     GET /grids/game/<id>   /heroes/game/<id>   /logos/game/<id>   /icons/game/<id>
 *     ?mimes=a,b&types=static&nsfw=false&humor=any&dimensions=600x900,...&page=N
 *     Authorization: Bearer <the player's own API key>
 *     -> { success, data: [{ id, url, thumb, width, height, mime, style,
 *          author: { name }, nsfw, humor, score, ... }], page, total, limit }
 * Only this game's id (38752 -- SteamGridDB's own id, not a Steam app id) is
 * ever asked for.  Only PNG and JPEG are asked for (and ICO for icons): what
 * Steam reads, so an original is copied, never converted.
 *
 * In the game folder:
 *     data/steam-artwork/grids|heroes|logos|icons/   the originals, as downloaded
 *     data/steam-artwork/selected/<slot>.<ext>       a copy of each chosen one
 *     data/steam-artwork/selections.json             what was downloaded, what
 *                                                    is chosen, what Steam got
 *     data/steam-artwork/settings.json               the API key (sealed by the
 *                                                    system where it can) and
 *                                                    the Steam account chosen
 */
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const GAME_ID = 38752;
const API = 'https://www.steamgriddb.com/api/v2';
const KINDS = {
    grid: { api: 'grids', dir: 'grids', mimes: ['image/png', 'image/jpeg'] },
    hero: { api: 'heroes', dir: 'heroes', mimes: ['image/png', 'image/jpeg'] },
    logo: { api: 'logos', dir: 'logos', mimes: ['image/png'] },
    icon: { api: 'icons', dir: 'icons', mimes: ['image/png', 'image/vnd.microsoft.icon'] },
};
/* grid shapes Steam uses: the portrait capsule and the wide one */
const DIMS = {
    portrait: ['600x900', '342x482', '660x930'],
    wide: ['460x215', '920x430'],
};
const SLOT_KIND = { grid: 'grid', wide: 'grid', hero: 'hero', logo: 'logo', icon: 'icon' };

function base(root) { return path.join(root, 'data', 'steam-artwork'); }

function ensureDirs(root) {
    const b = base(root);
    for (const d of ['grids', 'heroes', 'logos', 'icons', 'selected'])
        fs.mkdirSync(path.join(b, d), { recursive: true });
    return b;
}

function writeJsonAtomic(file, obj) {
    const tmp = file + '.tmp';
    fs.writeFileSync(tmp, JSON.stringify(obj, null, 2) + '\n');
    fs.renameSync(tmp, file);
}

function readJson(file, d) {
    try { return JSON.parse(fs.readFileSync(file, 'utf8')); } catch { return d; }
}

/* ------------------------------------------------------- selections ---- */
function emptySelections() {
    return {
        game: { name: 'Ultimate Mortal Kombat 3 IOS Decomp', steamgriddb: GAME_ID,
                source: `https://www.steamgriddb.com/game/${GAME_ID}` },
        downloaded: {},
        selected: { grid: null, wide: null, hero: null, logo: null, icon: null },
        steam: {},
    };
}

function loadSel(root) {
    const s = readJson(path.join(base(root), 'selections.json'), null) || emptySelections();
    const e = emptySelections();
    s.downloaded = s.downloaded || {};
    s.selected = Object.assign(e.selected, s.selected || {});
    s.steam = s.steam || {};
    s.game = e.game;
    return s;
}

function saveSel(root, s) {
    ensureDirs(root);
    writeJsonAtomic(path.join(base(root), 'selections.json'), s);
}

/* ---------------------------------------------------------- settings ---- */
let g_vault = null;
function setVault(v) { g_vault = v; }

function seal(t) {
    if (!t) return '';
    try {
        if (g_vault && g_vault.isEncryptionAvailable())
            return 'enc:' + g_vault.encryptString(t).toString('base64');
    } catch { }
    return 'plain:' + t;
}
function unseal(t) {
    if (!t) return '';
    if (t.startsWith('plain:')) return t.slice(6);
    if (t.startsWith('enc:')) {
        try { return g_vault.decryptString(Buffer.from(t.slice(4), 'base64')); } catch { return ''; }
    }
    return '';
}

function settingsFile(root) { return path.join(base(root), 'settings.json'); }
function loadSettings(root) { return readJson(settingsFile(root), {}); }
function saveSettings(root, delta) {
    ensureDirs(root);
    const s = Object.assign(loadSettings(root), delta);
    writeJsonAtomic(settingsFile(root), s);
    return s;
}
/* The key: the player's own (typed in the page), else the project's, which a
 * release carries as TOOLS/steamgriddb.key (make_release.py copies it from
 * UMK3_SGDB_KEY_FILE or the project's TOOLS/; never committed), else
 * UMK3_SGDB_KEY.  With the project's key nobody types anything. */
function projectKey(root) {
    for (const p of [path.join(root, 'TOOLS', 'steamgriddb.key'), path.join(root, 'tools', 'steamgriddb.key')]) {
        try {
            const k = fs.readFileSync(p, 'utf8').trim();
            if (k) return k;
        } catch { }
    }
    return String(process.env.UMK3_SGDB_KEY || '').trim();
}
function ownKey(root) { return unseal(loadSettings(root).key || ''); }
function apiKey(root) { return ownKey(root) || projectKey(root); }
function keySource(root) { return ownKey(root) ? 'own' : projectKey(root) ? 'project' : null; }
function setApiKey(root, key) {
    key = String(key || '').trim();
    saveSettings(root, { key: seal(key) });
    return !!key;
}

/* ------------------------------------------------------- the API ---- */
async function apiGet(root, url, fetchImpl) {
    const key = apiKey(root);
    if (!key) return { ok: false, error: 'nokey' };
    let r;
    try {
        r = await (fetchImpl || fetch)(url, { headers: { Authorization: `Bearer ${key}` } });
    } catch (e) {
        return { ok: false, error: 'net', detail: String(e.message || e) };
    }
    let j = null;
    try { j = await r.json(); } catch { }
    if (r.status === 401 || r.status === 403) return { ok: false, error: 'badkey' };
    if (!r.ok || !j || !j.success)
        return { ok: false, error: 'http', detail: `HTTP ${r.status} ${j && j.errors ? j.errors.join(', ') : ''}`.trim() };
    return { ok: true, json: j };
}

function itemKey(kind, id) { return `${kind}:${id}`; }

/* one page of a kind: what SteamGridDB has, each marked if it is here already */
async function browse(root, kind, page = 0, shape = 'all', fetchImpl) {
    const k = KINDS[kind];
    if (!k) return { ok: false, error: 'kind' };
    const q = new URLSearchParams({ mimes: k.mimes.join(','), types: 'static', nsfw: 'false', humor: 'any', epilepsy: 'false', page: String(page | 0) });
    if (kind === 'grid' && DIMS[shape]) q.set('dimensions', DIMS[shape].join(','));
    const res = await apiGet(root, `${API}/${k.api}/game/${GAME_ID}?${q}`, fetchImpl);
    if (!res.ok) return res;
    const sel = loadSel(root);
    const j = res.json;
    const items = (j.data || []).map(d => {
        const key = itemKey(kind, d.id), have = sel.downloaded[key];
        return {
            key, kind, id: d.id, url: d.url, thumb: d.thumb || d.url,
            width: d.width, height: d.height, mime: d.mime, style: d.style || '',
            author: (d.author && d.author.name) || '', score: d.score || 0,
            nsfw: !!d.nsfw, humor: !!d.humor,
            downloaded: !!(have && fs.existsSync(path.join(base(root), have.file))),
        };
    });
    const limit = j.limit || 50, total = typeof j.total === 'number' ? j.total : null;
    const more = total !== null ? (page + 1) * limit < total : items.length >= limit;
    return { ok: true, items, page, total, limit, more };
}

/* the file's own bytes say what it is; anything else (an error page) is refused */
function sniff(buf) {
    if (buf.length > 8 && buf[0] === 0x89 && buf[1] === 0x50 && buf[2] === 0x4E && buf[3] === 0x47) return '.png';
    if (buf.length > 3 && buf[0] === 0xFF && buf[1] === 0xD8 && buf[2] === 0xFF) return '.jpg';
    if (buf.length > 4 && buf[0] === 0 && buf[1] === 0 && buf[2] === 1 && buf[3] === 0) return '.ico';
    return null;
}

function allowedUrl(u) {
    try {
        const x = new URL(u);
        return x.protocol === 'https:' && (x.hostname === 'steamgriddb.com' || x.hostname.endsWith('.steamgriddb.com'));
    } catch { return false; }
}

const clean = s => String(s || '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 24);

/* the original, into its kind's folder; a second call finds it there */
async function download(root, item, fetchImpl) {
    const k = KINDS[item.kind];
    if (!k) return { ok: false, error: 'kind' };
    const sel = loadSel(root);
    const key = itemKey(item.kind, item.id);
    const have = sel.downloaded[key];
    if (have && fs.existsSync(path.join(base(root), have.file)))
        return { ok: true, cached: true, entry: have };
    if (!allowedUrl(item.url)) return { ok: false, error: 'url' };
    let buf;
    try {
        const r = await (fetchImpl || fetch)(item.url);
        if (!r.ok) return { ok: false, error: 'http', detail: `HTTP ${r.status}` };
        buf = Buffer.from(await r.arrayBuffer());
    } catch (e) {
        return { ok: false, error: 'net', detail: String(e.message || e) };
    }
    const ext = sniff(buf);
    if (!ext) return { ok: false, error: 'format' };
    ensureDirs(root);
    const style = clean(item.style);
    const name = `umk3-${item.kind}-${item.id}-${item.width}x${item.height}${style ? '-' + style : ''}${ext}`;
    const rel = path.posix.join(k.dir, name);
    const abs = path.join(base(root), rel);
    fs.writeFileSync(abs + '.part', buf);
    fs.renameSync(abs + '.part', abs);
    const entry = {
        key, kind: item.kind, id: item.id, file: rel, url: item.url, thumb: item.thumb || '',
        width: item.width, height: item.height, mime: item.mime || '', style: item.style || '',
        author: item.author || '', size: buf.length,
        sha1: crypto.createHash('sha1').update(buf).digest('hex'),
        downloadedAt: new Date().toISOString(),
    };
    const fresh = loadSel(root);
    fresh.downloaded[key] = entry;
    saveSel(root, fresh);
    return { ok: true, cached: false, entry };
}

/* a grid is the portrait capsule or the wide one by its shape */
function slotFor(entry) {
    if (entry.kind !== 'grid') return entry.kind;
    return entry.height > entry.width ? 'grid' : 'wide';
}

function selectedFile(root, slot) {
    const dir = path.join(base(root), 'selected');
    for (const e of ['.png', '.jpg', '.ico']) {
        const p = path.join(dir, slot + e);
        if (fs.existsSync(p)) return p;
    }
    return null;
}

function clearSelectedCopy(root, slot) {
    for (const e of ['.png', '.jpg', '.ico']) {
        const p = path.join(base(root), 'selected', slot + e);
        if (fs.existsSync(p)) fs.unlinkSync(p);
    }
}

/* chooses a downloaded image (downloading it first when it is not here) */
async function select(root, item, fetchImpl) {
    const d = await download(root, item, fetchImpl);
    if (!d.ok) return d;
    const entry = d.entry, slot = slotFor(entry);
    const src = path.join(base(root), entry.file);
    clearSelectedCopy(root, slot);
    const dst = path.join(base(root), 'selected', slot + path.extname(src).toLowerCase());
    fs.copyFileSync(src, dst + '.tmp');
    fs.renameSync(dst + '.tmp', dst);
    const sel = loadSel(root);
    sel.selected[slot] = entry.key;
    saveSel(root, sel);
    return { ok: true, slot, entry, downloaded: !d.cached };
}

function unselect(root, slot) {
    if (!(slot in SLOT_KIND)) return { ok: false };
    clearSelectedCopy(root, slot);
    const sel = loadSel(root);
    sel.selected[slot] = null;
    saveSel(root, sel);
    return { ok: true };
}

/* removes a downloaded original; one that is chosen only with confirm */
function remove(root, key, confirm) {
    const sel = loadSel(root);
    const entry = sel.downloaded[key];
    if (!entry) return { ok: false, error: 'missing' };
    const slots = Object.keys(sel.selected).filter(s => sel.selected[s] === key);
    if (slots.length && !confirm)
        return { ok: false, needConfirm: true, slots };
    const abs = path.join(base(root), entry.file);
    /* only files inside data/steam-artwork */
    if (path.relative(base(root), abs).startsWith('..')) return { ok: false, error: 'path' };
    if (fs.existsSync(abs)) fs.unlinkSync(abs);
    for (const s of slots) { sel.selected[s] = null; clearSelectedCopy(root, s); }
    delete sel.downloaded[key];
    saveSel(root, sel);
    return { ok: true, unselected: slots };
}

/* what is here, for the offline view */
function library(root) {
    const sel = loadSel(root);
    const items = [];
    for (const e of Object.values(sel.downloaded)) {
        const abs = path.join(base(root), e.file);
        if (!fs.existsSync(abs)) continue;
        items.push(Object.assign({}, e, { path: abs, slot: slotFor(e) }));
    }
    const selected = {};
    for (const s of Object.keys(sel.selected))
        selected[s] = sel.selected[s] && selectedFile(root, s) ? sel.selected[s] : null;
    return { items, selected, steam: sel.steam, folder: base(root) };
}

/* the chosen files, slot -> path (null when none) */
function selectedFiles(root) {
    const sel = loadSel(root), out = {};
    for (const s of Object.keys(SLOT_KIND))
        out[s] = sel.selected[s] ? selectedFile(root, s) : null;
    return out;
}

/* The best of each kind chosen by itself (SteamGridDB's score, highest
 * first) for every slot still empty, or every slot with all=true: what
 * ADD TO STEAM does, so the game arrives in Steam with its artwork. */
/* The game's own page, https://www.steamgriddb.com/game/38752, the first of
 * each kind in its Top order (read 2026-11-12; the wide one the first at
 * 920x430, the logo the first that is not 6915 px wide): downloaded straight
 * from SteamGridDB's CDN, so they need no API key -- what ADD TO STEAM puts in
 * Steam when there is no key. */
const PAGE_SET = [
    { slot: 'grid', kind: 'grid', id: 161244, width: 600, height: 900, mime: 'image/png', url: 'https://cdn2.steamgriddb.com/grid/1b0087ddc6378c7dd7f3c156301f181f.png' },
    { slot: 'wide', kind: 'grid', id: 528222, width: 920, height: 430, mime: 'image/png', url: 'https://cdn2.steamgriddb.com/grid/fd8769c391f5a6ec652151f0941ea8d1.png' },
    { slot: 'hero', kind: 'hero', id: 253, width: 1920, height: 620, mime: 'image/png', url: 'https://cdn2.steamgriddb.com/hero/c24cd76e1ce41366a4bbe8a49b02a028.png' },
    { slot: 'logo', kind: 'logo', id: 33461, width: 933, height: 392, mime: 'image/png', url: 'https://cdn2.steamgriddb.com/logo/c07be9421fc64b8e8ca2ebc12fcbd59d.png' },
    { slot: 'icon', kind: 'icon', id: 76847, width: 512, height: 512, mime: 'image/png', url: 'https://cdn2.steamgriddb.com/icon/ba4eb2077a7c3feeafda768fd01d599a.png' },
].map(i => Object.assign(i, { thumb: i.url, style: 'page', author: '' }));

const AUTO = [['grid', 'grid', 'portrait'], ['wide', 'grid', 'wide'], ['hero', 'hero', 'all'], ['logo', 'logo', 'all'], ['icon', 'icon', 'all']];
async function autoPick(root, all = false, fetchImpl) {
    const sel = loadSel(root);
    const out = { ok: true, picked: [], failed: [] };
    if (!apiKey(root)) {
        /* no key: the page's own set */
        for (const item of PAGE_SET) {
            if (!all && sel.selected[item.slot] && selectedFile(root, item.slot)) continue;
            const s = await select(root, item, fetchImpl);
            if (s.ok) out.picked.push({ slot: item.slot, key: s.entry.key });
            else out.failed.push({ slot: item.slot, error: s.error });
        }
        if (out.failed.some(f => f.error === 'net')) out.ok = false;
        return out;
    }
    for (const [slot, kind, shape] of AUTO) {
        if (!all && sel.selected[slot] && selectedFile(root, slot)) continue;
        const r = await browse(root, kind, 0, shape, fetchImpl);
        if (!r.ok) { out.failed.push({ slot, error: r.error }); if (r.error === 'badkey' || r.error === 'net') break; continue; }
        const fit = r.items.filter(i => kind !== 'grid' || slotFor(i) === slot)
            .sort((a, b) => (b.score || 0) - (a.score || 0));
        if (!fit.length) { out.failed.push({ slot, error: 'none' }); continue; }
        const s = await select(root, fit[0], fetchImpl);
        if (s.ok) out.picked.push({ slot, key: s.entry.key });
        else out.failed.push({ slot, error: s.error });
    }
    if (out.failed.some(f => f.error === 'badkey' || f.error === 'net')) out.ok = false;
    return out;
}

function noteSteam(root, accountId, info) {
    const sel = loadSel(root);
    sel.steam[accountId] = Object.assign({ appliedAt: new Date().toISOString() }, info);
    saveSel(root, sel);
}

module.exports = {
    GAME_ID, KINDS, DIMS, SLOT_KIND, base, setVault, apiKey, keySource, setApiKey, autoPick, PAGE_SET, loadSettings, saveSettings,
    browse, download, select, unselect, remove, library, selectedFiles, noteSteam, sniff, slotFor, allowedUrl,
};
