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
