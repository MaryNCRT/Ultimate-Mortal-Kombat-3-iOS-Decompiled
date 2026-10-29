/* UMK3-Launcher (Electron) main process.
 *
 * A compact, frameless arcade window that can be resized freely (corner-grip
 * scale, or maximize).  Any existing umk3.ini beside the game is read at
 * start and every change is written to it immediately, exactly like the old
 * launcher.c.  The renderer is a plain HTML/CSS/JS page; it talks to this
 * process over contextBridge (no nodeIntegration).
 *
 * Packaged unpacked (npm run dist -> dist/win-unpacked), shipped in
 * launcher\bin\ behind the stub UMK3-Launcher.exe (launcher/stub/).
 */
'use strict';

const { app, BrowserWindow, ipcMain, dialog, nativeImage, net, Notification, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const ini = require('./lib/ini');
const rootm = require('./lib/root');
const bld = require('./lib/build');
const upd = require('./lib/update');
const prof = require('./lib/profiles');
const mods = require('./lib/mods');
const replays = require('./lib/replays');

let g_win = null;
let g_root = null;
let g_cfg = null;

/* bring up the packaged build's errors to the surface (UMK3_LAUNCHER_SHOT;
 * not UMK3_SHOT, which the game itself reads and would inherit); the
 * portable wrapper detaches, so nothing would otherwise reach the console. */
function dbg(msg) {
    const where = process.env.UMK3_LAUNCHER_SHOT;
    if (!where)
        return;
    try {
        require('fs').appendFileSync(where.replace(/\.png[^.]*$/i, '.main.log'),
            new Date().toISOString() + ' ' + msg + '\n');
    } catch (err) { /* ignore */ }
}

process.on('uncaughtException', err => {
    dbg('uncaughtException: ' + (err && err.stack || err));
});
process.on('unhandledRejection', err => {
    dbg('unhandledRejection: ' + (err && err.stack || err));
});

function getWritableExeDir() {
    if (process.env.APPIMAGE)
        return path.dirname(process.env.APPIMAGE);
    if (process.env.PORTABLE_EXECUTABLE_DIR)
        return process.env.PORTABLE_EXECUTABLE_DIR;
    return path.dirname(process.execPath);
}

function findRoot() {
    if (process.env.UMK3_ROOT)
        return path.resolve(process.env.UMK3_ROOT);
    if (app.isPackaged)
        return getWritableExeDir();
    const exeDir = getWritableExeDir();
    return rootm.findRoot([
        process.cwd(),
        exeDir,
    ]);
}

/* Started without the stub (the bin exe directly, a shortcut), its location
 * says nothing certain about where the game lives; when the heuristics above
 * still fail -- e.g. launched from a shortcut or the search box -- remember
 * the folder chosen / found once, in the roaming profile. */
function savedRoot() {
    const f = path.join(app.getPath('appData'), 'umk3-launcher', 'root.json');
    try {
        const r = JSON.parse(fs.readFileSync(f, 'utf8'));
        return r && typeof r.root === 'string' && r.root ? r.root : null;
    } catch (err) {
        return null;
    }
}

function saveRoot(root) {
    const f = path.join(app.getPath('appData'), 'umk3-launcher', 'root.json');
    try {
        fs.mkdirSync(path.dirname(f), { recursive: true });
        fs.writeFileSync(f, JSON.stringify({ root }));
    } catch (err) { /* non-fatal */ }
}

async function resolveRoot() {
    let r = findRoot();
    if (!r && savedRoot() && rootm.looksLikeRoot(savedRoot()))
        r = savedRoot();
    if (!r) {
        const picked = await dialog.showOpenDialog({
            title: 'UMK3 LAUNCHER: Select game folder',
            properties: ['openDirectory'],
            message: 'Select the folder containing the game (the folder with src or umk3-game).',
        });
        r = picked.canceled ? null : picked.filePaths[0];
        if (r && !rootm.looksLikeRoot(r)) {
            dialog.showMessageBox({
                type: 'error',
                message: 'That folder does not look like a game installation (missing src folder or game executable).',
            });
            r = null;
        }
    }
    if (r)
        saveRoot(r);
    return r;
}

function send(channel, data) {
    if (g_win && !g_win.isDestroyed())
        g_win.webContents.send(channel, data);
}

function createWindow() {
    g_win = new BrowserWindow({
        /* 4:3 by default; the page scales with the window (styles.css --u),
         * any size or fullscreen (F11). UMK3_LAUNCHER_SIZE=WxH: dev aid. */
        width: Number((process.env.UMK3_LAUNCHER_SIZE || '').split('x')[0]) || 1280,
        height: Number((process.env.UMK3_LAUNCHER_SIZE || '').split('x')[1]) || 768,
        minWidth: 640,
        minHeight: 480,
        useContentSize: true,
        resizable: true,
        maximizable: true,
        fullscreenable: true,
        frame: false,
        backgroundColor: '#050505',
        icon: path.join(__dirname, 'build', 'icon.ico'),
        show: false,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
        },
    });
    g_win.setMenu(null);
    /* The window and taskbar icon: icon.png in the game folder if the player
     * put one there, else the launcher's own, build/icon.ico. */
    for (const f of ['icon.png']) {
        const p = g_root ? path.join(g_root, f) : null;
        if (p && fs.existsSync(p)) {
            g_win.setIcon(nativeImage.createFromPath(p));
            break;
        }
    }
    g_win.webContents.on('did-fail-load', (_e, code, desc) =>
        dbg('did-fail-load ' + code + ' ' + desc));
    g_win.webContents.on('render-process-gone', (_e, d) =>
        dbg('render-process-gone ' + d.reason));
    g_win.webContents.on('console-message', (_e, level, message) => {
        if (level >= 2)
            dbg('console<' + level + '>: ' + message);
    });
    /* UMK3_LAUNCHER_SEC=<section>: open on that page (with the shot aid);
     * UMK3_LAUNCHER_PICK=<kind>/<id>[@<clip>]: the MODS viewer opens that */
    g_win.loadFile(path.join(__dirname, 'renderer', 'index.html'),
                   { query: { sec: process.env.UMK3_LAUNCHER_SEC || '', pick: process.env.UMK3_LAUNCHER_PICK || '' } });
    g_win.webContents.once('did-finish-load', () => dbg('did-finish-load'));
    g_win.once('ready-to-show', () => g_win.show());
    g_win.on('closed', () => { g_win = null; });
}

