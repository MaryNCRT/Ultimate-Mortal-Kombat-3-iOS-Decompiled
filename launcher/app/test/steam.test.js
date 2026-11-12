/* lib/vdf.js, lib/steam.js, lib/artwork.js, lib/steamsync.js against a
 * fake Steam folder and a fake SteamGridDB (no network, no real Steam). */
'use strict';

const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const vdf = require('../lib/vdf');
const steam = require('../lib/steam');
const art = require('../lib/artwork');
const sync = require('../lib/steamsync');

const S = vdf.T.STR, I = vdf.T.INT;

/* a shortcuts.vdf as Steam writes it, with someone else's game in it
 * (the layout of a real one: appid, AppName, Exe ... tags) */
function otherShortcuts() {
    return [{ type: vdf.T.MAP, key: 'shortcuts', value: [
        { type: vdf.T.MAP, key: '0', value: [
            { type: I, key: 'appid', value: 0x939d84c8 },
            { type: S, key: 'AppName', value: 'Another Game' },
            { type: S, key: 'Exe', value: '"D:\\Other\\game.exe"' },
            { type: S, key: 'StartDir', value: '"D:\\Other"' },
            { type: S, key: 'icon', value: '' },
            { type: I, key: 'LastPlayTime', value: 1784973000 },
            { type: vdf.T.U64, key: 'odd64', value: Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]) },
            { type: vdf.T.MAP, key: 'tags', value: [{ type: S, key: '0', value: 'Favorite' }] },
        ] },
    ] }];
}

function fakeWorld() {
    const top = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-steam-'));
    const steamDir = path.join(top, 'Steam');
    const cfg = path.join(steamDir, 'userdata', '283305792', 'config');
    fs.mkdirSync(cfg, { recursive: true });
    fs.mkdirSync(path.join(steamDir, 'userdata', '0'), { recursive: true });
    fs.mkdirSync(path.join(steamDir, 'config'), { recursive: true });
    fs.writeFileSync(path.join(steamDir, 'config', 'loginusers.vdf'),
        '"users"\n{\n\t"76561198243571520"\n\t{\n\t\t"AccountName"\t\t"x"\n\t\t"PersonaName"\t\t"Tester"\n\t\t"MostRecent"\t\t"1"\n\t}\n}\n');
    fs.writeFileSync(path.join(cfg, 'shortcuts.vdf'), vdf.serializeBinary(otherShortcuts()));
    const root = path.join(top, 'UMK3');
    fs.mkdirSync(root);
    fs.writeFileSync(path.join(root, 'UMK3-Launcher.exe'), 'stub');
    const opts = { platform: 'win32', candidates: [{ path: steamDir, kind: 'windows' }], running: false, autoArt: false };
    return { top, steamDir, cfg, root, opts, vdfFile: path.join(cfg, 'shortcuts.vdf') };
}

const PNG = Buffer.concat([Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]), Buffer.alloc(32, 7)]);
const JPG = Buffer.concat([Buffer.from([0xFF, 0xD8, 0xFF, 0xE0]), Buffer.alloc(32, 9)]);

/* SteamGridDB: one page per kind, images from its CDN */
function fakeFetch(calls) {
    return async (url, init) => {
        calls.push({ url: String(url), auth: init && init.headers && init.headers.Authorization });
        const u = new URL(url);
        if (u.hostname === 'cdn2.steamgriddb.com') {
            const body = u.pathname.endsWith('.jpg') ? JPG : PNG;
            return { ok: true, status: 200, arrayBuffer: async () => body.buffer.slice(body.byteOffset, body.byteOffset + body.length) };
        }
        const kind = u.pathname.split('/')[3];
        const mk = (id, w, h, ext) => ({ id, url: `https://cdn2.steamgriddb.com/${kind}/${id}.${ext}`, thumb: `https://cdn2.steamgriddb.com/thumb/${id}.${ext}`,
            width: w, height: h, mime: ext === 'jpg' ? 'image/jpeg' : 'image/png', style: 'alternate', author: { name: 'someone' } });
        const data = kind === 'grids' ? [mk(11, 600, 900, 'png'), mk(12, 920, 430, 'jpg')]
            : kind === 'heroes' ? [mk(21, 1920, 620, 'png')] : kind === 'logos' ? [mk(31, 800, 300, 'png')] : [mk(41, 256, 256, 'png')];
        return { ok: true, status: 200, json: async () => ({ success: true, page: Number(u.searchParams.get('page')), total: 120, limit: 50, data }) };
    };
}

test('vdf: a binary file serializes back to the same bytes', () => {
    const bytes = vdf.serializeBinary(otherShortcuts());
    assert.ok(vdf.serializeBinary(vdf.parseBinary(bytes)).equals(bytes));
    assert.throws(() => vdf.parseBinary(bytes.subarray(0, bytes.length - 3)));
});

