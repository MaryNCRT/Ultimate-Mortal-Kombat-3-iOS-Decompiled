'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const { spawn, execSync } = require('child_process');

function getCSourceFiles(dir) {
    if (!fs.existsSync(dir)) return [];
    return fs.readdirSync(dir)
        .filter(f => f.endsWith('.c'))
        .map(f => path.join(dir, f));
}

function isStateFile(srcPath, buildDir) {
    const filename = path.basename(srcPath);
    const normalized = srcPath.replace(/\\/g, '/');
    const normalizedBuild = buildDir.replace(/\\/g, '/');

    if (normalized.includes('/src/decomp/')) return true;
    if (normalized.includes('/src/runtime/lime/')) return true;
    if (normalized.startsWith(normalizedBuild)) return true;

    const stateRuntimeNames = [
        'gamecode_globals.c',
        'gamecode_globals64.c',
        'gamecode_stubs.c',
        'lime_menu.c',
        'lime_platform.c',
        'lime_app.c',
        'wav.c',
        'fight_runtime.c',
    ];

    if (normalized.includes('/src/runtime/') && stateRuntimeNames.includes(filename)) {
        return true;
    }

    return false;
}

/**
 * Compiles a single source file into an object file.
 */
function compileSingleFile(zigExe, args, rspPath) {
    return new Promise((resolve, reject) => {
        // Write arguments to response file to avoid CLI length limits on Windows
        const rspContent = args.map(a => a.startsWith('-') ? a : `"${a.replace(/\\/g, '/')}"`).join('\n');
        fs.writeFileSync(rspPath, rspContent, 'utf8');

        const child = spawn(zigExe, ['cc', `@${rspPath}`], {
            stdio: ['ignore', 'pipe', 'pipe'],
            windowsHide: true,
        });

        let stderr = '';
        child.stderr.on('data', d => { stderr += d.toString('utf8'); });

        child.on('close', code => {
            if (code === 0) {
                resolve();
            } else {
                reject(new Error(`Error compilando ${path.basename(rspPath, '.rsp')}: ${stderr || 'código ' + code}`));
            }
        });

        child.on('error', reject);
    });
}

/**
 * Runs tasks with a concurrency pool.
 */
async function runWithPool(tasks, concurrency, onItemComplete) {
    let index = 0;
    const running = new Set();
    const results = [];

    async function next() {
        if (index >= tasks.length) return;
        const currentIdx = index++;
        const task = tasks[currentIdx];
        const promise = task().then(
            res => {
                onItemComplete(currentIdx, null);
                return res;
            },
            err => {
                onItemComplete(currentIdx, err);
                throw err;
            }
        );

        running.add(promise);
        promise.finally(() => running.delete(promise));

        if (running.size < concurrency && index < tasks.length) {
            next();
        }

        return promise;
    }

    const initialBatches = [];
    for (let i = 0; i < Math.min(concurrency, tasks.length); i++) {
        initialBatches.push(next());
    }

    while (running.size > 0) {
        await Promise.race(running);
        if (index < tasks.length) {
            next();
        }
    }
}

/**
 * Orchestrates the full native compilation of UMK3.
 */
