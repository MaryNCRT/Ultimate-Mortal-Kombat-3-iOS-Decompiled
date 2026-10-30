'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { spawnSync } = require('child_process');
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

const tar = process.platform === 'win32' ? path.join(process.env.SystemRoot || 'C:\\Windows', 'System32', 'tar.exe') : 'tar';

test('install downloads, checks, unpacks and updates in place', { skip: !fs.existsSync(tar) && process.platform === 'win32' }, async () => {
    const root = tmpRoot();
    const make = (ver, file) => {
        const src = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3hubsrc-'));
        fs.mkdirSync(path.join(src, 'Rain'));
        fs.writeFileSync(path.join(src, 'Rain', 'mod.ini'), 'name=Rain\nversion=' + ver + '\n');
        fs.writeFileSync(path.join(src, 'Rain', file), 'x');
        const zip = path.join(src, 'r.zip');
        const r = spawnSync(tar, ['-a', '-c', '-f', zip, '-C', src, 'Rain']);
        assert.equal(r.status, 0, String(r.stderr));
        return fs.readFileSync(zip);
    };
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
