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

const root = path.resolve(process.argv[2] || '.');
const port = Number(process.argv[3]) || 5178;
const dir = path.join(__dirname, '..', 'renderer');
const editFile = id => path.join(root, 'stage edits', id + '.json');

let g_hub = [];
const calls = {
    root: () => ({ root, ready: true, builtAt: 0, version: 'harness', both: false, any: true }),
    loadConfig: () => ini.load(path.join(root, 'umk3.ini')),
    saveConfig: () => ini.load(path.join(root, 'umk3.ini')),
    modsList: () => mods.list(root),
    resFiles: () => mods.resFiles(root),
    resRead: (rel, layer) => mods.resRead(root, rel, layer),
    gameTables: () => mods.gameTables(root),
    stagePics: () => [],
    stageEditsLoad: id => { try { return JSON.parse(fs.readFileSync(editFile(id), 'utf8')); } catch { return null; } },
    stageEditsSave: (id, json) => { fs.mkdirSync(path.dirname(editFile(id)), { recursive: true }); fs.writeFileSync(editFile(id), json); return true; },
    listPads: () => [], listMonitors: () => [], glyphs: () => ({}), replaysList: () => [],
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
    window.umk3 = new Proxy({ platform: 'win32' }, {
        get: (t, k) => (k in t ? t[k] : typeof k === 'string' && k.startsWith('on') ? () => {} : (...a) => call(k, a)),
    });
})();
</script>`;

const types = { '.js': 'text/javascript', '.css': 'text/css', '.html': 'text/html', '.png': 'image/png', '.svg': 'image/svg+xml', '.json': 'application/json', '.woff2': 'font/woff2', '.ttf': 'font/ttf', '.mp3': 'audio/mpeg' };

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
    let p = path.join(dir, u === '/' ? 'index.html' : u);
    if (!p.startsWith(dir) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) p = path.join(dir, 'index.html');
    let data = fs.readFileSync(p);
    if (p.endsWith('index.html')) data = Buffer.from(data.toString().replace('<head>', '<head>' + shim));
    res.writeHead(200, { 'content-type': types[path.extname(p)] || 'application/octet-stream' });
    res.end(data);
}).listen(port, () => console.log('harness on http://localhost:' + port + '/ for ' + root));
