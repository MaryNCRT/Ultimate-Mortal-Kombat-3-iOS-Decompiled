'use strict';
/* The launcher's UI in an ordinary browser, for debugging it without
 * Electron: serves renderer/ and answers the preload's calls (window.umk3)
 * over HTTP from a game folder. Read-only but for stage edits and MOD HUB
 * installs, which go to the given folder too.
 *
 *   node scripts/ui-harness.js <game folder> [port]
 *
 * then open http://localhost:<port>/ (default 5178). Calls it does not know
 * answer null. */
const http = require('http');
const fs = require('fs');
const path = require('path');
const ini = require('../lib/ini');
const mods = require('../lib/mods');
const hub = require('../lib/hub');
const artwork = require('../lib/artwork');
const steamsync = require('../lib/steamsync');

const root = path.resolve(process.argv[2] || '.');
const port = Number(process.argv[3]) || 5178;
const dir = path.join(__dirname, '..', 'renderer');
const editFile = id => path.join(root, 'stage edits', id + '.json');

let g_hub = [];
/* STEAM: the real Steam folders, or UMK3_HARNESS_STEAM=<a Steam folder> (a
 * copy to try ADD / REMOVE on), UMK3_HARNESS_STEAM_RUNNING=1|0 to say
 * whether Steam is open; images from data/steam-artwork under /art/ */
const steamOpts = process.env.UMK3_HARNESS_STEAM
    ? { candidates: [{ path: process.env.UMK3_HARNESS_STEAM, kind: process.platform === 'win32' ? 'windows' : 'native' }] } : {};
if (process.env.UMK3_HARNESS_STEAM_RUNNING) steamOpts.running = process.env.UMK3_HARNESS_STEAM_RUNNING === '1';
const artLib = () => {
    const l = artwork.library(root);
    l.items.forEach(i => { i.src = '/art/' + i.file + '?' + i.sha1; });
    return l;
};
const synced = async r => { if (r && r.ok) r.sync = await steamsync.syncIfAdded(root, steamOpts); return r; };
const calls = {
    root: () => ({ root, ready: true, builtAt: 0, version: 'harness', both: false, any: true }),
    loadConfig: () => Object.assign(ini.load(path.join(root, 'umk3.ini')),
        process.env.UMK3_HARNESS_FIRST === '1' ? { preset: '', accountAsked: false } : {}),
    saveConfig: () => ini.load(path.join(root, 'umk3.ini')),
    /* the first start's windows, nothing written: the preset shown, WHAT'S NEW always offered */
    applyPreset: name => ini.applyPreset(ini.load(path.join(root, 'umk3.ini')), name, { w: 1920, h: 1080 }),
    whatsNew: () => {
        const entries = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'lib', 'whatsnew.json'), 'utf8')).entries;
        return { entries, current: entries[0].version, fresh: false, show: true };
    },
    whatsNewSeen: () => true,
    /* the account: UMK3_HARNESS_ACCOUNT=1 signed in with sample friends (a
     * picture of FRIENDS), else a guest with the server up; the first
     * start's windows again with UMK3_HARNESS_FIRST=1 */
    'account.status': () => process.env.UMK3_HARNESS_ACCOUNT === '1'
        ? { server: true, user: 'tester' } : { server: true, user: null },
    'account.me': () => ({ ok: true, email: '' }),
    'account.friends': () => ({ ok: true, code: '7KQ2-9XMD-4TPA', name: 'TESTER',
        friends: [{ code: 'AAAA-BBBB-CCCC', name: 'FRIEND ONE', look: '20|E6B800|C|F2D21B|90|0' },
                  { code: 'DDDD-EEEE-FFFF', name: 'FRIEND TWO', look: '15|3A8EE6|L' }],
        incoming: [{ code: 'GGGG-HHHH-JJJJ', name: 'A REQUEST', look: '3|7A1F45|C|1D0C17|135|1' }],
        outgoing: [], invites: [{ code: 'KKKK-MMMM-NNNN', name: 'A CHALLENGER', look: '8|0B6E2C|D' }] }),
    modsList: () => mods.list(root),
    modsSkinCreate: fields => mods.skinCreate(root, fields),
    modsInfo: (folder, fields) => mods.setInfo(root, folder, fields),
    resFiles: () => mods.resFiles(root),
    resRead: (rel, layer) => mods.resRead(root, rel, layer),
    gameTables: () => mods.gameTables(root),
    stagePics: () => [],
    stageEditsLoad: id => { try { return JSON.parse(fs.readFileSync(editFile(id), 'utf8')); } catch { return null; } },
    stageEditsSave: (id, json) => { fs.mkdirSync(path.dirname(editFile(id)), { recursive: true }); fs.writeFileSync(editFile(id), json); return true; },
    listPads: () => [], listMonitors: () => [], glyphs: () => ({}), replaysList: () => [],
    'steam.status': () => steamsync.status(root, steamOpts),
    'steam.account': ref => steamsync.chooseAccount(root, ref),
    'steam.add': ref => steamsync.add(root, ref, Object.assign({ closeSteam: true, shutdown: async () => true, start: () => true }, steamOpts)),
    'steam.remove': ref => steamsync.remove(root, ref, Object.assign({ closeSteam: true, shutdown: async () => true, start: () => true }, steamOpts)),
    'steam.apply': ref => steamsync.apply(root, ref, steamOpts),
    'steam.auto': () => steamsync.autoAll(root, steamOpts),
    'art.setKey': key => artwork.setApiKey(root, key),
    'art.browse': (kind, page, shape) => artwork.browse(root, kind, Number(page) || 0, shape),
    'art.download': item => artwork.download(root, item),
    'art.select': async item => synced(await artwork.select(root, item)),
    'art.unselect': slot => synced(artwork.unselect(root, slot)),
    'art.remove': (key, confirm) => synced(artwork.remove(root, key, !!confirm)),
    'art.library': () => artLib(),
    'art.open': () => true, 'art.site': () => true,
    hubList: async () => { g_hub = await hub.fetchIndex(fetch); return { ok: true, mods: await hub.check(fetch, root, g_hub) }; },
    hubInstall: async id => { const m = g_hub.find(x => x.id === id); return m ? Object.assign(await hub.get(fetch, root, m), { mods: await hub.check(fetch, root, g_hub) }) : { ok: false, error: 'not in the hub' }; },
};

