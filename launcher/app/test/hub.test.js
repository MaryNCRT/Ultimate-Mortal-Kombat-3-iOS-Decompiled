'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const zlib = require('zlib');
const hub = require('../lib/hub');

const DL = 'https://github.com/MaryNCRT/UMK3-IOS-Mod-Hub/releases/download/';
const sha = 'b'.repeat(64);

test('the index keeps well-formed mods from the hub repository only', () => {
    const list = hub.parseIndex(JSON.stringify({ format: 1, mods: [
        { id: 'rain', folder: 'Rain', name: 'Rain', author: 'Jay', version: '1.0', category: 'textures',
          description: 'a\nb', thumbnail: 'mods/rain/cover.png', url: DL + 'rain-v1.0/rain-1.0.zip', size: 10, sha256: sha },
        { id: 'rain', folder: 'Rain2', version: '2.0', url: DL + 'x/rain-2.0.zip' },            /* same id twice */
        { id: 'evil', folder: 'Evil', version: '1.0', url: 'https://example.com/evil.zip' },     /* not the hub */
        { id: 'up', folder: '../up', version: '1.0', url: DL + 'up/up.zip' },                    /* bad folder */
        { id: 'nover', folder: 'NoVer', url: DL + 'n/n.zip' },                                   /* no version */
        { id: 'Odd Id!', folder: 'Odd', version: '1', url: DL + 'o/o.zip' },                     /* bad id */
        { id: 'pic', folder: 'Pic', version: '1', url: DL + 'p/p.zip', thumbnail: '../secret.png', category: 'weird' },
    ] }));
    assert.deepEqual(list.map(m => m.id), ['rain', 'pic']);
    assert.equal(list[0].description, 'a b');
    assert.equal(list[0].file, 'rain-1.0.zip');
    assert.equal(list[0].sha256, sha);
    assert.equal(list[1].thumbnail, '');
    assert.equal(list[1].category, 'mixed');
    assert.equal(list[1].sha256, '');
    assert.throws(() => hub.parseIndex('{'));
    assert.throws(() => hub.parseIndex('{"mods": 3}'));
});

function tmpRoot() {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3hub-'));
    fs.mkdirSync(path.join(root, 'mods'));
    return root;
}

test('status: installed version and update by version compare', () => {
    const root = tmpRoot();
    fs.mkdirSync(path.join(root, 'mods', 'Rain'));
    fs.writeFileSync(path.join(root, 'mods', 'hub.json'), JSON.stringify({
        rain: { folder: 'Rain', version: '1.0' }, gone: { folder: 'Gone', version: '1.0' } }));
    const list = hub.parseIndex({ mods: [
        { id: 'rain', folder: 'Rain', version: '1.1', url: DL + 'r/r.zip' },
        { id: 'gone', folder: 'Gone', version: '1.0', url: DL + 'g/g.zip' },
        { id: 'new', folder: 'New', version: '1.0', url: DL + 'n/n.zip' },
    ] });
    const s = hub.status(root, list);
    assert.deepEqual(s.map(m => [m.installed, m.update]), [['1.0', true], [null, false], [null, false]]);
    fs.writeFileSync(path.join(root, 'mods', 'hub.json'), JSON.stringify({ rain: { folder: 'Rain', version: '1.1' } }));
    assert.deepEqual(hub.status(root, list).map(m => m.update), [false, false, false]);
});

test('register adds a new mod at the top, on, and leaves a known one where it is', () => {
    const root = tmpRoot();
    const ini = path.join(root, 'mods', 'mods.ini');
    hub.register(root, 'Rain');
    assert.match(fs.readFileSync(ini, 'utf8'), /^;.*\n;.*\non=Rain\n$/);
    fs.writeFileSync(ini, '; c\non=A\noff=Rain\n');
    hub.register(root, 'Rain');
    hub.register(root, 'B');
    assert.equal(fs.readFileSync(ini, 'utf8'), '; c\non=B\non=A\noff=Rain\n');
});

/* a zip as publish.js makes one (the mod's folder at its top): deflated
 * files, a folder entry, data descriptors' flag left off */
function makeZip(files) {
    const local = [], central = [];
    let off = 0;
    const entries = [['Rain/', null]].concat(Object.entries(files));
    for (const [name, text] of entries) {
        const data = text === null ? Buffer.alloc(0) : Buffer.from(text);
        const comp = text === null ? data : zlib.deflateRawSync(data);
        const nm = Buffer.from(name);
        const h = Buffer.alloc(30);
        h.writeUInt32LE(0x04034b50, 0); h.writeUInt16LE(20, 4); h.writeUInt16LE(text === null ? 0 : 8, 8);
        h.writeUInt32LE(hub.crc32(data), 14); h.writeUInt32LE(comp.length, 18); h.writeUInt32LE(data.length, 22);
        h.writeUInt16LE(nm.length, 26);
        const c = Buffer.alloc(46);
        c.writeUInt32LE(0x02014b50, 0); c.writeUInt16LE(20, 4); c.writeUInt16LE(20, 6); c.writeUInt16LE(text === null ? 0 : 8, 10);
        c.writeUInt32LE(hub.crc32(data), 16); c.writeUInt32LE(comp.length, 20); c.writeUInt32LE(data.length, 24);
        c.writeUInt16LE(nm.length, 28); c.writeUInt32LE(off, 42);
        local.push(h, nm, comp);
        central.push(c, nm);
        off += 30 + nm.length + comp.length;
    }
    const cd = Buffer.concat(central);
    const e = Buffer.alloc(22);
    e.writeUInt32LE(0x06054b50, 0); e.writeUInt16LE(entries.length, 8); e.writeUInt16LE(entries.length, 10);
    e.writeUInt32LE(cd.length, 12); e.writeUInt32LE(off, 16);
    return Buffer.concat(local.concat([cd, e]));
}

