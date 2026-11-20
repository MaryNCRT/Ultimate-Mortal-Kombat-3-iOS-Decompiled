'use strict';

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { ensureToolchain, describeError } = require('./toolchain');
const { extractIpaPipeline } = require('./extract/ipa');
const { compileGame } = require('./compile');

let g_isCancelled = false;

/* Step boundaries as percentages of the build */
const STEPS = [
    { lo: 0,  hi: 18, title: 'Preparing compiler toolchain' },
    { lo: 18, hi: 25, title: 'Reading iOS binary from .ipa' },
    { lo: 25, hi: 42, title: 'Extracting data tables and assets' },
    { lo: 42, hi: 90, title: 'Compiling game sources' },
    { lo: 90, hi: 100, title: 'Linking final executable' },
];

function resolveSourceRoot(root) {
    if (fs.existsSync(path.join(root, 'src', 'runtime'))) {
        return root;
    }
    if (process.resourcesPath && fs.existsSync(path.join(process.resourcesPath, 'src', 'runtime'))) {
        return process.resourcesPath;
    }
    const repoFallback = path.resolve(__dirname, '../../..');
    if (fs.existsSync(path.join(repoFallback, 'src', 'runtime'))) {
        return repoFallback;
    }
    return root;
}

const VERSION = (() => { try { return require('../package.json').version; } catch (_) { return '?'; } })();

/* What another launcher's compile left in build/ (its objects, response and
 * dependency files, the link's list), removed when this launcher did not
 * make them: a version's objects are only ever reused by the same version.
 * build/built-by.txt names the version that compiled last. The data tables
 * made from the .ipa are made again every compile and stay. */
function cleanOtherVersion(buildDir, onData) {
    const mark = path.join(buildDir, 'built-by.txt');
    let was = '';
    try { was = fs.readFileSync(mark, 'utf8').trim(); } catch (_) { /* never */ }
    if (was === VERSION)
        return false;
    let n = 0;
    for (const d of ['obj', 'obj64']) {
        const p = path.join(buildDir, d);
        if (fs.existsSync(p)) {
            try { n += fs.readdirSync(p).length; } catch (_) { /* unreadable */ }
            fs.rmSync(p, { recursive: true, force: true });
        }
    }
    fs.rmSync(path.join(buildDir, 'link.rsp'), { force: true });
    if (n)
        onData({ text: `Removed ${n} files left by ${was ? 'version ' + was : 'an older version'}'s compile` });
    return n > 0;
}

/* REPAIR: what a compile leaves behind that can stop the next -- an object
 * without its response, stamp or dependency file, one of zero bytes, a
 * response file without its object, the link's list -- removed. The rest is
 * kept, so only what was broken compiles again. */
function cleanBroken(buildDir, onData) {
    let n = 0;
    for (const d of ['obj', 'obj64']) {
        const dir = path.join(buildDir, d);
        let files;
        try { files = fs.readdirSync(dir); } catch (_) { continue; }
        const have = new Set(files);
        for (const f of files) {
            const p = path.join(dir, f);
            let bad = false;
            if (f.endsWith('.o')) {
                let size = 0;
                try { size = fs.statSync(p).size; } catch (_) { /* gone */ }
                bad = size === 0 || !have.has(f + '.rsp') || !have.has(f + '.stamp') || !have.has(f + '.d');
                if (bad)
                    for (const x of ['', '.rsp', '.stamp', '.d'])
                        fs.rmSync(p + x, { force: true });
            } else if (/\.o\.(rsp|stamp|d)$/.test(f) && !have.has(f.replace(/\.(rsp|stamp|d)$/, ''))) {
                bad = true;
                fs.rmSync(p, { force: true });
            }
            if (bad)
                n++;
        }
    }
    fs.rmSync(path.join(buildDir, 'link.rsp'), { force: true });
    onData({ text: n ? `Repair: removed ${n} broken or orphaned build files` : 'Repair: no broken build files' });
}

/* The lines of a failure worth reading first: the compiler's "error:" lines
 * (not its notes and warnings), else the message's first line */
function errorSummary(err) {
    const msg = String((err && (err.message || err)) || 'unknown error');
    const lines = msg.split(/\r?\n/).filter(l => /error[:\s]/i.test(l) && !/warning:/i.test(l));
    const head = err && err.file ? err.file + ': ' : '';
    return (head + (lines.length ? lines.slice(0, 4).join('\n') : msg.split(/\r?\n/)[0])).slice(0, 1200);
}

/**
 * Runs the complete build pipeline: toolchain -> extraction -> compilation.
 * Calls onData({ text, pct, step, phase }) per event. `opts.repair`: the
 * .ipa's files are not extracted again when they are all there, broken build
 * files are removed and only what needs it compiles (REPAIR).
 * Every line also goes to build/last-build.log; a failure's summary is in
 * build.lastError.
 */
