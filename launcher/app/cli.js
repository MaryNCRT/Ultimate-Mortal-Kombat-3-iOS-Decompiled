/*
 * cli.js -- the game compiled from a .ipa without the launcher's window: the
 * same pipeline as COMPILE YOUR .IPA (lib/build.js), for a terminal, a
 * script or a machine without a desktop (CI).
 *
 *     ./build-game.sh  <your UMK3 1.2.59 .ipa> [--arch x64|x86] [--root DIR]
 *     build-game.cmd   <your .ipa> [--arch x64|x86]
 *
 * Those two run this file with the launcher's own Electron as Node
 * (ELECTRON_RUN_AS_NODE=1), so nothing else needs installing; from the
 * repository, `node launcher/app/cli.js <ipa> --root .` works as well.
 *
 * The game goes where the launcher puts it: the folder the launcher runs
 * from (the release's folder), or --root. That folder must be writable. The
 * exit code is the build's: 0 when the game is ready.
 */
const fs = require('fs');
const path = require('path');
const bld = require('./lib/build');
const ini = require('./lib/ini');

function usage(msg) {
    if (msg)
        console.error(msg);
    console.error('usage: build-game <your UMK3 1.2.59 .ipa> [--arch x64|x86] [--root DIR]');
    process.exit(2);
}

/* the release's folder: where the launcher keeps the game (main.js
 * getWritableExeDir), whatever the current directory is */
function defaultRoot() {
    if (process.env.UMK3_ROOT)
        return path.resolve(process.env.UMK3_ROOT);
    if (process.env.APPIMAGE)
        return path.dirname(process.env.APPIMAGE);
    const exeDir = path.dirname(process.execPath);
    /* Windows: launcher\bin\UMK3-Launcher.exe, the game two folders up */
    if (path.basename(exeDir).toLowerCase() === 'bin' && path.basename(path.dirname(exeDir)).toLowerCase() === 'launcher')
        return path.dirname(path.dirname(exeDir));
    if (/node(\.exe)?$/i.test(process.execPath))
        return process.cwd();
    return exeDir;
}

async function main() {
    const args = process.argv.slice(2);
    let ipa = null, root = null;
    let arch = process.platform === 'win32' ? 'x64' : 'x64';

    for (let i = 0; i < args.length; i++) {
        if (args[i] === '--arch') arch = args[++i];
        else if (args[i] === '--root') root = path.resolve(args[++i] || '.');
        else if (args[i] === '-h' || args[i] === '--help') usage();
        else if (!ipa) ipa = path.resolve(args[i]);
        else usage('one .ipa only');
    }
    if (!ipa) usage();
    if (!fs.existsSync(ipa)) usage('no such file: ' + ipa);
    if (arch !== 'x64' && arch !== 'x86') usage('--arch is x64 or x86');
    if (arch === 'x86' && process.platform !== 'win32') usage('the 32-bit build is Windows only');
    root = root || defaultRoot();
    try {
        fs.accessSync(root, fs.constants.W_OK);
    } catch {
        usage('cannot write to ' + root + ': the game is built there; give a writable folder with --root');
    }

    console.log('UMK3: building into ' + root);
    let last = -1;
    const code = await bld.build(root, ipa, arch, m => {
        if (!m) return;
        if (m.text) console.log(m.text);
        else if (typeof m.pct === 'number' && m.pct !== last) last = m.pct;
    });
    const ok = code === 0 && ini.gameReady(root, arch);
    console.log(ok ? 'UMK3: the game is ready in ' + root : 'UMK3: the build failed (' + code + ')');
    process.exit(ok ? 0 : 1);
}

main().catch(e => {
    console.error(e && e.stack || e);
    process.exit(1);
});
