'use strict';
const test = require('node:test');
const assert = require('node:assert');
const upd = require('../lib/update');

test('versions compare by number, then letter', () => {
    const c = upd.compareVersions;
    assert.ok(c('v0.0.10-alpha', '0.0.9-alpha') > 0);
    assert.ok(c('v0.0.7b-alpha', '0.0.7-alpha') > 0);
    assert.ok(c('v0.0.7c-alpha', '0.0.7b-alpha') > 0);
    assert.ok(c('v0.0.9-alpha', '0.0.9-alpha') === 0);
    assert.ok(c('v0.0.8-alpha', '0.0.9-alpha') < 0);
    assert.ok(c('v0.1.0', '0.0.99') > 0);
    assert.ok(c('v0.1.10b-beta', '0.1.10-beta.b') === 0);
    assert.ok(c('v0.1.11-beta', '0.1.10-beta.b') > 0);
    assert.ok(c('v0.1.10b-beta', '0.1.10-beta') > 0);
});

test('check picks the newest non-draft release with a zip', async () => {
    const list = [
        { draft: true, tag_name: 'v9.9.9', assets: [{ name: 'x.zip' }] },
        { tag_name: 'v0.0.10-alpha', name: 'n', body: 'b', assets: [{ name: 'notes.txt' }] },
        { tag_name: 'v0.0.9-alpha', name: 'UMK3 0.0.9', body: '## hi', html_url: 'u',
          /* Windows takes the .zip, Linux the linux .tar.gz (lib/update.js) */
          assets: [{ name: 'UMK3-PC-0.0.9-alpha.zip', size: 5, browser_download_url: 'd', digest: 'sha256:' + 'a'.repeat(64) },
                   { name: 'UMK3-PC-0.0.9-alpha-linux-x64.tar.gz', size: 5, browser_download_url: 'd', digest: 'sha256:' + 'a'.repeat(64) }] },
    ];
    const f = async () => ({ ok: true, json: async () => list });
    const r = await upd.check(f, '0.0.8-alpha');
    assert.equal(r.tag, 'v0.0.9-alpha');
    assert.equal(r.newer, true);
    assert.equal(r.sha256, 'a'.repeat(64));
    assert.equal((await upd.check(f, '0.0.9-alpha')).newer, false);
});

/* Mary: "ENOTEMPTY: directory not empty, rmdir ...\update\new\...\resources"
 * stopped an update. A leftover update/new Windows will not let go of no
 * longer does: the update unpacks beside it. */
test('unpack gets past an update/new that cannot be removed', { skip: process.platform !== 'win32' }, () => {
    const fs = require('fs');
    const os = require('os');
    const path = require('path');
    const { spawnSync } = require('child_process');
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-upd-'));
    const rel = path.join(root, 'pack', 'UMK3-PC-9.9.9');
    fs.mkdirSync(path.join(rel, 'src'), { recursive: true });
    fs.writeFileSync(path.join(rel, 'UMK3-Launcher.exe'), '');
    fs.writeFileSync(path.join(rel, 'src', 'a.c'), '');
    const zip = path.join(root, 'r.zip');
    const tar = path.join(process.env.SystemRoot || 'C:\\Windows', 'System32', 'tar.exe');
    assert.strictEqual(spawnSync(tar, ['-a', '-cf', zip, '-C', path.join(root, 'pack'), 'UMK3-PC-9.9.9']).status, 0);
    const left = path.join(root, 'update', 'new', 'old', 'resources');
    fs.mkdirSync(left, { recursive: true });
    fs.writeFileSync(path.join(left, 'app.asar'), 'x');

    const rm = fs.rmSync;
    fs.rmSync = (p, o) => {
        if (String(p).includes(path.join('update', 'new')))
            throw Object.assign(new Error('ENOTEMPTY: directory not empty, rmdir'), { code: 'ENOTEMPTY' });
        return rm(p, o);
    };
    let r;
    try { r = upd.unpack(root, zip); } finally { fs.rmSync = rm; }
    assert.notStrictEqual(r.dest, path.join(root, 'update', 'new'));
    assert.ok(fs.existsSync(path.join(r.src, 'src', 'a.c')));

    r = upd.unpack(root, zip);          /* the next update clears them both */
    assert.strictEqual(r.dest, path.join(root, 'update', 'new'));
    assert.deepStrictEqual(fs.readdirSync(path.join(root, 'update')), ['new']);
    fs.rmSync(root, { recursive: true, force: true });
});