async function build(root, ipa, arch = 'x64', onDataIn = () => {}, opts = {}) {
    g_isCancelled = false;
    build.lastError = '';

    const srcRoot = resolveSourceRoot(root);
    const buildDir = path.join(root, 'build');
    fs.mkdirSync(buildDir, { recursive: true });
    const logLines = [];
    const onData = m => {
        if (m && m.text)
            logLines.push(m.text);
        onDataIn(m);
    };
    const writeLog = () => {
        try {
            fs.writeFileSync(path.join(buildDir, 'last-build.log'),
                `UMK3 launcher ${VERSION}, ${new Date().toISOString()}, ${opts.repair ? 'repair' : 'compile'}\n`
                + logLines.join('\n') + '\n');
        } catch (_) { /* no room */ }
    };

    try {
        cleanOtherVersion(buildDir, onData);
        if (opts.repair)
            cleanBroken(buildDir, onData);

        // Step 1: Ensure toolchain
        onData({
            text: '== 1/5 Preparing compiler toolchain...',
            step: 1,
            phase: STEPS[0].title,
            pct: 5,
        });

        const zigExe = await ensureToolchain(p => {
            const pct = Math.round(STEPS[0].lo + (p.pct / 100) * (STEPS[0].hi - STEPS[0].lo));
            onData({
                text: p.phase,
                step: 1,
                phase: p.phase,
                pct,
            });
        });

        if (g_isCancelled) {
            onData({ text: 'Build cancelled by user.', pct: -1 });
            return -1;
        }

        // Step 2 & 3: Extract IPA & Generate Tables & Extract Assets
        onData({
            text: '== 2/5 Inspecting .ipa and extracting assets...',
            step: 2,
            phase: STEPS[1].title,
            pct: 20,
        });

        const isLow32 = arch === 'x64';
        /* REPAIR: the .ipa's files are taken as they are when all there */
        const have = f => fs.existsSync(path.join(buildDir, f));
        const extracted = fs.existsSync(path.join(root, 'res', 'Info.plist'))
            && have('logic_tables.c') && have('level_info.c') && have('seq_data.c')
            && (!isLow32 || have('gamecode_globals64.c'));
        if (opts.repair && extracted) {
            onData({ text: "Repair: the .ipa's files are all there; not extracted again", step: 3, pct: 42 });
            const addSrc = path.join(srcRoot, 'launcher', 'aditional textures');
            if (fs.existsSync(addSrc))
                fs.cpSync(addSrc, path.join(root, 'aditional textures'), { recursive: true });
            const litSrc = path.join(srcRoot, 'launcher', 'lighting');
            if (fs.existsSync(litSrc))
                fs.cpSync(litSrc, path.join(root, 'lighting'), { recursive: true });
        } else await extractIpaPipeline({
            ipaPath: ipa,
            destDir: root,
            buildDir,
            rootDir: srcRoot,
            isLow32,
            onProgress: p => {
                let mappedPct;
                let step = 3;
                if (p.pct < 40) {
                    step = 2;
                    mappedPct = Math.round(STEPS[1].lo + ((p.pct - 20) / 20) * (STEPS[1].hi - STEPS[1].lo));
                } else {
                    step = 3;
                    mappedPct = Math.round(STEPS[2].lo + ((p.pct - 40) / 60) * (STEPS[2].hi - STEPS[2].lo));
                }

                onData({
                    text: p.phase + (p.file ? ` (${p.file})` : ''),
                    step,
                    phase: p.phase,
                    pct: mappedPct,
                });
            },
        });

        if (g_isCancelled) {
            onData({ text: 'Build cancelled by user.', pct: -1 });
            return -1;
        }

        // Step 4 & 5: Compile & Link
        onData({
            text: '== 4/5 Compiling sources in parallel...',
            step: 4,
            phase: STEPS[3].title,
            pct: 45,
        });

        const targetOs = process.platform === 'win32' ? 'windows' : 'linux';
        await compileGame({
            rootDir: srcRoot,
            buildDir,
            destDir: root,
            targetArch: arch,
            targetOs,
            zigExe,
            onProgress: p => {
                let mappedPct;
                let step = 4;
                if (p.pct < 92) {
                    step = 4;
                    mappedPct = Math.round(STEPS[3].lo + ((p.pct - 42) / 50) * (STEPS[3].hi - STEPS[3].lo));
                } else {
                    step = 5;
                    mappedPct = Math.round(STEPS[4].lo + ((p.pct - 92) / 8) * (STEPS[4].hi - STEPS[4].lo));
                }

                onData({
                    text: p.phase,
                    step,
                    phase: p.phase,
                    pct: mappedPct,
                });
            },
        });

        try { fs.writeFileSync(path.join(buildDir, 'built-by.txt'), VERSION); } catch (_) { /* no room */ }
        onData({
            text: 'Build finished successfully! The game is ready.',
            step: 5,
            phase: 'Completed',
            pct: 100,
        });
        writeLog();
        return 0;
    } catch (err) {
        onData({
            text: `Build failed: ${err.message || describeError(err)}`,
            pct: -1,
            phase: 'Error',
        });
        build.lastError = errorSummary(err);
        writeLog();
        return -1;
    }
}

function cancelBuild() {
    g_isCancelled = true;
    return true;
}

function play(root, exeName, args = [], env = null) {
    const exePath = path.join(root, exeName);
    if (!fs.existsSync(exePath)) {
        return false;
    }

    if (process.platform !== 'win32') {
        try {
            fs.chmodSync(exePath, 0o755);
        } catch (_) {}
    }

    /* detached: the game outlives the launcher if that is closed; the
     * caller hears when it ends (main.js brings its window back) */
    return spawn(exePath, args, {
        cwd: root,
        env: env ? { ...process.env, ...env } : process.env,
        detached: true,
        stdio: 'ignore',
        windowsHide: false,
    });
}

module.exports = {
    build,
    cancelBuild,
    play,
    STEPS,
};