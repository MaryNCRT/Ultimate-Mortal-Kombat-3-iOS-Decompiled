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

/**
 * Runs the complete build pipeline: toolchain -> extraction -> compilation.
 * Calls onData({ text, pct, step, phase }) per event.
 */
async function build(root, ipa, arch = 'x64', onData = () => {}) {
    g_isCancelled = false;

    const srcRoot = resolveSourceRoot(root);
    const buildDir = path.join(root, 'build');
    fs.mkdirSync(buildDir, { recursive: true });

    try {
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
        await extractIpaPipeline({
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

        onData({
            text: 'Build finished successfully! The game is ready.',
            step: 5,
            phase: 'Completed',
            pct: 100,
        });
        return 0;
    } catch (err) {
        onData({
            text: `Build failed: ${err.message || describeError(err)}`,
            pct: -1,
            phase: 'Error',
        });
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