test('install downloads, checks, unpacks and updates in place', async () => {
    const root = tmpRoot();
    const make = (ver, file) => makeZip({ 'Rain/mod.ini': 'name=Rain\nversion=' + ver + '\n', ['Rain/' + file]: 'x'.repeat(300) });
    const fetchOf = bytes => async () => ({
        ok: true, headers: { get: () => String(bytes.length) },
        body: { getReader: () => { let done = false; return { read: async () => done ? { done: true } : (done = true, { done: false, value: bytes }) }; } },
    });
    const v1 = make('1.0', 'OLD.png');
    const mod = v => hub.parseIndex({ mods: [{ id: 'rain', folder: 'Rain', version: v.ver, url: DL + 'r/rain.zip',
        sha256: crypto.createHash('sha256').update(v.bytes).digest('hex') }] })[0];

    await assert.rejects(hub.install(fetchOf(v1), root, Object.assign(mod({ ver: '1.0', bytes: v1 }), { sha256: 'c'.repeat(64) })));
    assert.ok(!fs.existsSync(path.join(root, 'mods', 'Rain')));

    let r = await hub.install(fetchOf(v1), root, mod({ ver: '1.0', bytes: v1 }));
    assert.equal(r.folder, 'Rain');
    assert.ok(fs.existsSync(path.join(root, 'mods', 'Rain', 'OLD.png')));
    assert.equal(hub.readInstalled(root).rain.version, '1.0');

    fs.writeFileSync(path.join(root, 'mods', 'mods.ini'), 'on=X\noff=Rain\n');
    const v2 = make('1.1', 'NEW.png');
    r = await hub.install(fetchOf(v2), root, mod({ ver: '1.1', bytes: v2 }));
    assert.ok(fs.existsSync(path.join(root, 'mods', 'Rain', 'NEW.png')));
    assert.ok(!fs.existsSync(path.join(root, 'mods', 'Rain', 'OLD.png')));
    assert.ok(!fs.existsSync(path.join(root, 'mods', '.hub')));
    assert.equal(hub.readInstalled(root).rain.version, '1.1');
    assert.equal(fs.readFileSync(path.join(root, 'mods', 'mods.ini'), 'utf8'), 'on=X\noff=Rain\n');
});

test('a mod already here: found by folder, compared by range reads, only what differs fetched', async () => {
    const bytes = makeZip({ 'Rain/mod.ini': 'name=Rain\n', 'Rain/A.png': 'a'.repeat(5000), 'Rain/Sub/B.png': 'bbbb' });
    const reads = [];
    const fetchFn = async (_url, opt) => {
        const m = /bytes=(\d+)-(\d+)/.exec(opt.headers.Range);
        const a = +m[1], b = Math.min(+m[2], bytes.length - 1);
        reads.push(b - a + 1);
        const part = bytes.subarray(a, b + 1);
        return { status: 206, ok: true, arrayBuffer: async () => part.buffer.slice(part.byteOffset, part.byteOffset + part.length) };
    };
    const mod = hub.parseIndex({ mods: [{ id: 'rain', folder: 'Rain', version: '1.0', url: DL + 'r/rain.zip',
        size: bytes.length, sha256: crypto.createHash('sha256').update(bytes).digest('hex') }] })[0];

    assert.deepEqual((await hub.zipFiles(hub.remote(fetchFn, mod), mod.size)).map(f => f.path).sort(), ['A.png', 'Sub/B.png', 'mod.ini']);

    /* the player's copy: B.png changed at the same size, A.png missing, an extra file */
    const root = tmpRoot();
    const base = path.join(root, 'mods', 'Rain');
    fs.mkdirSync(path.join(base, 'Sub'), { recursive: true });
    fs.writeFileSync(path.join(base, 'mod.ini'), 'name=Rain\n');
    fs.writeFileSync(path.join(base, 'Sub', 'B.png'), 'bxbb');
    fs.writeFileSync(path.join(base, 'old.png'), 'o');
    fs.writeFileSync(path.join(base, 'mod.meta.xml'), '<mod/>');
    let [s] = await hub.check(fetchFn, root, [mod]);
    assert.equal(s.here, 'Rain');
    assert.equal(s.installed, '');            /* no version in its mod.ini */
    assert.equal(s.update, false);
    assert.equal(s.changes, 2);               /* A.png missing, old.png extra; B.png same size */

    reads.length = 0;
    const r = await hub.get(fetchFn, root, mod);
    assert.deepEqual([r.fetched, r.dropped], [2, 1]);
    assert.equal(reads.length, 1 + 2);        /* the zip's end with its directory, then one read per file */
    assert.equal(fs.readFileSync(path.join(base, 'A.png'), 'utf8'), 'a'.repeat(5000));
    assert.equal(fs.readFileSync(path.join(base, 'Sub', 'B.png'), 'utf8'), 'bbbb');
    assert.ok(!fs.existsSync(path.join(base, 'old.png')));
    assert.ok(fs.existsSync(path.join(base, 'mod.meta.xml')));
    [s] = await hub.check(fetchFn, root, [mod]);
    assert.deepEqual([s.installed, s.changes], ['1.0', 0]);
    assert.deepEqual(await hub.get(fetchFn, root, mod).then(x => [x.fetched, x.dropped]), [0, 0]);
});

test('crc32 matches the zip standard', () => {
    assert.equal(hub.crc32(Buffer.from('123456789')), 0xcbf43926);
});