test('vdf: the real shortcuts.vdf on this machine, read only, round-trips', { skip: !fs.existsSync('C:/Program Files (x86)/Steam/userdata') }, () => {
    const ud = 'C:/Program Files (x86)/Steam/userdata';
    for (const id of fs.readdirSync(ud)) {
        const f = path.join(ud, id, 'config', 'shortcuts.vdf');
        if (!fs.existsSync(f)) continue;
        const b = fs.readFileSync(f);
        assert.ok(vdf.serializeBinary(vdf.parseBinary(b)).equals(b), f);
    }
});

test('vdf: text files', () => {
    const o = vdf.parseText('"users" { "1" { "PersonaName" "A \\"B\\"" } // c\n }');
    assert.strictEqual(o.users['1'].PersonaName, 'A "B"');
});

test('steam: the app id is the CRC of the quoted Exe and the name, top bit set', () => {
    /* a shortcut Steam showed with its artwork on the project machine */
    assert.strictEqual(steam.shortcutAppId('"D:\\DEF JAM FIGHT FOR NY\\DefJamRecompiled-1.1.1-windows-x64.exe"', 'Def Jam Recompiled'), 2476573896);
    assert.strictEqual(steam.crc32(Buffer.from('123456789')), 0xCBF43926);
});

test('steam: accounts, add, no duplicates, other shortcuts kept, remove', async () => {
    const w = fakeWorld();
    const st = await sync.status(w.root, w.opts);
    assert.strictEqual(st.accounts.length, 1);
    assert.strictEqual(st.accounts[0].persona, 'Tester');
    assert.strictEqual(st.accounts[0].shortcut, null);
    const ref = st.account;

    const otherBefore = vdf.serializeBinary([vdf.get(steam.readShortcuts(w.vdfFile), 'shortcuts').value[0]]);
    const r1 = await sync.add(w.root, ref, w.opts);
    assert.ok(r1.ok && r1.added, JSON.stringify(r1));
    const r2 = await sync.add(w.root, ref, w.opts);
    assert.ok(r2.ok && !r2.added);
    assert.strictEqual(r2.appid, r1.appid);

    const root = steam.readShortcuts(w.vdfFile);
    const list = vdf.get(root, 'shortcuts').value;
    assert.strictEqual(list.length, 2);
    assert.deepStrictEqual(list.map(n => n.key), ['0', '1']);
    assert.ok(vdf.serializeBinary([list[0]]).equals(otherBefore), 'the other game is byte-identical');
    const ours = list[1].value;
    assert.strictEqual(vdf.val(ours, 'AppName'), steam.NAME);
    assert.strictEqual(vdf.val(ours, 'Exe'), `"${path.join(w.root, 'UMK3-Launcher.exe')}"`);
    assert.strictEqual(vdf.val(ours, 'appid'), steam.shortcutAppId(vdf.val(ours, 'Exe'), steam.NAME));
    assert.ok(fs.readdirSync(w.cfg).some(f => /^shortcuts\.vdf\.umk3-.*\.bak$/.test(f)), 'a backup');

    /* a copy someone added by hand under our name is folded into one */
    list.push({ type: vdf.T.MAP, key: '2', value: [{ type: S, key: 'AppName', value: steam.NAME }, { type: S, key: 'Exe', value: '"C:\\old\\x.exe"' }] });
    fs.writeFileSync(w.vdfFile, vdf.serializeBinary(root));
    const r3 = await sync.add(w.root, ref, w.opts);
    assert.strictEqual(r3.removedDuplicates, 1);
    assert.strictEqual(vdf.get(steam.readShortcuts(w.vdfFile), 'shortcuts').value.length, 2);

    const rm = await sync.remove(w.root, ref, w.opts);
    assert.strictEqual(rm.removed, 1);
    const after = vdf.get(steam.readShortcuts(w.vdfFile), 'shortcuts').value;
    assert.strictEqual(after.length, 1);
    assert.ok(vdf.serializeBinary([after[0]]).equals(otherBefore));
});

test('steam: nothing is written while Steam runs', async () => {
    const w = fakeWorld();
    const before = fs.readFileSync(w.vdfFile);
    const st = await sync.status(w.root, w.opts);
    const r = await sync.add(w.root, st.account, Object.assign({}, w.opts, { running: true }));
    assert.strictEqual(r.error, 'running');
    assert.ok(fs.readFileSync(w.vdfFile).equals(before));
});

