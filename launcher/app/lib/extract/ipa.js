'use strict';

const fs = require('fs');
const path = require('path');
const { openZip } = require('./zip');
const { loadMachO } = require('./macho');
const { extractLogicTables } = require('./logic_tables');
const { extractLevelInfo } = require('./level_info');
const { extractSeqData } = require('./seq_data');
const { transformLow32Static } = require('./low32_static');

const WANT_UUID = '90d6f56a18e2303f8f2053b02a42742e';
const WANT_VERSION = '1.2.59';

/**
 * Inspects the given IPA file and returns metadata.
 */
function inspectIpa(ipaPath) {
    const zip = openZip(ipaPath);
    try {
        let appPrefix = null;
        let binEntry = null;
        let plistEntry = null;

        for (const name of zip.namelist()) {
            const m = name.match(/^(Payload\/[^/]+\.app\/)/);
            if (m && !appPrefix) {
                appPrefix = m[1];
            }
            if (/^Payload\/[^/]+\.app\/UMK3$/.test(name)) {
                binEntry = name;
            } else if (/^Payload\/[^/]+\.app\/Info\.plist$/.test(name)) {
                plistEntry = name;
            }
        }

        if (!appPrefix || !binEntry) {
            throw new Error(`${path.basename(ipaPath)} does not contain Payload/UMK3.app/UMK3`);
        }

        let version = 'unknown';
        if (plistEntry) {
            try {
                const plXml = zip.read(plistEntry).toString('utf8');
                const mVer = plXml.match(/<key>CFBundle(?:ShortVersionString|Version)<\/key>\s*<string>([^<]+)<\/string>/);
                if (mVer) {
                    version = mVer[1];
                }
            } catch (_) {
                // Plist parsing failure is non-fatal for inspection
            }
        }

        const rawBin = zip.read(binEntry);
        const { m } = loadMachO(rawBin);
        const cryptid = m.encryption ? m.encryption[2] : 0;

        return {
            appPrefix,
            binEntry,
            plistEntry,
            version,
            uuid: m.uuid,
            cryptid,
            arch: m.arch(),
            isEncrypted: cryptid !== 0,
            isCompatible: m.uuid === WANT_UUID && cryptid === 0,
        };
    } finally {
        zip.close();
    }
}

/**
 * Extracts assets and generates C source tables from the IPA.
 *
 * @param {Object} options
 * @param {string} options.ipaPath Path to the .ipa file.
 * @param {string} options.destDir Target root directory for res/ and assets.
 * @param {string} options.buildDir Target directory for generated .c files.
 * @param {string} [options.rootDir] Repository root path.
 * @param {boolean} [options.isLow32] Whether to generate low32 static globals.
 * @param {boolean} [options.allowMismatch] Proceed even if UUID does not match 1.2.59.
 * @param {function} [options.onProgress] Callback for progress updates: { step, total, file, pct, phase }.
 */
/* A generated table written to a file beside it and put in place only when
 * it differs: the same file keeps its date, and the compile (compile.js,
 * upToDate) does not build it again. */
function keepIfSame(out, make) {
    const tmp = out + '.new';
    make(tmp);
    let same = false;
    try { same = fs.readFileSync(tmp).equals(fs.readFileSync(out)); } catch (_) { same = false; }
    if (same) fs.rmSync(tmp, { force: true });
    else fs.renameSync(tmp, out);
}