/* ------------------------------------------------------------- ipc ---- */

/* The title logo ships inside the launcher (renderer/logo.png), so it is
 * there the moment the window opens; logo.png in the game folder, if the
 * player put one there, replaces it. */
ipcMain.handle('logo:get', () => {
    try {
        const p = path.join(g_root, 'logo.png');
        return 'data:image/png;base64,' + fs.readFileSync(p).toString('base64');
    } catch (err) {
        return null;
    }
});

/* The picture inside the home screen's small ring, as the console's main
 * menu shows the game: "aditional textures/image.png" -- the game folder's
 * (the compile copies it there, and the player may replace it), else the
 * one the release carries under launcher/ (Linux: in resources/), so it is
 * there before the first compile. None: an empty ring. */
ipcMain.handle('hero:get', () => {
    const dirs = [path.join(g_root, 'aditional textures'),
                  path.join(g_root, 'launcher', 'aditional textures')];
    if (process.resourcesPath)
        dirs.push(path.join(process.resourcesPath, 'launcher', 'aditional textures'));
    dirs.push(path.join(__dirname, '..', 'aditional textures'));
    for (const d of dirs) {
        for (const n of ['image.png', 'image.jpg']) {
            try {
                return 'data:image/' + (n.endsWith('png') ? 'png' : 'jpeg') + ';base64,'
                       + fs.readFileSync(path.join(d, n)).toString('base64');
            } catch (err) { /* the next */ }
        }
    }
    return null;
});

/* arch= in umk3.ini: which exe is built and played (x86 or x64). */
function arch() {
    return (g_cfg || ini.load(path.join(g_root, 'umk3.ini'))).arch;
}

/* The exe asked for its pads and monitors: the chosen one, else the other. */
function anyExe() {
    for (const a of [arch(), arch() === 'x64' ? 'x86' : 'x64']) {
        const exe = path.join(g_root, ini.gameExe(a));
        if (fs.existsSync(exe))
            return exe;
    }
    return null;
}

