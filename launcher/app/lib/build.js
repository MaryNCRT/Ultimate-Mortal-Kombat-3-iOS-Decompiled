'use strict';

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { ensureToolchain, describeError } = require('./toolchain');
const { extractIpaPipeline } = require('./extract/ipa');
const { compileGame } = require('./compile');
const vfy = require('./verify');

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
function scanBroken(buildDir) {
    const out = [];
    for (const d of ['obj', 'obj64']) {
        const dir = path.join(buildDir, d);
        let files;
        try { files = fs.readdirSync(dir); } catch (_) { continue; }
        const have = new Set(files);
        for (const f of files) {
            const p = path.join(dir, f);
            if (f.endsWith('.o')) {
                let size = 0;
                try { size = fs.statSync(p).size; } catch (_) { /* gone */ }
                if (size === 0 || !have.has(f + '.rsp') || !have.has(f + '.stamp') || !have.has(f + '.d'))
                    for (const x of ['', '.rsp', '.stamp', '.d'])
                        if (have.has(f + x))
                            out.push(p + x);
            } else if (/\.o\.(rsp|stamp|d)$/.test(f) && !have.has(f.replace(/\.(rsp|stamp|d)$/, '')))
                out.push(p);
        }
    }
    return out;
}

function cleanBroken(buildDir, onData) {
    const bad = scanBroken(buildDir);
    for (const p of bad)
        fs.rmSync(p, { force: true });
    if (bad.length)
        fs.rmSync(path.join(buildDir, 'link.rsp'), { force: true });
    onData({ text: bad.length ? `Repair: removed ${bad.length} broken or orphaned build files`
                              : 'Repair: no broken build files' });
}

/* A folder's files copied where they are missing (or of another size; with
 * `newer`, also where the source is newer -- DLSS 5's DLLs change without
 * changing size), the rest left alone */
function copyMissing(from, to, newer = false) {
    let n = 0;
    let list;
    try { list = fs.readdirSync(from, { withFileTypes: true }); } catch (_) { return 0; }
    for (const e of list) {
        const a = path.join(from, e.name), b = path.join(to, e.name);
        if (e.isDirectory())
            n += copyMissing(a, b, newer);
        else {
            let same = false;
            try {
                const sa = fs.statSync(a), sb = fs.statSync(b);
                same = sa.size === sb.size && (!newer || sa.mtimeMs <= sb.mtimeMs);
            } catch (_) { /* missing */ }
            if (!same) {
                fs.mkdirSync(to, { recursive: true });
                fs.copyFileSync(a, b);
                n++;
            }
        }
    }
    return n;
}

function exeName(arch) {
    return process.platform === 'win32' ? (arch === 'x64' ? 'umk3-game64.exe' : 'umk3-game.exe')
                                        : 'umk3-game';
}

function extractedOk(root, buildDir, isLow32) {
    const have = f => fs.existsSync(path.join(buildDir, f));
    return fs.existsSync(path.join(root, 'res', 'Info.plist'))
        && have('logic_tables.c') && have('level_info.c') && have('seq_data.c')
        && (!isLow32 || have('gamecode_globals64.c'));
}

/**
 * VERIFY (Mary, 0.5.6): what in the game folder is missing or damaged, told
 * line by line, nothing changed. { need, sources, srcError, broken,
 * otherVersion, extracted, exe, lines }. `need`: REPAIR has something to do.
 */