test('artwork: browse, download once, select, sync to Steam, delete with confirm', async () => {
    const w = fakeWorld();
    const calls = [];
    const f = fakeFetch(calls);
    assert.strictEqual((await art.browse(w.root, 'grid', 0, 'all', f)).error, 'nokey');
    art.setApiKey(w.root, 'test-key');

    const page = await art.browse(w.root, 'grid', 0, 'portrait', f);
    assert.ok(page.ok && page.more && page.items.length === 2);
    const q = new URL(calls.at(-1).url);
    assert.strictEqual(q.pathname, '/api/v2/grids/game/38752');
    assert.strictEqual(q.searchParams.get('mimes'), 'image/png,image/jpeg');
    assert.strictEqual(q.searchParams.get('dimensions'), '600x900,342x482,660x930');
    assert.strictEqual(calls.at(-1).auth, 'Bearer test-key');

    const st = await sync.status(w.root, w.opts);
    await sync.add(w.root, st.account, w.opts);
    const appid = (await sync.status(w.root, w.opts)).accounts[0].shortcut.appid;

    const items = { grid: page.items[0], wide: page.items[1] };
    for (const k of ['hero', 'logo', 'icon'])
        items[k] = (await art.browse(w.root, k, 0, 'all', f)).items[0];
    for (const it of Object.values(items)) {
        const r = await art.select(w.root, it, f);
        assert.ok(r.ok, JSON.stringify(r));
    }
    const n = calls.filter(c => c.url.includes('cdn2')).length;
    await art.download(w.root, items.hero, f);
    assert.strictEqual(calls.filter(c => c.url.includes('cdn2')).length, n, 'cached: not downloaded again');

    const b = art.base(w.root);
    assert.ok(fs.existsSync(path.join(b, 'grids', 'umk3-grid-11-600x900-alternate.png')));
    assert.ok(fs.existsSync(path.join(b, 'selected', 'wide.jpg')));
    const sel = JSON.parse(fs.readFileSync(path.join(b, 'selections.json'), 'utf8'));
    assert.strictEqual(sel.selected.grid, 'grid:11');
    assert.strictEqual(sel.selected.wide, 'grid:12');

    const ap = await sync.apply(w.root, st.account, w.opts);
    assert.ok(ap.ok && !ap.iconPending, JSON.stringify(ap));
    const grid = path.join(w.cfg, 'grid');
    for (const name of [`${appid}p.png`, `${appid}.jpg`, `${appid}_hero.png`, `${appid}_logo.png`, `${appid}_icon.png`])
        assert.ok(fs.existsSync(path.join(grid, name)), name);
    assert.ok(fs.readFileSync(path.join(grid, `${appid}.jpg`)).equals(JPG), 'the original, unchanged');
    assert.ok(fs.existsSync(path.join(b, 'grids', 'umk3-grid-12-920x430-alternate.jpg')), 'copied, not moved');
    const ours = vdf.get(steam.readShortcuts(w.vdfFile), 'shortcuts').value[1].value;
    assert.strictEqual(vdf.val(ours, 'icon'), path.join(grid, `${appid}_icon.png`));

    /* the chosen hero needs a confirm to go */
    const del = art.remove(w.root, 'hero:21', false);
    assert.ok(del.needConfirm);
    assert.ok(art.remove(w.root, 'hero:21', true).ok);
    assert.strictEqual(art.library(w.root).selected.hero, null);
    const ap2 = await sync.apply(w.root, st.account, w.opts);
    assert.ok(!fs.existsSync(path.join(grid, `${appid}_hero.png`)), 'the unchosen slot leaves Steam');
    assert.ok(ap2.ok);

    /* with Steam running the files are copied, the icon field waits */
    art.unselect(w.root, 'icon');
    const ap3 = await sync.apply(w.root, st.account, Object.assign({}, w.opts, { running: true }));
    assert.ok(ap3.iconPending);
});

test('artwork: an error page is never saved as an image', async () => {
    const w = fakeWorld();
    art.setApiKey(w.root, 'k');
    const html = Buffer.from('<html>nope</html>');
    const r = await art.download(w.root, { kind: 'grid', id: 5, url: 'https://cdn2.steamgriddb.com/x.png', width: 1, height: 1 },
        async () => ({ ok: true, status: 200, arrayBuffer: async () => html }));
    assert.strictEqual(r.error, 'format');
    assert.strictEqual((await art.download(w.root, { kind: 'grid', id: 6, url: 'https://evil.example/x.png' }, null)).error, 'url');
});