ipcMain.handle('root:get', () =>
    ({ root: g_root, ready: ini.gameReady(g_root, arch()),
       builtAt: ini.gameBuiltAt(g_root, arch()), version: app.getVersion(),
       /* both compiled: JUGAR offers the choice (Windows only) */
       both: process.platform === 'win32' && ini.gameReady(g_root, 'x86') && ini.gameReady(g_root, 'x64'),
       /* none compiled yet: the menu shows COMPILE alone */
       any: ini.gameReady(g_root, 'x64') || (process.platform === 'win32' && ini.gameReady(g_root, 'x86')) }));

/* The first compile done: the launcher opens again with the whole menu. */
ipcMain.handle('app:relaunch', () => {
    app.relaunch();
    app.exit(0);
});

ipcMain.handle('config:load', () => {
    g_cfg = ini.load(path.join(g_root, 'umk3.ini'));
    return JSON.parse(JSON.stringify(g_cfg));
});

ipcMain.handle('config:save', (_ev, delta) => {
    if (!g_cfg || typeof delta !== 'object')
        return g_cfg;
    g_cfg = ini.load(path.join(g_root, 'umk3.ini'));
    /* a delta names only the keys that changed: merge them, never replace
     * the whole set (that wrote "undefined" for every other key) */
    for (const [k, v] of Object.entries(delta)) {
        if (['keys', 'keys5', 'padKeys', 'padKeys5', 'p2Keys', 'p2Keys5',
             'p2PadKeys', 'p2PadKeys5'].includes(k)) {
            if (v && typeof v === 'object')
                Object.assign(g_cfg[k], v);
        } else {
            g_cfg[k] = v;
        }
    }
    ini.save(path.join(g_root, 'umk3.ini'), g_cfg);
    return JSON.parse(JSON.stringify(g_cfg));
});

ipcMain.handle('window:close', () => {
    if (g_win)
        g_win.close();
    return true;
});

ipcMain.handle('window:minimize', () => {
    if (g_win)
        g_win.minimize();
    return true;
});

ipcMain.handle('window:maximize', () => {
    if (!g_win)
        return false;
    if (g_win.isMaximized())
        g_win.unmaximize();
    else
        g_win.maximize();
    return g_win.isMaximized();
});

ipcMain.handle('window:fullscreen', () => {
    if (!g_win)
        return false;
    g_win.setFullScreen(!g_win.isFullScreen());
    return g_win.isFullScreen();
});

/* The file picker. On Windows a dialog owned by a fullscreen window opens
 * BEHIND it, so BUSCAR looked dead in fullscreen: leave fullscreen while the
 * picker is up and come back after, and bring it to the front. */
async function pickFile(opts, save = false) {
    const full = g_win.isFullScreen();
    if (full)
        g_win.setFullScreen(false);
    g_win.focus();
    try {
        return save ? await dialog.showSaveDialog(g_win, opts) : await dialog.showOpenDialog(g_win, opts);
    } finally {
        if (full)
            g_win.setFullScreen(true);
    }
}

ipcMain.handle('ipa:browse', async () => {
    if (!g_win)
        return null;
    const r = await pickFile({
        title: 'Choose your UMK3 .ipa',
        properties: ['openFile'],
        filters: [
            { name: 'UMK3 .ipa', extensions: ['ipa'] },
            { name: 'All files', extensions: ['*'] },
        ],
        defaultPath: g_cfg && g_cfg.ipa && fs.existsSync(path.dirname(g_cfg.ipa))
            ? g_cfg.ipa : undefined,
    });
    if (r.canceled || r.filePaths.length === 0)
        return null;
    return r.filePaths[0];
});

/* The picture behind the bars in fullscreen: any PNG the player owns. */
ipcMain.handle('frame:browse', async () => {
    if (!g_win)
        return null;
    const r = await pickFile({
        title: 'Frame picture',
        properties: ['openFile'],
        filters: [{ name: 'PNG', extensions: ['png'] }],
    });
    if (r.canceled || r.filePaths.length === 0)
        return null;
    return r.filePaths[0];
});

