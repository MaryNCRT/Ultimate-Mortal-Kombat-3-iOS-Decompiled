/* The launcher as a non-Steam game in the player's Steam library, and the
 * artwork Steam shows for it.
 *
 * Where Steam keeps things (checked on the project machine, 2026-11-12,
 * against a non-Steam shortcut Steam was showing with its artwork):
 *
 *   <Steam>/userdata/<account id>/config/shortcuts.vdf   the non-Steam games
 *       (binary VDF, lib/vdf.js); one per Steam account that logged in here.
 *   <Steam>/userdata/<account id>/config/grid/           the custom artwork,
 *       named by the shortcut's 32-bit app id:
 *           <appid>p.<ext>      the library's portrait capsule (600x900)
 *           <appid>.<ext>       the wide capsule (920x430 / 460x215)
 *           <appid>_hero.<ext>  the banner behind the game's page
 *           <appid>_logo.<ext>  the logo drawn over the hero
 *           <appid>_icon.<ext>  the icon -- Steam reads the icon from the
 *                               shortcut's own "icon" field, so that field
 *                               is pointed at this file
 *       .png and .jpg are what Steam reads; the SteamGridDB browser asks for
 *       those only (lib/sgdb.js), so nothing is converted.
 *   The app id: CRC-32 of the shortcut's Exe (with its quotes, as stored)
 *   followed by its AppName, with the top bit set -- the value Steam
 *   stores in the shortcut's "appid" field.  An existing shortcut's own
 *   "appid" is always used as it is.
 *
 *   Windows: the Steam folder from HKCU\Software\Valve\Steam SteamPath.
 *   Linux: ~/.steam/steam, ~/.local/share/Steam, the Flatpak's
 *   ~/.var/app/com.valvesoftware.Steam/.local/share/Steam and the Snap's
 *   ~/snap/steam/common/.local/share/Steam, the same folder counted once.
 *
 * shortcuts.vdf is only written while Steam is closed (Steam keeps its own
 * copy in memory and writes it back on exit, undoing any change), after a
 * backup beside it, through a temporary file renamed over the old one; every
 * other shortcut in it is written back byte for byte.
 */
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { execFile } = require('child_process');
const vdf = require('./vdf');

const NAME = 'Ultimate Mortal Kombat 3 IOs Decomp';
const STEAMID64_BASE = 76561197960265728n;
const KEEP_BACKUPS = 5;

/* the artwork slots: the selection's name -> the grid folder's suffix */
const SLOTS = { grid: 'p', wide: '', hero: '_hero', logo: '_logo', icon: '_icon' };
const EXTS = ['.png', '.jpg', '.jpeg', '.webp', '.ico'];

/* ------------------------------------------------------------ crc32 ---- */
let CRC_TABLE = null;
function crc32(buf) {
    if (!CRC_TABLE) {
        CRC_TABLE = new Uint32Array(256);
        for (let i = 0; i < 256; i++) {
            let c = i;
            for (let k = 0; k < 8; k++)
                c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
            CRC_TABLE[i] = c >>> 0;
        }
    }
    let c = 0xFFFFFFFF;
    for (const b of buf)
        c = CRC_TABLE[(c ^ b) & 0xFF] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
}

function shortcutAppId(exeField, appName) {
    return (crc32(Buffer.from(exeField + appName, 'utf8')) | 0x80000000) >>> 0;
}

/* ------------------------------------------------------- installs ---- */
function regQuery(key, value) {
    return new Promise(resolve => {
        execFile('reg', ['query', key, '/v', value], { windowsHide: true, timeout: 5000 }, (err, out) => {
            if (err) return resolve(null);
            const m = String(out).match(new RegExp(value + '\\s+REG_\\w+\\s+(.+)', 'i'));
            resolve(m ? m[1].trim() : null);
        });
    });
}