/* the page's side: every call a POST, bytes back as base64 */
const shim = `<script>
(() => {
    const call = async (name, args) => {
        const r = await fetch('/api/' + name, { method: 'POST', body: JSON.stringify(args) });
        const j = await r.json();
        if (j && j.__bytes !== undefined) { const s = atob(j.__bytes); const b = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) b[i] = s.charCodeAt(i); return b; }
        return j;
    };
    const ns = pre => new Proxy({}, {
        get: (t, k) => (typeof k === 'string' && k.startsWith('on') ? () => {} : (...a) => call(pre + k, a)),
    });
    window.umk3 = new Proxy({ platform: 'win32', account: ns('account.'), steam: ns('steam.'), art: ns('art.') }, {
        get: (t, k) => (k in t ? t[k] : typeof k === 'string' && k.startsWith('on') ? () => {} : (...a) => call(k, a)),
    });
})();
</script>`;

const types = { '.js': 'text/javascript', '.css': 'text/css', '.html': 'text/html', '.png': 'image/png', '.jpg': 'image/jpeg', '.ico': 'image/x-icon', '.svg': 'image/svg+xml', '.json': 'application/json', '.woff2': 'font/woff2', '.ttf': 'font/ttf', '.mp3': 'audio/mpeg' };

http.createServer((req, res) => {
    const u = decodeURIComponent(req.url.split('?')[0]);
    if (u.startsWith('/api/')) {
        let body = '';
        req.on('data', c => { body += c; });
        req.on('end', async () => {
            let out = null;
            try {
                const f = calls[u.slice(5)];
                out = f ? await f(...JSON.parse(body || '[]')) : null;
            } catch (e) { console.error(u, e); }
            if (out instanceof Uint8Array) out = { __bytes: Buffer.from(out).toString('base64') };
            res.writeHead(200, { 'content-type': 'application/json' });
            res.end(JSON.stringify(out === undefined ? null : out));
        });
        return;
    }
    if (u.startsWith('/art/')) {
        const b = artwork.base(root), f = path.join(b, u.slice(5));
        if (!f.startsWith(b) || !fs.existsSync(f)) { res.writeHead(404); res.end(); return; }
        res.writeHead(200, { 'content-type': types[path.extname(f)] || 'application/octet-stream' });
        res.end(fs.readFileSync(f));
        return;
    }
    let p = path.join(dir, u === '/' ? 'index.html' : u);
    if (!p.startsWith(dir) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) p = path.join(dir, 'index.html');
    let data = fs.readFileSync(p);
    if (p.endsWith('index.html')) data = Buffer.from(data.toString().replace('<head>', '<head>' + shim));
    res.writeHead(200, { 'content-type': types[path.extname(p)] || 'application/octet-stream' });
    res.end(data);
}).listen(port, () => console.log('harness on http://localhost:' + port + '/ for ' + root));