ipcMain.handle('build:start', async (_ev, ipa) => {
    if (!ipa || typeof ipa !== 'string')
        return { ok: false, code: -2 };
    if (!fs.existsSync(ipa))
        return { ok: false, code: -3 };
    const code = await bld.build(g_root, ipa, arch(), msg => send('build:data', msg));
    const ok = code === 0 && ini.gameReady(g_root, arch());
    return { ok, code };
});

ipcMain.handle('build:cancel', () => bld.cancelBuild());

/* ---------------------------------------------------------- updates ---- */

/* The newest release on GitHub and its notes (lib/update.js). The page asks
 * once when it opens and again on CHECK FOR UPDATES; a newer one also
 * gets a Windows notification, once per launch. */
let g_rel = null, g_notified = false;

ipcMain.handle('update:check', async () => {
    try {
        g_rel = await upd.check(net.fetch, app.getVersion());
    } catch (err) {
        return { ok: false, error: err.message, current: app.getVersion() };
    }
    if (g_rel && g_rel.newer && !g_notified && Notification.isSupported()) {
        g_notified = true;
        new Notification({
            title: 'UMK3 — Update available',
            body: g_rel.name + ' (' + g_rel.tag + '). Open it in COMPILE YOUR .IPA.',
        }).show();
    }
    return { ok: true, rel: g_rel, current: app.getVersion() };
});

/* Download, unpack, hand over to update/apply.ps1 and quit: the script copies
 * the files once this launcher is closed and starts it again. */
ipcMain.handle('update:install', async () => {
    if (!g_rel || !g_rel.newer)
        return { ok: false, error: 'no newer version available' };
    try {
        const zip = await upd.download(net.fetch, g_rel, path.join(g_root, 'update'),
            (got, total) => send('update:progress', { got, total }));
        send('update:progress', { got: 1, total: 1, phase: 'install' });
        upd.install(g_root, zip, g_rel);
    } catch (err) {
        return { ok: false, error: err.message };
    }
    setTimeout(() => app.quit(), 400);
    return { ok: true };
});

/* After apply.ps1 restarted us: what it installed, once. */
let g_updResult;
ipcMain.handle('update:result', () => {
    if (g_updResult === undefined)
        g_updResult = upd.takeResult(g_root);
    const r = g_updResult;
    g_updResult = null;
    return r;
});

/* The pads Windows has attached, by name: `umk3-game --list-pads` prints one
 * "pad=value<TAB>name" a line (the game's own search, WinMM and XInput).
 * Empty before the game is compiled. */
ipcMain.handle('pads:list', () => new Promise(resolve => {
    const exe = anyExe();
    if (!exe)
        return resolve([]);
    require('child_process').execFile(exe, ['--list-pads'],
        { cwd: g_root, timeout: 10000, windowsHide: true }, (err, out) => {
            const pads = [];
            for (const line of String(out || '').split(/\r?\n/)) {
                const tab = line.indexOf('\t');
                const value = line.slice(0, tab).trim().toLowerCase();
                if (tab > 0 && /^(xinput|[0-9a-f]{4}:[0-9a-f]{4})$/.test(value))
                    pads.push({ value, name: line.slice(tab + 1).trim() });
            }
            resolve(pads);
        });
}));

/* The monitors, as the game counts them: `umk3-game --list-monitors`
 * prints "index<TAB>name" a line (monitor= in umk3.ini). */
ipcMain.handle('monitors:list', () => new Promise(resolve => {
    const exe = anyExe();
    if (!exe)
        return resolve([]);
    require('child_process').execFile(exe, ['--list-monitors'],
        { cwd: g_root, timeout: 10000, windowsHide: true }, (err, out) => {
            const mons = [];
            for (const line of String(out || '').split(/\r?\n/)) {
                const m = line.match(/^(\d+)\t(.+)$/);
                if (m)
                    mons.push({ value: Number(m[1]), name: m[2].trim() });
            }
            resolve(mons);
        });
}));

/* The player's profile (lib/profiles.js): profiles.ini beside the game,
 * which the game's settings menu edits too. One per installation; its id is
 * given the first time and never changes. */