async function compileGame({
    rootDir = path.resolve(__dirname, '../../..'),
    buildDir,
    destDir,
    targetArch = 'x64',
    targetOs = process.platform === 'win32' ? 'windows' : 'linux',
    zigExe,
    onProgress = () => {},
}) {
    const isWindows = targetOs === 'windows';
    const isLow32 = targetArch === 'x64';
    const objDir = path.join(buildDir, isLow32 ? 'obj64' : 'obj');
    fs.mkdirSync(objDir, { recursive: true });

    // 1. Gather all source files
    const sources = [
        path.join(rootDir, 'src/runtime/game_main.c'),
        path.join(rootDir, 'src/runtime/debug_menu.c'),
        path.join(rootDir, 'src/runtime/settings_menu.c'),
        path.join(rootDir, 'src/runtime/pad_nav.c'),
        path.join(rootDir, 'src/runtime/profiles.c'),
        path.join(rootDir, 'src/runtime/draw_gl.c'),
        path.join(rootDir, 'src/runtime/gamecode_stubs.c'),
        path.join(rootDir, 'src/runtime/mp_net.c'),
        path.join(rootDir, 'src/runtime/online_ui.c'),
        path.join(rootDir, 'src/runtime/lime_menu.c'),
        path.join(rootDir, 'src/runtime/lime_platform.c'),
        path.join(rootDir, 'src/runtime/lime_app.c'),
        path.join(rootDir, 'src/runtime/wav.c'),
        path.join(rootDir, 'src/runtime/fight_runtime.c'),
        path.join(rootDir, 'src/runtime/savestate.c'),
    ];

    if (isWindows) {
        sources.push(
            path.join(rootDir, 'src/runtime/platform/win32_gl.c'),
            path.join(rootDir, 'src/runtime/platform/win32_gl_cdecl.c'),
            path.join(rootDir, 'src/runtime/platform/win32_audio.c')
        );
    } else {
        sources.push(
            path.join(rootDir, 'src/runtime/platform/sdl_gl.c'),
            path.join(rootDir, 'src/runtime/platform/sdl_audio.c')
        );
    }

    // Glob directory sources
    for (const d of ['src/decomp/gamecode', 'src/decomp/lime', 'src/runtime/lime', 'src/decomp/gamecode/logic']) {
        sources.push(...getCSourceFiles(path.join(rootDir, d)));
    }

    // Generated tables
    sources.push(
        path.join(buildDir, 'logic_tables.c'),
        path.join(buildDir, 'level_info.c'),
        path.join(buildDir, 'seq_data.c')
    );

    // Globals (low32 or standard)
    if (isLow32) {
        sources.push(path.join(buildDir, 'gamecode_globals64.c'));
    } else {
        sources.push(path.join(rootDir, 'src/runtime/gamecode_globals.c'));
    }

    // 2. Base flags
    const baseFlags = [
        '-std=gnu11',
        '-w',
        '-DUMK3_HAVE_MK3',
        '-DUMK3_REAL_GL',
        '-DUMK3_SHELL',
        '-I', path.join(rootDir, 'src/runtime'),
        '-I', path.join(rootDir, 'src/decomp/lime'),
        '-I', path.join(rootDir, 'src/decomp/gamecode/logic'),
        '-include', path.join(rootDir, 'src/runtime/savestate.h'),
    ];

    if (isWindows) {
        baseFlags.push(`-target`, isLow32 ? 'x86_64-windows-gnu' : 'x86-windows-gnu');
    } else {
        if (process.platform !== 'linux') {
            baseFlags.push(`-target`, isLow32 ? 'x86_64-linux-gnu' : 'x86-linux-gnu');
        }
        baseFlags.push('-D_GNU_SOURCE=1', '-D_REENTRANT', '-DUMK3_NO_SDL_MIXER=1', '-D_strdup=strdup', '-D_snprintf=snprintf');
        if (fs.existsSync('/usr/include')) {
            baseFlags.push('-I', '/usr/include');
        }
        if (fs.existsSync('/usr/include/SDL2')) {
            baseFlags.push('-I', '/usr/include/SDL2');
        }
    }

    if (isLow32) {
        baseFlags.push(
            '-DUMK3_LOW32',
            '-fms-extensions',
            '-O2',
            '-fno-strict-aliasing',
            '-fno-jump-tables'
        );
    }

    const objFiles = [];
    const tasks = [];

    for (let i = 0; i < sources.length; i++) {
        const src = sources[i];
        const baseName = path.basename(src, '.c');
        const objName = `${String(i + 1).padStart(3, '0')}_${baseName}.o`;
        const objPath = path.join(objDir, objName);
        const rspPath = path.join(objDir, `${objName}.rsp`);
        objFiles.push(objPath);

        const fileFlags = [...baseFlags];
        if (isStateFile(src, buildDir)) {
            fileFlags.push('-DUMK3_STATE_FILE');
        }

        const compileArgs = [...fileFlags, '-c', src, '-o', objPath];
        tasks.push(() => compileSingleFile(zigExe, compileArgs, rspPath));
    }

    // Windows icon resource (.rc)
    if (isWindows) {
        const rcPath = path.join(rootDir, 'src/runtime/umk3_game.rc');
        const resObj = path.join(objDir, 'umk3_game.res');
        objFiles.push(resObj);
        tasks.push(async () => {
            execSync(`"${zigExe}" rc /fo "${resObj}" "${rcPath}"`, { stdio: 'pipe' });
        });
    }

    const totalTasks = tasks.length;
    let completedCount = 0;
    const concurrency = Math.max(1, os.cpus().length);

    onProgress({
        phase: `Compilando código del juego (0/${totalTasks})`,
        pct: 42,
        done: 0,
        total: totalTasks,
    });

    await runWithPool(tasks, concurrency, (idx, err) => {
        if (!err) {
            completedCount++;
            const pct = Math.round(42 + (completedCount / totalTasks) * 48);
            const fileName = path.basename(sources[idx] || 'icon');
            onProgress({
                phase: `Compiling [${completedCount}/${totalTasks}] ${fileName}`,
                pct,
                done: completedCount,
                total: totalTasks,
                file: fileName,
            });
        }
    });

    // 3. Linking
    onProgress({ phase: 'Linking final executable...', pct: 92 });
    const exeName = isWindows ? (isLow32 ? 'umk3-game64.exe' : 'umk3-game.exe') : 'umk3-game';
    const outExe = path.join(destDir, exeName);

    const linkArgs = [...objFiles, '-o', outExe];
    if (isWindows) {
        linkArgs.push(
            `-target`, isLow32 ? 'x86_64-windows-gnu' : 'x86-windows-gnu',
            '-lopengl32', '-lwinmm', '-lgdi32', '-luser32', '-lkernel32', '-lws2_32'
        );
        if (isLow32) {
            linkArgs.push('-Wl,--image-base,0x400000', '-Wl,--disable-dynamicbase', '-Wl,--disable-high-entropy-va');
        } else {
            linkArgs.push('-Wl,--large-address-aware');
        }
    } else {
        if (process.platform !== 'linux') {
            linkArgs.push(`-target`, isLow32 ? 'x86_64-linux-gnu' : 'x86-linux-gnu');
        }
        linkArgs.push('-no-pie');
        const libDirs = ['/usr/lib', '/usr/lib/x86_64-linux-gnu', '/usr/lib64', '/lib/x86_64-linux-gnu', '/lib64'];
        for (const dir of libDirs) {
            if (fs.existsSync(dir)) {
                linkArgs.push(`-L${dir}`);
            }
        }
        linkArgs.push('-lSDL2', '-lGL', '-lm');
    }

    const linkRsp = path.join(buildDir, 'link.rsp');
    const linkRspContent = linkArgs.map(a => a.startsWith('-') ? a : `"${a.replace(/\\/g, '/')}"`).join('\n');
    fs.writeFileSync(linkRsp, linkRspContent, 'utf8');

    await new Promise((resolve, reject) => {
        const linkChild = spawn(zigExe, ['cc', `@${linkRsp}`], {
            stdio: ['ignore', 'pipe', 'pipe'],
            windowsHide: true,
        });

        let linkErr = '';
        linkChild.stderr.on('data', d => { linkErr += d.toString('utf8'); });

        linkChild.on('close', code => {
            if (code === 0) {
                if (!isWindows) {
                    try { fs.chmodSync(outExe, 0o755); } catch (_) {}
                }
                resolve();
            } else {
                reject(new Error(`Link failed: ${linkErr || 'code ' + code}`));
            }
        });

        linkChild.on('error', reject);
    });

    onProgress({ phase: 'Compilation completed', pct: 100 });
    return {
        success: true,
        exePath: outExe,
        exeName,
    };
}

module.exports = {
    compileGame,
    isStateFile,
};