function realDir(p) {
    try {
        return fs.statSync(p).isDirectory() ? fs.realpathSync(p) : null;
    } catch { return null; }
}

/* every Steam folder on this machine that has a userdata\ */
async function findInstalls(opts = {}) {
    const platform = opts.platform || process.platform;
    const home = opts.home || os.homedir();
    const cands = [];
    if (opts.candidates)
        cands.push(...opts.candidates);
    else if (platform === 'win32') {
        const hkcu = await regQuery('HKCU\\Software\\Valve\\Steam', 'SteamPath');
        if (hkcu) cands.push({ path: hkcu, kind: 'windows' });
        const hklm = await regQuery('HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam', 'InstallPath');
        if (hklm) cands.push({ path: hklm, kind: 'windows' });
        cands.push({ path: 'C:\\Program Files (x86)\\Steam', kind: 'windows' });
    } else {
        const flat = path.join(home, '.var', 'app', 'com.valvesoftware.Steam');
        cands.push(
            { path: path.join(home, '.steam', 'steam'), kind: 'native' },
            { path: path.join(home, '.steam', 'root'), kind: 'native' },
            { path: path.join(home, '.local', 'share', 'Steam'), kind: 'native' },
            { path: path.join(flat, '.local', 'share', 'Steam'), kind: 'flatpak' },
            { path: path.join(flat, 'data', 'Steam'), kind: 'flatpak' },
            { path: path.join(home, 'snap', 'steam', 'common', '.local', 'share', 'Steam'), kind: 'snap' });
    }
    const seen = new Set(), out = [];
    for (const c of cands) {
        const real = realDir(path.normalize(c.path));
        if (!real || !realDir(path.join(real, 'userdata')))
            continue;
        const k = platform === 'win32' ? real.toLowerCase() : real;
        if (seen.has(k)) continue;
        seen.add(k);
        out.push({ path: real, kind: c.kind });
    }
    return out;
}

/* the Steam accounts that logged in on this install: userdata\<id> folders,
 * named from config\loginusers.vdf */
function accounts(install) {
    const names = {};
    try {
        const lu = vdf.parseText(fs.readFileSync(path.join(install.path, 'config', 'loginusers.vdf'), 'utf8'));
        const users = lu.users || lu.Users || {};
        for (const [sid, u] of Object.entries(users)) {
            try {
                const id = String(BigInt(sid) - STEAMID64_BASE);
                names[id] = { persona: u.PersonaName || '', account: u.AccountName || '', mostRecent: u.MostRecent === '1' };
            } catch { /* not a SteamID64 */ }
        }
    } catch { /* no loginusers.vdf: ids only */ }
    let dirs = [];
    try { dirs = fs.readdirSync(path.join(install.path, 'userdata')); } catch { }
    return dirs.filter(d => /^\d+$/.test(d) && d !== '0').map(id => {
        const cfg = path.join(install.path, 'userdata', id, 'config');
        return {
            id, install: install.path, kind: install.kind,
            persona: (names[id] && names[id].persona) || '',
            mostRecent: !!(names[id] && names[id].mostRecent),
            shortcuts: path.join(cfg, 'shortcuts.vdf'),
            grid: path.join(cfg, 'grid'),
        };
    });
}

/* ---------------------------------------------------- Steam running ---- */
function isSteamRunning(opts = {}) {
    if (typeof opts.running === 'boolean')
        return Promise.resolve(opts.running);
    const platform = opts.platform || process.platform;
    if (platform === 'win32') {
        return new Promise(resolve => {
            execFile('tasklist', ['/FI', 'IMAGENAME eq steam.exe', '/NH', '/FO', 'CSV'],
                { windowsHide: true, timeout: 8000 }, (err, out) => {
                    /* tasklist failing: say running, so nothing is written blind */
                    if (err) return resolve(true);
                    resolve(/"steam\.exe"/i.test(String(out)));
                });
        });
    }
    /* Linux: the processes' names; a Flatpak's Steam shows in the host's /proc too */
    try {
        for (const pid of fs.readdirSync('/proc')) {
            if (!/^\d+$/.test(pid)) continue;
            let comm = '';
            try { comm = fs.readFileSync(`/proc/${pid}/comm`, 'utf8').trim(); } catch { continue; }
            if (comm === 'steam' || comm === 'steam.sh')
                return Promise.resolve(true);
        }
        return Promise.resolve(false);
    } catch {
        return Promise.resolve(true);
    }
}