ipcMain.handle('profile:load', (_ev, name) => prof.load(g_root, name));
ipcMain.handle('profile:update', (_ev, fields) => prof.update(g_root, fields || {}));
ipcMain.handle('profile:reset', (_ev, name) => prof.reset(g_root, name));
ipcMain.handle('profile:icon', (_ev, i) => prof.iconData(g_root, Number(i)));
ipcMain.handle('profile:iconAlt', (_ev, i) => prof.iconAltData(g_root, Number(i)));

/* The key and button pictures (Julio Cacko's Free Input Prompts, CC0), the
 * same the game's menus draw: "aditional textures/glyphs/<set>/<file>.png"
 * beside the game, or the launcher's own copy before the first compile.
 * Every one at once, as data: URLs by "<set>/<file>". */
ipcMain.handle('glyphs:all', () => {
    const dirs = [path.join(g_root || '', 'aditional textures', 'glyphs'),
                  path.join(g_root || '', 'launcher', 'aditional textures', 'glyphs'),
                  process.resourcesPath ? path.join(process.resourcesPath, 'launcher', 'aditional textures', 'glyphs') : '',
                  path.join(__dirname, '..', 'aditional textures', 'glyphs')].filter(Boolean);
    const out = {};
    for (const d of dirs) {
        let sets;
        try { sets = fs.readdirSync(d); } catch (err) { continue; }
        for (const set of sets) {
            let files;
            try { files = fs.readdirSync(path.join(d, set)); } catch (err) { continue; }
            for (const f of files)
                if (f.toLowerCase().endsWith('.png') && !out[set + '/' + f.slice(0, -4)])
                    out[set + '/' + f.slice(0, -4)] = 'data:image/png;base64,' +
                        fs.readFileSync(path.join(d, set, f)).toString('base64');
        }
        if (Object.keys(out).length) break;
    }
    return out;
});

/* COMPILE SHADERS BEFORE PLAYING: `umk3-game --compile-shaders` opens the
 * renderer umk3.ini names, makes every pipeline it knows (the game's list and
 * what it met before, render/rgl.h), keeps them on disk for the game, and
 * closes; then the game starts. Nothing to do for OpenGL, which says so. */
function compileShaders(exe) {
    return new Promise(resolve => {
        require('child_process').execFile(exe, ['--compile-shaders'],
            { cwd: g_root, timeout: 120000 }, (_err, out) => resolve(String(out || '')));
    });
}

/* While the game runs the launcher steps aside -- its window hidden, once
 * the answer has reached the page -- and when the game closes it comes back
 * as it was, on the page it was on (a replay: REPLAYS). */
function stepAside(child) {
    if (!child || !g_win)
        return;
    const hide = setTimeout(() => g_win && g_win.hide(), 800);
    child.once('error', () => clearTimeout(hide));
    child.once('exit', () => {
        clearTimeout(hide);
        if (!g_win)
            return;
        g_win.show();
        g_win.focus();
        g_win.webContents.send('game:closed');
    });
}

ipcMain.handle('play', async () => {
    if (!ini.gameReady(g_root, arch()))
        return { ok: false };
    if (ini.load(path.join(g_root, 'umk3.ini')).compileShaders)
        await compileShaders(path.join(g_root, ini.gameExe(arch())));
    stepAside(bld.play(g_root, ini.gameExe(arch())));
    return { ok: true };
});

/* ------------------------------------------------------------ replays --
 * REPLAYS (lib/replays.js): the last fights the game kept, the ones kept for
 * good, and watching one (the game with --replay). */

const REPLAY_FILTER = [{ name: 'UMK3 replay', extensions: ['umkr'] }];