async function extractIpaPipeline({
    ipaPath,
    destDir,
    buildDir,
    rootDir = path.resolve(__dirname, '../../../../'),
    isLow32 = true,
    allowMismatch = false,
    onProgress = () => {},
}) {
    if (!fs.existsSync(ipaPath)) {
        throw new Error(`.ipa not found: ${ipaPath}`);
    }

    onProgress({ phase: 'Inspecting .ipa', pct: 20 });
    const zip = openZip(ipaPath);

    try {
        let appPrefix = null;
        let binEntry = null;

        for (const name of zip.namelist()) {
            const m = name.match(/^(Payload\/[^/]+\.app\/)/);
            if (m && !appPrefix) {
                appPrefix = m[1];
            }
            if (/^Payload\/[^/]+\.app\/UMK3$/.test(name)) {
                binEntry = name;
            }
        }

        if (!appPrefix || !binEntry) {
            throw new Error(`${path.basename(ipaPath)} does not contain Payload/UMK3.app/UMK3`);
        }

        onProgress({ phase: 'Verifying iOS binary', pct: 22 });
        const rawBin = zip.read(binEntry);
        const { m } = loadMachO(rawBin);
        const cryptid = m.encryption ? m.encryption[2] : 0;

        if (cryptid !== 0) {
            throw new Error(`The binary is FairPlay encrypted (cryptid=${cryptid}). A decrypted .ipa is required.`);
        }

        if (m.uuid !== WANT_UUID && !allowMismatch) {
            throw new Error(`The binary UUID is ${m.uuid}, but the decompiler requires version 1.2.59 (${WANT_UUID}).`);
        }

        // 1. Generate logic_tables.c
        onProgress({ phase: 'Generating logic_tables.c', pct: 25 });
        const mapPath = path.join(rootDir, 'tools/build/logic_tables.map');
        const outLogicC = path.join(buildDir, 'logic_tables.c');
        keepIfSame(outLogicC, tmp => extractLogicTables(rawBin, tmp, mapPath));

        // 2. Generate level_info.c
        onProgress({ phase: 'Generating level_info.c', pct: 30 });
        const outLevelC = path.join(buildDir, 'level_info.c');
        keepIfSame(outLevelC, tmp => extractLevelInfo(rawBin, tmp, 'UMK3'));

        // 3. Generate seq_data.c
        onProgress({ phase: 'Generating seq_data.c', pct: 35 });
        const outSeqC = path.join(buildDir, 'seq_data.c');
        keepIfSame(outSeqC, tmp => extractSeqData(rawBin, tmp, 'UMK3'));

        // 4. Generate gamecode_globals64.c if Low32
        if (isLow32) {
            onProgress({ phase: 'Generating gamecode_globals64.c', pct: 38 });
            const globalsH = path.join(rootDir, 'src/runtime/gamecode_globals.h');
            const globalsC = path.join(rootDir, 'src/runtime/gamecode_globals.c');
            const outGlobalsC = path.join(buildDir, 'gamecode_globals64.c');
            keepIfSame(outGlobalsC, tmp => transformLow32Static([globalsH], globalsC, tmp, 'umk3_globals_fixup'));
        }

        // 5. Extract assets: res/* and Info.plist
        onProgress({ phase: 'Extracting game assets', pct: 40 });
        const resDir = path.join(destDir, 'res');
        fs.mkdirSync(resDir, { recursive: true });

        const resPrefix = appPrefix + 'res/';
        const entries = zip.namelist();
        const assetEntries = entries.filter(name => {
            if (name.endsWith('/')) return false;
            return name.startsWith(resPrefix) || name === (appPrefix + 'Info.plist');
        });

        const totalAssets = assetEntries.length;
        let extractedCount = 0;

        /* What an earlier compile extracted, by the zip's CRC of each file:
         * a file already there with that CRC and its size is not inflated
         * and written again. A compile after an update took 4 to 8 s here
         * for nothing; now a fraction of one. */
        const manifestPath = path.join(resDir, '.extracted.json');
        let had = {};
        try { had = JSON.parse(fs.readFileSync(manifestPath, 'utf8')) || {}; } catch (_) { had = {}; }
        const now = {};
        const made = new Set();

        for (const name of assetEntries) {
            let rel;
            if (name.startsWith(resPrefix)) {
                rel = name.slice(resPrefix.length);
            } else {
                rel = 'Info.plist';
            }

            const outPath = path.join(resDir, ...rel.split('/'));
            const ent = zip.get(name);
            let same = false;
            if (ent && had[rel] === ent.crc) {
                try { same = fs.statSync(outPath).size === ent.uncompSize; } catch (_) { same = false; }
            }
            if (!same) {
                const dir = path.dirname(outPath);
                if (!made.has(dir)) {
                    fs.mkdirSync(dir, { recursive: true });
                    made.add(dir);
                }
                fs.writeFileSync(outPath, zip.read(name));
            }
            if (ent) now[rel] = ent.crc;

            extractedCount++;
            if (extractedCount % 100 === 0 || extractedCount === totalAssets) {
                const pct = Math.round(40 + (extractedCount / totalAssets) * 50);
                onProgress({
                    phase: `Extracting assets (${extractedCount}/${totalAssets})`,
                    pct,
                    done: extractedCount,
                    total: totalAssets,
                    file: rel,
                });
            }
        }

        fs.writeFileSync(manifestPath, JSON.stringify(now));

        // Copy additional textures if present
        const addSrc = path.join(rootDir, 'launcher/aditional textures');
        if (fs.existsSync(addSrc)) {
            const addDst = path.join(destDir, 'aditional textures');
            fs.cpSync(addSrc, addDst, { recursive: true });
        }

        // Default umk3.ini
        const iniPath = path.join(destDir, 'umk3.ini');
        if (!fs.existsSync(iniPath)) {
            fs.writeFileSync(iniPath, 'width=960\nheight=640\nfullscreen=0\nlanguage=\n', 'ascii');
        }

        onProgress({ phase: 'Extraction completed', pct: 90 });
        return {
            success: true,
            version: WANT_VERSION,
            uuid: m.uuid,
            assetsExtracted: totalAssets,
        };
    } finally {
        zip.close();
    }
}

module.exports = {
    inspectIpa,
    extractIpaPipeline,
    WANT_UUID,
    WANT_VERSION,
};
