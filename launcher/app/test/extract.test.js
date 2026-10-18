'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('path');
const fs = require('fs');

const { openZip } = require('../lib/extract/zip');
const { loadMachO } = require('../lib/extract/macho');
const { extractLevelInfo } = require('../lib/extract/level_info');
const { extractSeqData } = require('../lib/extract/seq_data');
const { transformLow32Static } = require('../lib/extract/low32_static');

const ROOT = path.resolve(__dirname, '../../..');
const IPA_PATH = path.join(ROOT, 'Ultimate_Mortal_Kombat__3_1.2.59_ios_3.0.ipa');

test('zip: opens IPA and reads Info.plist', { skip: !fs.existsSync(IPA_PATH) }, () => {
    const zip = openZip(IPA_PATH);
    const names = zip.namelist();
    assert.ok(names.length > 5000);
    assert.ok(zip.has('Payload/UMK3.app/Info.plist'));

    const plistData = zip.read('Payload/UMK3.app/Info.plist').toString('utf8');
    assert.ok(plistData.includes('1.2.59'));
    zip.close();
});

test('macho: loads armv7 slice and validates UUID', { skip: !fs.existsSync(IPA_PATH) }, () => {
    const zip = openZip(IPA_PATH);
    const bin = zip.read('Payload/UMK3.app/UMK3');
    const { m } = loadMachO(bin);
    assert.equal(m.arch(), 'armv7');
    assert.equal(m.uuid, '90d6f56a18e2303f8f2053b02a42742e');
    assert.equal(m.encryption ? m.encryption[2] : 0, 0);
    zip.close();
});

test('low32_static: transforms globals header and source', () => {
    const headerPath = path.join(ROOT, 'src/runtime/gamecode_globals.h');
    const srcPath = path.join(ROOT, 'src/runtime/gamecode_globals.c');
    if (!fs.existsSync(headerPath) || !fs.existsSync(srcPath)) return;

    const res = transformLow32Static([headerPath], srcPath, null, 'umk3_globals_fixup');
    assert.ok(res.fixupsCount > 900);
    assert.ok(res.content.includes('void umk3_globals_fixup(void)'));
});