ipcMain.handle('replays:list', () => replays.list(g_root));
ipcMain.handle('replays:keep', (_ev, name) => replays.keep(g_root, name));
ipcMain.handle('replays:delete', (_ev, folder, name) => replays.remove(g_root, folder, name));
ipcMain.handle('replays:import', async () => {
    if (!g_win)
        return 0;
    const r = await pickFile({ title: 'Replays', properties: ['openFile', 'multiSelections'], filters: REPLAY_FILTER });
    return r.canceled ? 0 : replays.importFiles(g_root, r.filePaths);
});
ipcMain.handle('replays:export', async (_ev, folder, name) => {
    const from = replays.filePath(g_root, folder, name);
    if (!g_win || !from || !fs.existsSync(from))
        return false;
    const r = await pickFile({ title: 'Export replay', defaultPath: name, filters: REPLAY_FILTER }, true);
    if (r.canceled || !r.filePath)
        return false;
    fs.copyFileSync(from, r.filePath);
    return true;
});
ipcMain.handle('replays:open', () => {
    const p = replays.dir(g_root, 'saved');
    fs.mkdirSync(p, { recursive: true });
    shell.openPath(p);
    return true;
});
ipcMain.handle('replays:play', (_ev, folder, name) => {
    if (!ini.gameReady(g_root, arch()) || !replays.filePath(g_root, folder, name))
        return { ok: false };
    stepAside(bld.play(g_root, ini.gameExe(arch()), ['--replay', `replays/${folder}/${name}`]));
    return { ok: true };
});

/* --------------------------------------------------------------- mods --
 * The MODS section (lib/mods.js): the mod list and its order, the game's
 * files for the viewer, the exports. */

ipcMain.handle('mods:list', () => mods.list(g_root));
ipcMain.handle('mods:save', (_ev, order) => mods.save(g_root, order));
ipcMain.handle('mods:create', (_ev, fields) => mods.create(g_root, fields));
ipcMain.handle('mods:info', (_ev, folder, fields) => mods.setInfo(g_root, folder, fields));
ipcMain.handle('mods:put', (_ev, folder, file, bytes) => mods.putFile(g_root, folder, file, bytes));
ipcMain.handle('mods:add', async (_ev, folder) => {
    if (!g_win)
        return 0;
    const r = await pickFile({
        title: 'Files for the mod',
        properties: ['openFile', 'multiSelections'],
        filters: [
            { name: 'Game files', extensions: ['png', 'pvr', 'wav', 'mp3', 'meshset', 'skin', 'bones', 'skinanim', 'scene', 'events'] },
            { name: 'All files', extensions: ['*'] },
        ],
    });
    if (r.canceled)
        return 0;
    return mods.addFiles(g_root, folder, r.filePaths);
});
ipcMain.handle('mods:open', (_ev, folder) => {
    const p = mods.modPath(g_root, folder);
    if (!p)
        return false;
    fs.mkdirSync(p, { recursive: true });
    shell.openPath(p);
    return true;
});
/* to the recycle bin, not deleted: the player can get it back */
ipcMain.handle('mods:trash', async (_ev, folder) => {
    const p = folder && mods.modPath(g_root, folder);
    if (!p || !fs.existsSync(p))
        return false;
    await shell.trashItem(p);
    return true;
});
/* the cover, as the PNG the window made of the picture picked */
ipcMain.handle('mods:cover', (_ev, folder, png) => mods.setCover(g_root, folder, png));
ipcMain.handle('mods:dataRead', (_ev, folder) => mods.dataRead(g_root, folder));
ipcMain.handle('mods:dataWrite', (_ev, folder, entries) => mods.dataWrite(g_root, folder, entries));
ipcMain.handle('res:files', () => mods.resFiles(g_root));
ipcMain.handle('res:read', (_ev, rel, layer) => mods.resRead(g_root, rel, layer));
ipcMain.handle('game:tables', () => mods.gameTables(g_root));
/* a model or an animation from Blender, for the viewer to bring in */
const PICTURES = ['png', 'jpg', 'jpeg', 'jfif', 'webp', 'gif', 'bmp', 'ico', 'avif', 'svg', 'tga'];

/* A model's textures and .mtl, from its own folder and a textures folder
 * beside it, for the importer to find by name: { name: bytes }, 40 files
 * and 128 MB at most. */