/* ------------------------------------------------------ the launcher ---- */
/* What Steam starts: the launcher's own executable in the game folder (the
 * small UMK3-Launcher.exe / UMK3-Launcher that starts launcher\bin), or the
 * AppImage when the launcher runs as one. */
function launcherTarget(root, opts = {}) {
    const platform = opts.platform || process.platform;
    const env = opts.env || process.env;
    let exe = null;
    if (platform !== 'win32' && env.APPIMAGE && fs.existsSync(env.APPIMAGE))
        exe = env.APPIMAGE;
    else if (root) {
        const p = path.join(root, platform === 'win32' ? 'UMK3-Launcher.exe' : 'UMK3-Launcher');
        if (fs.existsSync(p)) exe = p;
    }
    if (!exe) return null;
    const dir = path.dirname(exe);
    return { exe, startDir: dir, exeField: `"${exe}"`, startDirField: `"${dir}"` };
}

/* ----------------------------------------------------- shortcuts.vdf ---- */
function readShortcuts(file) {
    if (!fs.existsSync(file))
        return [{ type: vdf.T.MAP, key: 'shortcuts', value: [] }];
    const root = vdf.parseBinary(fs.readFileSync(file));
    if (!vdf.get(root, 'shortcuts'))
        root.push({ type: vdf.T.MAP, key: 'shortcuts', value: [] });
    return root;
}

function entries(root) {
    return vdf.get(root, 'shortcuts').value;
}

const unq = s => String(s || '').trim().replace(/^"(.*)"$/, '$1');
function samePath(a, b, platform) {
    if (!a || !b) return false;
    const na = path.resolve(unq(a)), nb = path.resolve(unq(b));
    return (platform || process.platform) === 'win32' ? na.toLowerCase() === nb.toLowerCase() : na === nb;
}

/* ours: our name, or our executable under any name */
function isOurs(entry, target, platform) {
    const name = String(vdf.val(entry, 'AppName', '')).trim().toLowerCase();
    if (name === NAME.toLowerCase())
        return true;
    return !!(target && samePath(vdf.val(entry, 'Exe', ''), target.exe, platform));
}

function findOurs(root, target, platform) {
    return entries(root).filter(n => n.type === vdf.T.MAP && isOurs(n.value, target, platform));
}

function appIdOf(entry) {
    const n = vdf.get(entry, 'appid');
    if (n && n.type === vdf.T.INT && n.value) return n.value >>> 0;
    return shortcutAppId(String(vdf.val(entry, 'Exe', '')), String(vdf.val(entry, 'AppName', '')));
}

function stamp() {
    const d = new Date(), z = n => String(n).padStart(2, '0');
    return `${d.getFullYear()}${z(d.getMonth() + 1)}${z(d.getDate())}-${z(d.getHours())}${z(d.getMinutes())}${z(d.getSeconds())}`;
}

/* a copy of the old file beside it (the last few kept), then the new one
 * through a temporary file; the result read back and compared */
