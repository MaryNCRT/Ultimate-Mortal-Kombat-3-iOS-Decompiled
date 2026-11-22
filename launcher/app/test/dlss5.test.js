'use strict';
/* lib/dlss5.js: which picked files are taken, the game's status line, and
 * what an import does with names it refuses. */
const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const dlss5 = require('../lib/dlss5');
const ini = require('../lib/ini');

test('classify: NVIDIA runtimes, consumer add-ons, the rest refused', () => {
    assert.strictEqual(dlss5.classify('C:\\x\\nvngx_dlss.dll'), 'nvngx');
    assert.strictEqual(dlss5.classify('NVNGX_DLSSNR.DLL'), 'nvngx');
    assert.strictEqual(dlss5.classify('renodx-dlss5.addon64'), 'consumer');
    assert.strictEqual(dlss5.classify('renodx-dlss5-v6.1.addon64'), 'consumer');
    assert.strictEqual(dlss5.classify('deep-fried-chicken.addon64'), 'consumer');
    assert.strictEqual(dlss5.classify('dlss5-feed-helper.addon64'), null);
    assert.strictEqual(dlss5.classify('nvngx_dlssg.dll'), null);
    assert.strictEqual(dlss5.classify('dxgi.dll'), null);
});

test('parseStatus: the game\'s --dlss5-status line', () => {
    assert.deepStrictEqual(dlss5.parseStatus('noise\ndlss5\t6\tneeds NVIDIA\'s files\t1\r\n'),
        { status: 'need_files', text: 'needs NVIDIA\'s files', on: true });
    assert.deepStrictEqual(dlss5.parseStatus('dlss5\t1\tready\t0'), { status: 'ready', text: 'ready', on: false });
    assert.strictEqual(dlss5.parseStatus('dlss5	7	ready, DLSS (DLAA) alone	1').status, 'dlaa_only');
    assert.strictEqual(dlss5.parseStatus('').status, 'unknown');
});

test('importFiles refuses names that are not DLSS 5 files, copies nothing', async () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-dlss5-'));
    const src = path.join(root, 'dxgi.dll');
    fs.writeFileSync(src, 'x');
    const r = await dlss5.importFiles(root, [src]);
    assert.strictEqual(r.length, 1);
    assert.strictEqual(r[0].ok, false);
    assert.deepStrictEqual(fs.readdirSync(path.join(root, dlss5.DIR)), []);
});

test('importFiles keeps one consumer: the old one is set aside as .old', async () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-dlss5-'));
    const dir = path.join(root, dlss5.DIR);
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, 'deep-fried-chicken.addon64'), 'old');
    const src = path.join(root, 'renodx-dlss5.addon64');
    fs.writeFileSync(src, 'new');
    const r = await dlss5.importFiles(root, [src]);
    assert.strictEqual(r[0].ok, true);
    const names = fs.readdirSync(dir).sort();
    assert.deepStrictEqual(names, ['deep-fried-chicken.addon64.old', 'renodx-dlss5.addon64']);
    assert.strictEqual(dlss5.present(root).consumer, 'renodx-dlss5.addon64');
});

test('umk3.ini dlss5= round trip', () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-dlss5-'));
    const p = path.join(root, 'umk3.ini');
    const cfg = ini.defaultConfig();
    assert.strictEqual(cfg.dlss5, false);
    ini.save(p, { ...cfg, dlss5: true });
    assert.match(fs.readFileSync(p, 'utf8'), /^dlss5=1$/m);
    assert.strictEqual(ini.load(p).dlss5, true);
});