async function verifyFolder(root, arch, fetchFn, onText = () => {}) {
    const buildDir = path.join(root, 'build');
    const srcRoot = resolveSourceRoot(root);
    const isLow32 = arch === 'x64';
    const r = { sources: null, srcError: '', broken: [], otherVersion: '', extracted: true,
                exe: true, lines: [] };
    const say = t => { r.lines.push(t); onText(t); };

    say(`Verify: launcher ${VERSION}, game folder ${root}`);
    try {
        const list = await vfy.manifest(fetchFn, buildDir, VERSION);
        r.sources = vfy.scanSources(srcRoot, list);
        const bad = r.sources.missing.length + r.sources.damaged.length;
        say(bad ? `Sources: ${r.sources.missing.length} missing, ${r.sources.damaged.length} damaged, of ${list.length}`
                : `Sources: all ${list.length} files match the repository`);
        for (const e of r.sources.missing.slice(0, 20))
            say(`  missing  ${e.path}`);
        for (const e of r.sources.damaged.slice(0, 20))
            say(`  damaged  ${e.path}`);
    } catch (err) {
        r.srcError = err.message;
        say(`Sources: not checked (${err.message})`);
    }
    r.broken = scanBroken(buildDir);
    say(r.broken.length ? `Build files: ${r.broken.length} broken or orphaned` : 'Build files: none broken');
    {
        let was = '';
        try { was = fs.readFileSync(path.join(buildDir, 'built-by.txt'), 'utf8').trim(); } catch (_) { /* never */ }
        const objs = ['obj', 'obj64'].some(d => fs.existsSync(path.join(buildDir, d)));
        if (objs && was !== VERSION) {
            r.otherVersion = was || 'an older version';
            say(`Build files: left by ${was ? 'version ' + was : 'an older version'}; they are compiled again`);
        }
    }
    r.extracted = extractedOk(root, buildDir, isLow32);
    say(r.extracted ? "The .ipa's files: there" : "The .ipa's files: missing (REPAIR extracts the .ipa again)");
    r.exe = fs.existsSync(path.join(root, exeName(arch)));
    say(r.exe ? `The game: ${exeName(arch)} is there` : `The game: ${exeName(arch)} is missing`);
    r.need = !!((r.sources && (r.sources.missing.length || r.sources.damaged.length))
                || r.broken.length || r.otherVersion || !r.extracted || !r.exe);
    say(r.need ? 'Result: REPAIR is needed' : 'Result: everything is in order, nothing to repair');
    return r;
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
        /* REPAIR: only what VERIFY finds -- the sources that are missing or
         * damaged downloaded again, the broken build files removed -- and
         * nothing compiled when nothing is wrong */
        if (opts.repair) {
            const v = await verifyFolder(root, arch, opts.fetch, t => onData({ text: t }));
            const bad = v.sources ? v.sources.missing.concat(v.sources.damaged) : [];
            if (bad.length) {
                if (!opts.fetch)
                    throw new Error('sources are missing and there is no network to download them');
                onData({ text: `Repair: downloading ${bad.length} files from the repository (v${VERSION})`, pct: 3 });
                const got = await vfy.restore(opts.fetch, resolveSourceRoot(root), VERSION, bad,
                                              t => onData({ text: t }));
                if (got.failed.length)
                    throw new Error(`${got.failed.length} files could not be downloaded: ${got.failed.slice(0, 3).join(', ')}`);
            }
            if (!v.need) {
                onData({ text: 'Repair: nothing is damaged; nothing was changed', step: 5, pct: 100 });
                writeLog();
                return 0;
            }
            cleanBroken(buildDir, onData);
        }
        cleanOtherVersion(buildDir, onData);

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
        if (opts.repair && extractedOk(root, buildDir, isLow32)) {
            onData({ text: "Repair: the .ipa's files are all there; not extracted again", step: 3, pct: 42 });
            const n = copyMissing(path.join(srcRoot, 'launcher', 'aditional textures'),
                                  path.join(root, 'aditional textures'))
                    + copyMissing(path.join(srcRoot, 'launcher', 'lighting'), path.join(root, 'lighting'));
            if (n)
                onData({ text: `Repair: ${n} textures and lighting files put back` });
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

        /* DLSS 5 (src/runtime/neural_mod.c): what the package brings in
         * launcher/dlss5 -- our ReShade, DLSS5-Feeder -- beside the game in
         * graphics/dlss5. Only copied over, never deleted: NVIDIA's files and
         * the consumer the player added there stay through every update */
        {
            const n = copyMissing(path.join(srcRoot, 'launcher', 'dlss5'), path.join(root, 'graphics', 'dlss5'), true);
            if (n)
                onData({ text: `DLSS 5: ${n} files put in graphics\\dlss5` });
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
    verifyFolder,
    cancelBuild,
    play,
    STEPS,
};