function writeShortcuts(file, root) {
    const bytes = vdf.serializeBinary(root);
    vdf.parseBinary(bytes);                 /* never write what cannot be read back */
    fs.mkdirSync(path.dirname(file), { recursive: true });
    let backup = null;
    if (fs.existsSync(file)) {
        backup = `${file}.umk3-${stamp()}.bak`;
        fs.copyFileSync(file, backup);
        const dir = path.dirname(file), base = path.basename(file) + '.umk3-';
        const old = fs.readdirSync(dir).filter(f => f.startsWith(base) && f.endsWith('.bak')).sort();
        for (const f of old.slice(0, Math.max(0, old.length - KEEP_BACKUPS)))
            try { fs.unlinkSync(path.join(dir, f)); } catch { }
    }
    const tmp = `${file}.umk3-tmp`;
    const fd = fs.openSync(tmp, 'w');
    try { fs.writeSync(fd, bytes); fs.fsyncSync(fd); } finally { fs.closeSync(fd); }
    fs.renameSync(tmp, file);
    if (!fs.readFileSync(file).equals(bytes))
        throw new Error('shortcuts.vdf did not read back as written');
    return backup;
}

function newEntry(target, appid) {
    const S = vdf.T.STR, I = vdf.T.INT;
    return [
        { type: I, key: 'appid', value: appid },
        { type: S, key: 'AppName', value: NAME },
        { type: S, key: 'Exe', value: target.exeField },
        { type: S, key: 'StartDir', value: target.startDirField },
        { type: S, key: 'icon', value: '' },
        { type: S, key: 'ShortcutPath', value: '' },
        { type: S, key: 'LaunchOptions', value: '' },
        { type: I, key: 'IsHidden', value: 0 },
        { type: I, key: 'AllowDesktopConfig', value: 1 },
        { type: I, key: 'AllowOverlay', value: 1 },
        { type: I, key: 'OpenVR', value: 0 },
        { type: I, key: 'Devkit', value: 0 },
        { type: S, key: 'DevkitGameID', value: '' },
        { type: I, key: 'DevkitOverrideAppID', value: 0 },
        { type: I, key: 'LastPlayTime', value: 0 },
        { type: S, key: 'FlatpakAppID', value: '' },
        { type: S, key: 'sortas', value: '' },
        { type: vdf.T.MAP, key: 'tags', value: [] },
    ];
}

function reindex(root) {
    entries(root).forEach((n, i) => { n.key = String(i); });
}

/* The shortcut for this account: added, or brought up to date (its Exe and
 * StartDir to where the launcher is now; its own app id kept, so its artwork
 * stays), the other copies of it removed.  iconPath: the grid's icon, or
 * null to leave the icon field as it is. */
function upsertShortcut(account, target, iconPath, opts = {}) {
    const root = readShortcuts(account.shortcuts);
    const list = entries(root);
    const ours = findOurs(root, target, opts.platform);
    let entry, appid, added = false;
    if (ours.length) {
        entry = ours[0].value;
        appid = appIdOf(entry);
        vdf.set(entry, 'appid', vdf.T.INT, appid);
        vdf.set(entry, 'AppName', vdf.T.STR, NAME);
        vdf.set(entry, 'Exe', vdf.T.STR, target.exeField);
        vdf.set(entry, 'StartDir', vdf.T.STR, target.startDirField);
        for (const dup of ours.slice(1))
            list.splice(list.indexOf(dup), 1);
    } else {
        appid = shortcutAppId(target.exeField, NAME);
        entry = newEntry(target, appid);
        list.push({ type: vdf.T.MAP, key: String(list.length), value: entry });
        added = true;
    }
    if (iconPath !== null && iconPath !== undefined)
        vdf.set(entry, 'icon', vdf.T.STR, iconPath);
    reindex(root);
    const backup = writeShortcuts(account.shortcuts, root);
    return { appid, added, removedDuplicates: Math.max(0, ours.length - 1), backup };
}

