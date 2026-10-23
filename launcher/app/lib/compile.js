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

/* State files that hold the machine's side of the game -- the clock and the
 * sound table, the low heap, the network session: a save state keeps them,
 * a rollback of an online fight does not (src/runtime/savestate.h). */
const NO_ROLLBACK = ['lime_menu', 'lime_platform', 'lime_app', 'wav', 'limeMPGameSpecific'];

/**
 * ONLINE over the internet (src/runtime/mp_eos.c, Port, not in the binary):
 * the Epic Online Services SDK, which is not in the repository. Found through
 * UMK3_EOS_DIR, or a TOOLS/eos folder in the root or any folder above it.
 * Returns that folder, or null: then the game plays on the local network
 * alone.
 */
function findEosDir(rootDir) {
    const has = d => d && fs.existsSync(path.join(d, 'SDK', 'Include', 'eos_sdk.h'));
    if (has(process.env.UMK3_EOS_DIR)) return process.env.UMK3_EOS_DIR;
    let d = path.resolve(rootDir);
    for (;;) {
        // tools/eos too: a package made on Windows merges TOOLS/ into tools/
        for (const t of ['TOOLS', 'tools']) {
            const c = path.join(d, t, 'eos');
            if (has(c)) return c;
        }
        const up = path.dirname(d);
        if (up === d) return null;
        d = up;
    }
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
                reject(new Error(`Error compiling ${path.basename(rspPath, '.rsp')}: ${stderr || 'code ' + code}`));
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
        path.join(rootDir, 'src/runtime/mods.c'),
        path.join(rootDir, 'src/runtime/draw_gl.c'),
        path.join(rootDir, 'src/runtime/gamecode_stubs.c'),
        path.join(rootDir, 'src/runtime/mp_net.c'),
        path.join(rootDir, 'src/runtime/mp_eos.c'),
        path.join(rootDir, 'src/runtime/online_ui.c'),
        path.join(rootDir, 'src/runtime/lime_menu.c'),
        path.join(rootDir, 'src/runtime/lime_platform.c'),
        path.join(rootDir, 'src/runtime/lime_app.c'),
        path.join(rootDir, 'src/runtime/wav.c'),
        path.join(rootDir, 'src/runtime/fight_runtime.c'),
        path.join(rootDir, 'src/runtime/savestate.c'),
        path.join(rootDir, 'src/runtime/rollback_probe.c'),
        path.join(rootDir, 'src/runtime/rollback.c'),
    ];

    if (isWindows) {
        sources.push(
            path.join(rootDir, 'src/runtime/platform/win32_gl.c'),
            path.join(rootDir, 'src/runtime/platform/win32_audio.c'),
            path.join(rootDir, 'src/runtime/platform/ui_alert.c'),
            path.join(rootDir, 'src/runtime/render/rgl_d3d12.c')
        );
    } else {
        sources.push(
            path.join(rootDir, 'src/runtime/platform/sdl_gl.c'),
            path.join(rootDir, 'src/runtime/platform/sdl_audio.c'),
            path.join(rootDir, 'src/runtime/platform/ui_alert.c')
        );
    }
    /* what draws: every GL call through rgl_front.c, to OpenGL or to the
     * Vulkan / Direct3D 12 fixed-function layer (src/runtime/render) */
    sources.push(
        path.join(rootDir, 'src/runtime/render/rgl_front.c'),
        path.join(rootDir, 'src/runtime/render/rgl_ff.c'),
        path.join(rootDir, 'src/runtime/render/rgl_vk.c')
    );

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
        '-O2',
        '-fno-strict-aliasing',
        '-fno-sanitize=undefined',
        '-fwrapv',
        '-DUMK3_HAVE_MK3',
        '-DUMK3_REAL_GL',
        '-DUMK3_SHELL',
        '-I', path.join(rootDir, 'src/runtime'),
        '-I', path.join(rootDir, 'src/decomp/lime'),
        '-I', path.join(rootDir, 'src/decomp/gamecode/logic'),
        '-include', path.join(rootDir, 'src/runtime/savestate.h'),
        '-I', path.join(rootDir, 'src/runtime/platform/vulkan-include'),
    ];

    if (isWindows) {
        baseFlags.push(`-target`, isLow32 ? 'x86_64-windows-gnu' : 'x86-windows-gnu', '-fms-extensions');
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
        /* The OpenGL and SDL2 headers come with the game too
         * (src/runtime/platform/linux-include): a desktop without the
         * -dev packages has the libraries but not their headers. Searched
         * after the system's own, which win when they are there. */
        const inc = path.join(rootDir, 'src/runtime/platform/linux-include');
        baseFlags.push('-idirafter', inc, '-idirafter', path.join(inc, 'SDL2'));
    }

    // EOS: 64 bits only (the SDK has no 32-bit Windows or Linux library)
    const eosDir = isLow32 ? findEosDir(rootDir) : null;
    const eosLib = isWindows ? 'EOSSDK-Win64-Shipping.dll' : 'libEOSSDK-Linux-Shipping.so';
    if (eosDir && fs.existsSync(path.join(eosDir, 'SDK', 'Bin', eosLib))) {
        baseFlags.push('-DUMK3_EOS', '-I', path.join(eosDir, 'SDK', 'Include'));
    }

    if (isLow32) {
        baseFlags.push(
            '-DUMK3_LOW32',
            '-fms-extensions',
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
            if (NO_ROLLBACK.includes(baseName)) {
                fileFlags.push('-DUMK3_NO_ROLLBACK');
            }
        }
        if (baseName === 'text') {
            fileFlags.push('-DUMK3_NO_LONG_REMAP');
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
            execSync(`"${zigExe}" rc -- "${rcPath}" "${resObj}"`, { stdio: 'pipe' });
        });
    }

    const totalTasks = tasks.length;
    let completedCount = 0;
    const concurrency = Math.max(1, os.cpus().length);

    onProgress({
        phase: `Compiling game code (0/${totalTasks})`,
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
            '-Wl,--subsystem,windows',
            '-lopengl32', '-lwinmm', '-lmsacm32', '-lgdi32', '-luser32', '-lkernel32', '-lws2_32'
        );
        if (isLow32) {
            linkArgs.push('-Wl,--image-base,0x400000', '-Wl,--no-dynamicbase');
        }
        // the 32-bit game is already large-address-aware (4 GB on 64-bit
        // Windows): zig's linker sets it and takes no flag for it
    } else {
        if (process.platform !== 'linux') {
            linkArgs.push(`-target`, isLow32 ? 'x86_64-linux-gnu' : 'x86-linux-gnu');
        }
        linkArgs.push('-no-pie');
        /* the 64-bit directories first: on Fedora and its kin /usr/lib
         * holds the 32-bit libraries (libGL.so.1 "incompatible with
         * elf64-x86-64"), the 64-bit ones are in /usr/lib64 */
        const libDirs = ['/usr/lib64', '/usr/lib/x86_64-linux-gnu', '/lib/x86_64-linux-gnu', '/lib64', '/usr/lib'];
        const want = process.platform === 'linux' || isLow32 ? 2 : 1;      /* ELF class: 2 = 64-bit */
        /* a library of the build's own width (a linker script, not ELF, is
         * taken as it is) */
        const fits = p => {
            try {
                const fd = fs.openSync(p, 'r'), h = Buffer.alloc(5);
                fs.readSync(fd, h, 0, 5, 0);
                fs.closeSync(fd);
                return h.readUInt32BE(0) !== 0x7f454c46 || h[4] === want;
            } catch { return false; }
        };
        for (const dir of libDirs) {
            if (fs.existsSync(dir)) {
                linkArgs.push(`-L${dir}`);
            }
        }
        /* libSDL2.so and libGL.so are the -dev packages' names for the
         * libraries; without them, the libraries themselves by their
         * versioned names, which every desktop has */
        const sysLib = (base, sonames) => {
            for (const dir of libDirs)
                if (fits(path.join(dir, `lib${base}.so`)))
                    return path.join(dir, `lib${base}.so`);
            for (const so of sonames)
                for (const dir of libDirs)
                    if (fits(path.join(dir, so)))
                        return path.join(dir, so);
            return `-l${base}`;
        };
        /* no libGL: render/rgl_front.c loads it when OpenGL draws, so a
         * machine without it can still run the game with Vulkan */
        linkArgs.push(sysLib('SDL2', ['libSDL2-2.0.so.0', 'libSDL2-2.0.so']), '-lm', '-ldl');
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
                if (isWindows) {
                    try {
                        const buf = fs.readFileSync(outExe);
                        if (buf.length >= 0x40 && buf.readUInt16LE(0) === 0x5A4D) {
                            const peOff = buf.readUInt32LE(0x3C);
                            if (peOff + 96 <= buf.length && buf.readUInt32LE(peOff) === 0x00004550) {
                                if (isLow32) {
                                    const dllCharOff = peOff + 94;
                                    const dllChar = buf.readUInt16LE(dllCharOff);
                                    buf.writeUInt16LE(dllChar & ~(0x0020 | 0x0040), dllCharOff);
                                } else {
                                    const charOff = peOff + 22;
                                    const ch = buf.readUInt16LE(charOff);
                                    buf.writeUInt16LE(ch | 0x0020, charOff);
                                }
                                fs.writeFileSync(outExe, buf);
                            }
                        }
                    } catch (_) {}
                } else {
                    try { fs.chmodSync(outExe, 0o755); } catch (_) {}
                }
                resolve();
            } else {
                reject(new Error(`Link failed: ${linkErr || 'code ' + code}`));
            }
        });

        linkChild.on('error', reject);
    });

    // The EOS library and the product's eos.ini beside the game
    if (baseFlags.includes('-DUMK3_EOS')) {
        fs.copyFileSync(path.join(eosDir, 'SDK', 'Bin', eosLib), path.join(destDir, eosLib));
        const ini = path.join(eosDir, 'eos.ini');
        if (fs.existsSync(ini)) {
            fs.copyFileSync(ini, path.join(destDir, 'eos.ini'));
        } else {
            onProgress({ phase: `EOS: no ${ini}; online on the local network only`, pct: 99 });
        }
    }

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