test('steam: Linux native, Flatpak and the same folder through ~/.steam/steam counted once', async () => {
    const home = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-home-'));
    const native = path.join(home, '.local', 'share', 'Steam');
    const flat = path.join(home, '.var', 'app', 'com.valvesoftware.Steam', '.local', 'share', 'Steam');
    for (const d of [native, flat])
        fs.mkdirSync(path.join(d, 'userdata', '42', 'config'), { recursive: true });
    fs.mkdirSync(path.join(home, '.steam'));
    let linked = true;
    try { fs.symlinkSync(native, path.join(home, '.steam', 'steam'), 'junction'); } catch { linked = false; }
    const found = await steam.findInstalls({ platform: 'linux', home });
    assert.deepStrictEqual(found.map(f => f.kind).sort(), ['flatpak', 'native']);
    if (linked) assert.strictEqual(found.length, 2);
    const t = steam.launcherTarget(home, { platform: 'linux', env: {} });
    assert.strictEqual(t, null);
    fs.writeFileSync(path.join(home, 'UMK3-Launcher'), '');
    assert.strictEqual(steam.launcherTarget(home, { platform: 'linux', env: {} }).exeField, `"${path.join(home, 'UMK3-Launcher')}"`);
});

test('artwork: with the project key, ADD TO STEAM arrives with the best of each kind chosen by itself', async () => {
    const w = fakeWorld();
    fs.mkdirSync(path.join(w.root, 'TOOLS'));
    fs.writeFileSync(path.join(w.root, 'TOOLS', 'steamgriddb.key'), 'project-key\n');
    assert.strictEqual(art.keySource(w.root), 'project');
    const calls = [];
    const st = await sync.status(w.root, w.opts);
    assert.ok(st.hasKey);
    const r = await sync.add(w.root, st.account, Object.assign({}, w.opts, { fetch: fakeFetch(calls), autoArt: true }));
    assert.ok(r.ok && r.auto && r.auto.ok, JSON.stringify(r));
    assert.deepStrictEqual(r.auto.picked.map(p => p.slot), ['grid', 'wide', 'hero', 'logo', 'icon']);
    assert.ok(calls.every(c => !c.auth || c.auth === 'Bearer project-key'));
    const grid = path.join(w.cfg, 'grid');
    for (const sfx of ['p.png', '.jpg', '_hero.png', '_logo.png', '_icon.png'])
        assert.ok(fs.existsSync(path.join(grid, r.appid + sfx)), sfx);
    /* the player's own key wins over the project's */
    art.setApiKey(w.root, 'mine');
    assert.strictEqual(art.keySource(w.root), 'own');
    assert.strictEqual(art.apiKey(w.root), 'mine');
});

test('artwork: with no key at all, ADD TO STEAM puts the game page’s own set in, from the CDN', async () => {
    const w = fakeWorld();
    delete process.env.UMK3_SGDB_KEY;
    assert.strictEqual(art.keySource(w.root), null);
    const calls = [];
    const st = await sync.status(w.root, w.opts);
    const r = await sync.add(w.root, st.account, Object.assign({}, w.opts, { fetch: fakeFetch(calls), autoArt: true }));
    assert.ok(r.ok && r.auto && r.auto.ok, JSON.stringify(r));
    assert.deepStrictEqual(r.auto.picked.map(p => p.key), art.PAGE_SET.map(i => `${i.kind}:${i.id}`));
    assert.ok(calls.every(c => new URL(c.url).hostname === 'cdn2.steamgriddb.com' && !c.auth), 'no API, no key');
    const grid = path.join(w.cfg, 'grid');
    for (const sfx of ['p.png', '.png', '_hero.png', '_logo.png', '_icon.png'])
        assert.ok(fs.existsSync(path.join(grid, r.appid + sfx)), sfx);
});

test('steam: with Steam open, ADD closes it, adds, and opens it again', async () => {
    const w = fakeWorld();
    let running = true, closed = 0, started = 0;
    const st = await sync.status(w.root, w.opts);
    const o = Object.assign({}, w.opts, { closeSteam: true });
    delete o.running;
    /* isSteamRunning reads opts.running: a getter follows the fake Steam */
    Object.defineProperty(o, 'running', { get: () => running, enumerable: true });
    o.shutdown = async () => { closed++; running = false; return true; };
    o.start = () => { started++; running = true; return true; };
    const r = await sync.add(w.root, st.account, o);
    assert.ok(r.ok && r.added, JSON.stringify(r));
    assert.strictEqual(closed, 1);
    assert.strictEqual(started, 1);
    /* Steam that will not close: nothing written */
    const w2 = fakeWorld();
    const before = fs.readFileSync(w2.vdfFile);
    const st2 = await sync.status(w2.root, w2.opts);
    const r2 = await sync.add(w2.root, st2.account, Object.assign({}, w2.opts, { running: true, closeSteam: true, shutdown: async () => false }));
    assert.strictEqual(r2.error, 'noclose');
    assert.ok(fs.readFileSync(w2.vdfFile).equals(before));
});