function besideModel(file) {
    const out = {};
    let n = 0, size = 0;
    const dir = path.dirname(file);
    for (const sub of ['', 'textures', 'Textures', 'tex', 'images']) {
        let names = [];
        try { names = fs.readdirSync(path.join(dir, sub)); } catch (_) { continue; }
        for (const nm of names) {
            const ext = path.extname(nm).slice(1).toLowerCase();
            if (ext !== 'mtl' && !PICTURES.includes(ext))
                continue;
            const p = path.join(dir, sub, nm);
            let st;
            try { st = fs.statSync(p); } catch (_) { continue; }
            if (!st.isFile() || n >= 40 || size + st.size > 128 << 20 || out[nm])
                continue;
            out[nm] = new Uint8Array(fs.readFileSync(p));
            n++;
            size += st.size;
        }
    }
    return out;
}

ipcMain.handle('file:pick', async (_ev, kind) => {
    const r = g_win && await pickFile({
        title: kind === 'picture' ? 'Picture' : kind === 'model' ? 'Model' : 'FBX from Blender',
        properties: ['openFile'],
        /* pictures: whatever the window decodes, and TGA (ui/src/lib/mods/image.js) */
        filters: kind === 'picture'
            ? [{ name: 'Pictures', extensions: PICTURES }, { name: 'All files', extensions: ['*'] }]
            : kind === 'model'
                ? [{ name: 'Models (FBX, GLB, OBJ)', extensions: ['fbx', 'glb', 'obj'] }]
                : [{ name: 'FBX', extensions: ['fbx'] }],
    });
    if (!r || r.canceled)
        return null;
    const file = r.filePaths[0];
    const got = { name: path.basename(file), bytes: new Uint8Array(fs.readFileSync(file)) };
    if (kind === 'model')
        got.beside = besideModel(file);
    return got;
});
ipcMain.handle('export:save', (_ev, sub, files) => mods.exportSave(g_root, sub, files));
ipcMain.handle('export:open', (_ev, sub) => {
    const p = path.join(g_root, 'exports', sub && /^[\w .()'-]+(\/[\w .()'-]+)*$/.test(sub) && !sub.includes('..') ? sub : '');
    fs.mkdirSync(p, { recursive: true });
    shell.openPath(p);
    return true;
});

/* ------------------------------------------------------------- splash -- */

/* A development aid: UMK3_LAUNCHER_SHOT=<png> starts, waits for the renderer's
 * "shot:ready" handshake, captures a PNG and exits.  Used to eyeball the
 * design without a real .ipa. */
ipcMain.handle('shot:capture', async () => {
    const where = process.env.UMK3_LAUNCHER_SHOT;
    if (!where || !g_win)
        return false;
    try {
        await new Promise(r => setTimeout(r, 1200));
        const img = await g_win.webContents.capturePage();
        require('fs').writeFileSync(where, img.toPNG());
        dbg('captured ok');
        app.quit();
        return true;
    } catch (err) {
        dbg('capture failed: ' + err.message);
        app.quit();
        return false;
    }
});

/* Development aid together with UMK3_SHOT: dumps the renderer's geometry so
 * the layout can be validated without looking at the PNG. */
ipcMain.handle('debug:layout', (_ev, payload) => {
    const where = process.env.UMK3_LAUNCHER_SHOT;
    if (!where || !payload)
        return false;
    const json = where.replace(/\.png[^.]*$/i, '.json');
    require('fs').writeFileSync(json, JSON.stringify(payload, null, 2));
    return true;
});

/* -------------------------------------------------------------- app ---- */

/* the capture aid (UMK3_LAUNCHER_SHOT) runs beside a launcher already open */
const gotLock = process.env.UMK3_LAUNCHER_SHOT ? true : app.requestSingleInstanceLock();
if (!gotLock) {
    app.quit();
} else {
    app.on('second-instance', () => {
        if (g_win) {
            g_win.show();
            if (g_win.isMinimized())
                g_win.restore();
            g_win.focus();
        }
    });
    app.whenReady().then(async () => {
        g_root = await resolveRoot();
        if (!g_root) {
            app.quit();
            return;                     /* nothing below may run without a root */
        }
        g_cfg = ini.load(path.join(g_root, 'umk3.ini'));
        dbg('ready root=' + g_root);
        createWindow();
        app.on('activate', () => {
            if (BrowserWindow.getAllWindows().length === 0)
                createWindow();
        });
    });
    app.on('window-all-closed', () => {
        if (process.platform !== 'darwin')
            app.quit();
    });
}