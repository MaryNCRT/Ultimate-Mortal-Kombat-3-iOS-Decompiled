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

const { app, BrowserWindow, ipcMain, dialog, nativeImage, net, Notification } = require('electron');
const path = require('path');
const fs = require('fs');
const ini = require('./lib/ini');
const rootm = require('./lib/root');
const bld = require('./lib/build');
const upd = require('./lib/update');
const prof = require('./lib/profiles');

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
    /* UMK3_LAUNCHER_SEC=<section>: open on that page (with the shot aid) */
    g_win.loadFile(path.join(__dirname, 'renderer', 'index.html'),
                   { query: { sec: process.env.UMK3_LAUNCHER_SEC || '' } });
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
       both: process.platform === 'win32' && ini.gameReady(g_root, 'x86') && ini.gameReady(g_root, 'x64') }));

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
async function pickFile(opts) {
    const full = g_win.isFullScreen();
    if (full)
        g_win.setFullScreen(false);
    g_win.focus();
    try {
        return await dialog.showOpenDialog(g_win, opts);
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

/* The key and button pictures (Julio Cacko's Free Input Prompts, CC0), the
 * same the game's menus draw: "aditional textures/glyphs/<set>/<file>.png"
 * beside the game, or the launcher's own copy before the first compile.
 * Every one at once, as data: URLs by "<set>/<file>". */
ipcMain.handle('glyphs:all', () => {
    const dirs = [path.join(g_root || '', 'aditional textures', 'glyphs'),
                  path.join(g_root || '', 'launcher', 'aditional textures', 'glyphs'),
                  path.join(__dirname, '..', 'aditional textures', 'glyphs')];
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

ipcMain.handle('play', () => {
    if (!ini.gameReady(g_root, arch()))
        return { ok: false };
    bld.play(g_root, ini.gameExe(arch()));
    /* the game runs on its own (detached): the launcher steps aside, once
     * the answer has reached the page */
    setTimeout(() => app.quit(), 800);
    return { ok: true };
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