function removeShortcut(account, target, opts = {}) {
    if (!fs.existsSync(account.shortcuts))
        return { removed: 0, appids: [] };
    const root = readShortcuts(account.shortcuts);
    const list = entries(root);
    const ours = findOurs(root, target, opts.platform);
    if (!ours.length)
        return { removed: 0, appids: [] };
    const appids = ours.map(n => appIdOf(n.value));
    for (const n of ours)
        list.splice(list.indexOf(n), 1);
    reindex(root);
    const backup = writeShortcuts(account.shortcuts, root);
    let art = [];
    if (opts.removeArtwork !== false)
        for (const id of appids) art = art.concat(clearArtwork(account, id));
    return { removed: ours.length, appids, backup, artworkRemoved: art };
}

/* this account's shortcut, as Steam has it, or null */
function shortcutOf(account, target, opts = {}) {
    try {
        const root = readShortcuts(account.shortcuts);
        const ours = findOurs(root, target, opts.platform);
        if (!ours.length) return { shortcut: null };
        const e = ours[0].value;
        return {
            shortcut: {
                appid: appIdOf(e),
                name: vdf.val(e, 'AppName', ''),
                exe: unq(vdf.val(e, 'Exe', '')),
                icon: vdf.val(e, 'icon', ''),
                stale: !!(target && !samePath(vdf.val(e, 'Exe', ''), target.exe, opts.platform)),
                copies: ours.length,
            },
        };
    } catch (e) {
        return { shortcut: null, error: 'shortcuts.vdf: ' + e.message };
    }
}

/* -------------------------------------------------------- artwork ---- */
function sha1(file) {
    return crypto.createHash('sha1').update(fs.readFileSync(file)).digest('hex');
}

function slotFiles(account, appid, slot) {
    const stem = String(appid) + SLOTS[slot];
    return EXTS.map(e => path.join(account.grid, stem + e));
}

/* the files for this app id in the grid folder (ours: named by our id) */
function clearArtwork(account, appid) {
    const gone = [];
    for (const slot of Object.keys(SLOTS))
        for (const f of slotFiles(account, appid, slot))
            if (fs.existsSync(f)) { fs.unlinkSync(f); gone.push(path.basename(f)); }
    return gone;
}

/* Copies the chosen files into the grid folder under the app id (copies:
 * the originals stay in data\steam-artwork), each checked by its hash after.
 * files: { slot: absolute path | null }; a null slot's old copy is removed.
 * Returns what was copied and the icon's path in the grid folder. */
function applyArtwork(account, appid, files) {
    fs.mkdirSync(account.grid, { recursive: true });
    const done = { copied: [], removed: [], verified: true, icon: null };
    for (const slot of Object.keys(SLOTS)) {
        const src = files[slot];
        const stem = String(appid) + SLOTS[slot];
        const ext = src ? path.extname(src).toLowerCase().replace('.jpeg', '.jpg') : null;
        const dst = src ? path.join(account.grid, stem + ext) : null;
        /* the same slot under another extension would be read instead */
        for (const f of slotFiles(account, appid, slot))
            if (f !== dst && fs.existsSync(f)) { fs.unlinkSync(f); done.removed.push(path.basename(f)); }
        if (!src) continue;
        const tmp = dst + '.umk3-tmp';
        fs.copyFileSync(src, tmp);
        fs.renameSync(tmp, dst);
        const ok = sha1(src) === sha1(dst);
        if (!ok) done.verified = false;
        done.copied.push({ slot, file: path.basename(dst), ok });
        if (slot === 'icon') done.icon = dst;
    }
    return done;
}

/* what is in the grid folder for the app id, slot by slot */
function artworkInSteam(account, appid) {
    const out = {};
    for (const slot of Object.keys(SLOTS)) {
        const f = slotFiles(account, appid, slot).find(p => fs.existsSync(p));
        out[slot] = f ? path.basename(f) : null;
    }
    return out;
}

module.exports = {
    NAME, SLOTS, crc32, shortcutAppId, findInstalls, accounts, isSteamRunning, launcherTarget,
    readShortcuts, writeShortcuts, findOurs, upsertShortcut, removeShortcut, shortcutOf,
    applyArtwork, clearArtwork, artworkInSteam, appIdOf,
};
