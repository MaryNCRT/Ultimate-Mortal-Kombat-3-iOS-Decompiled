'use strict';

const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const ini = require('../lib/ini');

function tmpIni() {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-ini-'));
    return path.join(dir, 'umk3.ini');
}

test('defaults match launcher.c', () => {
    const c = ini.defaultConfig();
    assert.equal(c.res, 1);                       /* 960x640 */
    assert.equal(c.fullscreen, false);
    assert.equal(c.widescreen, false);
    assert.equal(c.debug, false);
    assert.equal(c.language, 0);
    assert.equal(c.ui, 'EN');
    assert.equal(c.ipa, '');
    assert.equal(c.keys.up, 0x57);
    assert.equal(c.keys.down, 0x53);
    assert.equal(c.keys.left, 0x41);
    assert.equal(c.keys.right, 0x44);
    assert.equal(c.keys.hp, 0x55);
    assert.equal(c.keys.lp, 0x49);
    assert.equal(c.keys.block, 0x4f);
    assert.equal(c.keys.hk, 0x4a);
    assert.equal(c.keys.lk, 0x4b);
    assert.equal(c.keys.run, 0x4c);
    assert.equal(c.keys.pause, 0x50);
    assert.equal(c.keys.moves, 0x4d);
});

test('missing file keeps defaults', () => {
    const p = tmpIni();
    const c = ini.load(p);
    assert.equal(c.res, 1);
    assert.equal(c.ui, 'EN');
    assert.equal(c.keys.moves, 0x4d);
    assert.ok(!fs.existsSync(p));
});

test('round-trip through save/load is lossless', () => {
    const c = ini.defaultConfig();
    c.res = 4;                 /* 2400x1600 */
    c.fullscreen = true;
    c.borderless = true;
    c.rres = 10;               /* 3D at 1920x1080 */
    c.antialiasing = 8;
    c.hideControls = true;
    c.widescreen = true;
    c.debug = true;
    c.skipIntro = true;
    c.language = 2;            /* ES */
    c.ui = 'EN';
    c.ipa = 'C:\\Juegos\\UMK3 v1.2.59.ipa';
    c.keys.up = 0x26;          /* Up arrow */
    c.keys.block = 0x20;       /* Space */
    const p = tmpIni();
    ini.save(p, c);
    const r = ini.load(p);
    assert.deepEqual(r, c);
    assert.equal(r.res, 4);
    assert.equal(r.fullscreen, true);
    assert.equal(r.language, 2);
    assert.equal(r.ui, 'EN');
    assert.equal(r.keys.up, 0x26);
    assert.equal(r.keys.block, 0x20);
});

test('reads a file written by the old C launcher', () => {
    const p = tmpIni();
    fs.writeFileSync(p, [
        'width=960', 'height=640', 'fullscreen=1', 'widescreen=1', 'language=',
        'ui=ES', 'ipa=C:/games/UMK3.ipa', 'debug_keys=0',
        'key_up=87', 'key_down=83', 'key_left=65', 'key_right=68',
        'key_hp=85', 'key_lp=73', 'key_block=79', 'key_hk=74', 'key_lk=75',
        'key_run=76', 'key_pause=80', 'key_moves=77',
    ].join('\n') + '\n', 'utf8');
    const c = ini.load(p);
    assert.equal(c.res, 1);
    assert.equal(c.fullscreen, true);
    assert.equal(c.language, 0);        /* "" = automatic */
    assert.equal(c.ipa, 'C:/games/UMK3.ipa');
    assert.equal(c.keys.up, 0x57);
});

test('unlisted resolution falls back to 960x640', () => {
    const p = tmpIni();
    fs.writeFileSync(p, 'width=1234\nheight=5678\n', 'utf8');
    const c = ini.load(p);
    assert.equal(c.res, 1);
});

test('gameReady only when exe and Info.plist exist', () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'umk3-root-'));
    assert.equal(ini.gameReady(dir), false);
    fs.writeFileSync(path.join(dir, 'umk3-game.exe'), 'x');
    assert.equal(ini.gameReady(dir), false);
    fs.mkdirSync(path.join(dir, 'res'));
    fs.writeFileSync(path.join(dir, 'res', 'Info.plist'), 'x');
    assert.equal(ini.gameReady(dir), true);
});
test('pad= round-trips and rejects junk', () => {
    const p = tmpIni();
    const c = ini.defaultConfig();
    assert.equal(c.pad, 'auto');
    c.pad = '054c:0ce6';
    c.padName = 'DualSense Wireless Controller';
    ini.save(p, c);
    const back = ini.load(p);
    assert.equal(back.pad, '054c:0ce6');
    assert.equal(back.padName, 'DualSense Wireless Controller');
    fs.writeFileSync(p, 'pad=evil;rm\n');
    assert.equal(ini.load(p).pad, 'auto');
});
test('aspect= round-trips and rejects junk', () => {
    const p = tmpIni();
    fs.writeFileSync(p, 'aspect=21:9\n');
    assert.equal(ini.load(p).aspect, '21:9');
    fs.writeFileSync(p, 'aspect=5:1\n');
    assert.equal(ini.load(p).aspect, '3:2');
});

test('ui_size= round-trips and rejects junk', () => {
    const p = tmpIni();
    fs.writeFileSync(p, 'ui_size=IPAD\n');
    assert.equal(ini.load(p).uiSize, 'ipad');
    fs.writeFileSync(p, 'ui_size=huge\n');
    assert.equal(ini.load(p).uiSize, 'iphone');
});

test('texture_quality= round-trips and rejects junk', () => {
    const p = tmpIni();
    const c = ini.defaultConfig();
    assert.equal(c.textureQuality, 'iphone');
    c.textureQuality = 'max';
    ini.save(p, c);
    assert.equal(ini.load(p).textureQuality, 'max');
    fs.writeFileSync(p, 'texture_quality=IPAD\n');
    assert.equal(ini.load(p).textureQuality, 'ipad');
    fs.writeFileSync(p, 'texture_quality=ultra\n');
    assert.equal(ini.load(p).textureQuality, 'iphone');
});
