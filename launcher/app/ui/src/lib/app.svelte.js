/* The launcher's state and what it does: the game-folder / compiler /
 * settings logic of the old launcher.c, driven by the contextBridge API
 * (window.umk3) -- nothing here touches Node or the filesystem directly.
 *
 *   - every change writes umk3.ini at once (config:save)
 *   - COMPILE runs the build pipeline and streams its log and progress
 *   - controls: click an action, press a key; Esc cancels
 *   - the menu works with the mouse, the keyboard (arrows, Enter, Esc) and
 *     a pad; F11 toggles fullscreen
 *
 * One $state object, `app`, holds everything the components draw; the
 * functions below change it. */
import { api, isWin } from './platform.js';
import { T } from './i18n.js';
import { PAD_SETS, padMake, livePad } from './glyphs.js';
import { hoverBlip, selectBlip, doneChord, failChord } from './sound.js';
import { ms, MODS_TABS } from './mods/store.svelte.js';
import { decodePvr } from './mods/formats.js';
import { altArt } from './profileIcon.js';
import { stagePictures } from './mods/stagepics.js';

/* ------------------------------------------------------------ tables -- */

/* the menu, in order; QUIT closes */
export const SECTIONS = ['build', 'play', 'video', 'controls', 'profile', 'mods', 'replays', 'quit'];
/* each section's name in the menu and its full-screen header */
export const TITLE = { build: 'mBuild', play: 'mPlay', video: 'mVideo', controls: 'mControls', profile: 'mProfile', mods: 'mMods', replays: 'mReplays', quit: 'mQuit' };
export const DESC = { build: 'dBuild', video: 'dVideo', controls: 'dControls', profile: 'dProfile', play: 'dPlay', mods: 'dMods', replays: 'dReplays', quit: 'dQuit' };
/* the pages of GRAPHICS and CONTROLS, in their second row */
export const GFX_TABS = [['video', 'gVideo'], ['tex', 'gTex'], ['ui', 'gUi']];
export const CTL_TABS = [['p', 'tabKb'], ['pad', 'tabPad'], ['dbg', 'tabDbg']];

/* index = res in umk3.ini (lib/ini.js): the game's own 3:2 sizes and 16:9 ones */
export const RES = [
    { w: 480, h: 320 }, { w: 960, h: 640 }, { w: 1440, h: 960 },
    { w: 1920, h: 1280 }, { w: 2400, h: 1600 }, { w: 2880, h: 1920 },
    { w: 3840, h: 2560 },
    { w: 1280, h: 720 }, { w: 1600, h: 900 }, { w: 1920, h: 1080 },
    { w: 2560, h: 1440 }, { w: 3840, h: 2160 },
    { w: 1024, h: 768 }, { w: 1600, h: 1200 }, { w: 2048, h: 1536 },
    { w: 2560, h: 1080 }, { w: 3440, h: 1440 },
];
/* aspect= in umk3.ini */
export const ASPECT = ['3:2', '4:3', '16:9', '21:9'];
/* texture_quality= in umk3.ini: IPHONE = the _LOW sheets the 480-wide phone
 * loads, IPAD = the full sheets, NO LIMIT = full sheets, PNG before PVRTC,
 * mipmaps and anisotropic filtering */
export const TEXQ = ['iphone', 'ipad', 'max'];
/* arch= in umk3.ini: which game COMPILE builds and PLAY opens, 32 bits
 * (umk3-game.exe) or 64 bits (umk3-game64.exe); both can be kept */
export const ARCH = isWin ? ['x86', 'x64'] : ['x64'];
/* ui_size= in umk3.ini: IPAD lays the game out on 960x640, where it draws
 * the HUD at 1.75 of 2 */
export const UISIZE = ['iphone', 'ipad'];
/* renderer=: what draws (src/runtime/render); Direct3D 12 is Windows' */
export const RENDERERS = isWin ? ['opengl', 'vulkan', 'd3d12'] : ['opengl', 'vulkan'];
/* antialiasing= in umk3.ini: the multisampling the game asks the card for */
export const AA = [0, 2, 4, 8, 16];
export const LANGS = [null, 'English', 'Español', 'Français', 'Deutsch', 'Italiano', '한국어', '中文'];

/* The keyboard's actions. "5:x" names a key of the five-button layout
 * (umk3.ini key5_x); the rest are key_<name>. The special button (S) exists
 * only in the five. */
export const KGROUPS = {
    move: () => ['up', 'down', 'left', 'right'],
    buttons: () => (app.cfg.buttons !== 6
        ? ['5:p', '5:b', '5:k', '5:r', 'special']
        : ['hp', 'lp', 'block', 'hk', 'lk', 'run']),
    system: () => (app.pl === 2 ? [] : ['pause', 'moves']),
    dbgmenu: () => ['dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next'],
    dbgfight: () => ['dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose'],
    dbgscr: () => ['dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu'],
};
const DBG_KEYS = ['dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next',
    'dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose',
    'dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu'];
const KEY_DEFAULT = {
    up: 87, down: 83, left: 65, right: 68,
    hp: 85, lp: 73, block: 79, hk: 74, lk: 75, run: 76,
    pause: 80, moves: 77, special: 72,
    dbg_menu: 113, dbg_info: 114, dbg_page_prev: 81, dbg_page_next: 69,
    dbg_ko_p2: 120, dbg_ko_p1: 121, dbg_win: 122, dbg_lose: 123,
    dbg_scr_prev: 117, dbg_scr_next: 118, dbg_scr_menu: 119,
};
const KEY5_DEFAULT = { p: 85, b: 79, k: 74, r: 76 };
/* Player two's defaults: the game's second set, arrows and the keypad. */
const P2_KEY_DEFAULT = {
    up: 38, down: 40, left: 37, right: 39,
    hp: 103, lp: 104, block: 105, hk: 100, lk: 101, run: 102, special: 96,
};
const P2_KEY5_DEFAULT = { p: 103, b: 105, k: 100, r: 102 };

/* The pad's buttons for each action (umk3.ini pad_<name>= / pad5_<name>=,
 * runtime/game_main.c): up to two buttons each, either one presses it. The
 * defaults are the game's own, by position. "5:x" is the five-button
 * layout's; special, pause and moves serve both layouts. */
export const PAD_ROWS = {
    6: ['hp', 'lp', 'hk', 'lk', 'block', 'run', 'special', 'pause', 'moves', 'settings'],
    5: ['5:p', '5:k', '5:b', '5:r', 'special', 'pause', 'moves', 'settings'],
};
const PAD_DEFAULT = {
    hp: 'WEST', lp: 'SOUTH', block: 'R1', hk: 'NORTH', lk: 'EAST', run: 'L1+L2',
    special: 'R2', pause: 'START', moves: 'SELECT', settings: 'HOME',
};
const PAD5_DEFAULT = { p: 'WEST+NORTH', b: 'R1', k: 'SOUTH+EAST', r: 'L1+L2' };

/* -------------------------------------------------------------- state -- */

export const app = $state({
    cfg: null,
    ui: 'EN',
    /* the game folder: compiled (the chosen build), when, either build at
     * all (none: the menu offers COMPILE alone), both (PLAY picks) */
    ready: false, builtAt: null, locked: false, both: false, version: '',
    sec: 'build',
    full: false,                        /* a section fills the window */
    gfxTab: 'video', ctlTab: 'p',
    pl: 1,                              /* the player KEYBOARD and GAMEPAD edit */
    waitingKey: null,                   /* the action waiting for its key */
    usingPad: false,                    /* the last input: the pad's pictures show */
    overlay: 0,                         /* windows over the page (REPLACE MODEL's preview): the command bar hides */
    glyphs: {},
    seenPads: {}, padLooking: false,
    monitors: [],
    status: { kind: '', text: '' },
    build: { on: false, show: false, pct: 0, fail: false, label: '', log: '', console: false },
    upd: { cls: '', text: '', canGet: false, busy: false, bar: false, pct: 0, label: '',
           notesLbl: '', notes: null, news: false, checking: false },
    prof: null, profResetArmed: false, artTick: 0,
    gameClosed: 0,                      /* counts the game closing (REPLAYS reads its list again) */
    logo: null, hero: null,
    picker: null,                       /* { x, y, kind, pick } */
});

/* The fighters' pictures for the profile icons: Image objects, kept out of
 * the state (app.artTick says when more arrived). */
export const profArt = [];
/* the second outfits' pictures (profileIcon.js altArt); none for Sonya */
export const profArtAlt = [];

export const tx = k => (T[app.ui][k] !== undefined ? T[app.ui][k] : k);

/* ------------------------------------------------------------- config -- */

export async function save(delta) {
    const next = await api.saveConfig($state.snapshot(delta));
    if (next) app.cfg = next;
}

export function setStatus(kind, text) {
    app.status = { kind, text: text || '' };
}

/* Whether the game is built, and so the status line. */
export function paintState() {
    if (app.build.on) setStatus('building', tx('statusBuilding'));
    else setStatus(app.ready ? 'ready' : 'need', tx(app.ready ? 'statusReady' : 'statusNeed'));
}

export async function refreshRoot() {
    const info = await api.root();
    app.ready = info.ready;
    app.builtAt = info.builtAt;
    app.locked = !info.any;
    app.both = !!info.both;
    if (info.version)                   /* 0.1.3-beta: "0.1.3 beta" */
        app.version = info.version.replace('-', ' ');
    paintState();
    /* the game is there: STAGE SELECT's pictures for the stages without one */
    if (app.ready && !app.build.on) setTimeout(stagePictures, 1500);
}

export function setLang(ui) {
    app.ui = ui;
    save({ ui });
    paintState();
}

/* ----------------------------------------------------------- sections -- */

/* The menu's line chosen; run opens it: QUIT closes, a section fills the
 * window (openFull). */
export function setSection(next, run) {
    if (app.locked && next !== 'build' && next !== 'quit') return;
    app.sec = next;
    if (next === 'quit') {
        if (run) api.close();
    } else if (run || app.full)
        app.full = true;
}

/* back from a section to the menu */
export function closeFull() {
    if (!app.full) return;
    app.full = false;
    app.picker = null;
    if (document.activeElement) document.activeElement.blur();
}

/* the triggers / Q E / the header's arrows: the next section, round */
export function stepSection(d) {
    const list = SECTIONS.filter(n => n !== 'quit' && !(app.locked && n !== 'build'));
    if (list.length < 2) return;
    hoverBlip();
    setSection(list[(list.indexOf(app.sec) + d + list.length) % list.length], true);
}

/* left / right / the second row's arrows: the section's next page */
export function stepPage(d) {
    const tabs = app.sec === 'video' ? GFX_TABS : app.sec === 'controls' ? CTL_TABS : app.sec === 'mods' ? MODS_TABS : null;
    if (!tabs) return;
    const cur = app.sec === 'video' ? app.gfxTab : app.sec === 'mods' ? ms.tab : app.ctlTab;
    const i = Math.max(0, tabs.findIndex(t => t[0] === cur));
    const next = tabs[(i + d + tabs.length) % tabs.length][0];
    selectBlip();
    if (app.sec === 'video') app.gfxTab = next;
    else if (app.sec === 'mods') ms.tab = next;
    else setCtlTab(next);
}

export function setCtlTab(t) {
    app.ctlTab = t;
    app.waitingKey = null;
    if (t === 'pad') {
        app.padLooking = true;
        setTimeout(scanPads, 0);
    }
}

/* --------------------------------------------------------------- keys -- */

const KEYS_OF = n => (app.pl === 2 && !DBG_KEYS.includes(n) ? 'p2Keys' : 'keys');

export function keyGet(name) {
    const cfg = app.cfg;
    if (name.startsWith('5:')) {
        const d = app.pl === 2 ? P2_KEY5_DEFAULT : KEY5_DEFAULT, k = app.pl === 2 ? 'p2Keys5' : 'keys5';
        return (cfg[k] && cfg[k][name.slice(2)]) || d[name.slice(2)];
    }
    const k = KEYS_OF(name);
    return (cfg[k] && cfg[k][name]) || (k === 'p2Keys' ? P2_KEY_DEFAULT : KEY_DEFAULT)[name];
}

function keySet(name, vk) {
    const k = name.startsWith('5:') ? (app.pl === 2 ? 'p2Keys5' : 'keys5') : KEYS_OF(name);
    const n = name.startsWith('5:') ? name.slice(2) : name;
    app.cfg[k] = Object.assign({}, app.cfg[k], { [n]: vk });
    save({ [k]: { [n]: vk } });
}

/* the player's keys only; the debug keys have their own reset */
export function resetKeys() {
    selectBlip();
    const cfg = app.cfg;
    if (app.pl === 2) {
        cfg.p2Keys = Object.assign({}, P2_KEY_DEFAULT);
        cfg.p2Keys5 = Object.assign({}, P2_KEY5_DEFAULT);
        save({ p2Keys: cfg.p2Keys, p2Keys5: cfg.p2Keys5 });
    } else {
        const d = Object.assign({}, KEY_DEFAULT);
        for (const n of DBG_KEYS) d[n] = keyGet(n);
        cfg.keys = d;
        cfg.keys5 = Object.assign({}, KEY5_DEFAULT);
        save({ keys: cfg.keys, keys5: cfg.keys5 });
    }
    app.waitingKey = null;
}

export function resetDbgKeys() {
    selectBlip();
    const d = {};
    for (const n of DBG_KEYS) d[n] = KEY_DEFAULT[n];
    app.cfg.keys = Object.assign({}, app.cfg.keys, d);
    save({ keys: d });
    app.waitingKey = null;
}

export function setButtons(b) {
    app.cfg.buttons = b;
    save({ buttons: b });
}

/* ---------------------------------------------------------------- pad -- */

const PK = () => (app.pl === 2 ? 'p2PadKeys' : 'padKeys');

export function padGet(act) {
    const cfg = app.cfg;
    if (act.startsWith('5:')) {
        const v = cfg[PK() + '5'] && cfg[PK() + '5'][act.slice(2)];
        return v === undefined ? PAD5_DEFAULT[act.slice(2)] : v;
    }
    const v = cfg[PK()] && cfg[PK()][act];
    return v === undefined ? PAD_DEFAULT[act] : v;
}

export function padSet(act, v) {
    selectBlip();
    const k = act.startsWith('5:') ? PK() + '5' : PK();
    const n = act.startsWith('5:') ? act.slice(2) : act;
    app.cfg[k] = Object.assign({}, app.cfg[k], { [n]: v });
    save({ [k]: { [n]: v } });
}

export function resetPad() {
    selectBlip();
    app.cfg[PK()] = Object.assign({}, PAD_DEFAULT);
    app.cfg[PK() + '5'] = Object.assign({}, PAD5_DEFAULT);
    save({ [PK()]: app.cfg[PK()], [PK() + '5']: app.cfg[PK() + '5'] });
}

export const padWant = () => (app.pl === 2 ? app.cfg.p2Pad : app.cfg.pad) || 'auto';

/* The pictures for a player's pad: pad_icons= / p2pad_icons=, else the
 * pad's own make (the chosen one, or for auto the one connected). */
export function padKindOf(p) {
    const cfg = app.cfg;
    const st = (p === 2 ? cfg.p2PadIcons : cfg.padIcons) || 'auto';
    if (st !== 'auto' && PAD_SETS[st]) return st;
    const dev = (p === 2 ? cfg.p2Pad : cfg.pad) || 'auto';
    if (/^[0-9a-f]{4}:/i.test(dev)) return padMake(dev);
    const gp = livePad();
    return gp ? padMake('', gp.id) : 'xbox';
}

export function choosePad(v) {
    const k = app.pl === 2 ? 'p2Pad' : 'pad', kn = app.pl === 2 ? 'p2PadName' : 'padName';
    app.cfg[k] = v;
    app.cfg[kn] = app.seenPads[v] || '';
    selectBlip();
    save({ [k]: v, [kn]: app.seenPads[v] || '' });
}

export function choosePadIcons(v) {
    const k = app.pl === 2 ? 'p2PadIcons' : 'padIcons';
    app.cfg[k] = v;
    selectBlip();
    save({ [k]: v });
}

export function setPadDbg(v) {
    app.cfg.padDbgMenu = v;
    save({ padDbgMenu: v });
}

/* The pads the system has attached, by the name it gives them, as pad=
 * values: "xinput" or the USB "vendor:product". They come from the game
 * itself (`umk3-game --list-pads`, the same search it plays with), asked
 * again every few seconds while the GAMEPAD tab is open. */
let padScanBusy = false;
export async function scanPads() {
    if (padScanBusy || app.sec !== 'controls' || app.ctlTab !== 'pad') return;
    padScanBusy = true;
    try {
        const now = {};
        for (const p of await api.listPads())
            if (!now[p.value]) now[p.value] = p.name;
        if (JSON.stringify(now) !== JSON.stringify(app.seenPads))
            app.seenPads = now;
    } finally {
        padScanBusy = false;
        app.padLooking = false;
    }
}

/* the button picker under a pad slot */
export function openPicker(rect, kind, pick) {
    app.picker = { rect: { left: rect.left, right: rect.right, top: rect.top, bottom: rect.bottom }, kind, pick };
}
export function closePicker() {
    app.picker = null;
}

/* The monitors the game sees (`umk3-game --list-monitors`); before the
 * game is compiled only AUTOMATIC. */
export async function fillMonitors() {
    try { app.monitors = await api.listMonitors(); } catch (err) { app.monitors = []; }
}

/* -------------------------------------------------------------- build -- */

function logLine(line) {
    let t = app.build.log + line + '\n';
    if (t.length > 20000) t = t.slice(-16000);
    app.build.log = t;
}

function showBuildUI(on) {
    app.build.on = on;
    app.build.show = on;
    if (on) {
        app.build.console = true;
        app.build.log = '';
        app.build.pct = 0;
        app.build.fail = false;
        app.build.label = '';
    }
    paintState();
}

export function handleBuildData(m) {
    if (m.pct !== undefined && m.pct !== null) app.build.pct = m.pct;
    if (m.phase) app.build.label = m.phase;
    if (m.text) logLine(m.text);
}

export async function startBuild() {
    if (app.build.on) return;
    const ipa = app.cfg.ipa;
    if (!ipa || !ipa.trim()) {
        setStatus('need', tx('needIpa'));
        failChord();
        return;
    }
    selectBlip();
    const wasLocked = app.locked;
    showBuildUI(true);
    const r = await api.startBuild(ipa.trim());
    showBuildUI(false);
    app.build.show = true;
    await refreshRoot();
    app.build.pct = 100;
    if (r.ok) {
        logLine('OK.');
        setStatus('ready', tx('buildDone'));
        doneChord();
        if (wasLocked)          /* the first game: open again with the whole menu */
            setTimeout(() => api.relaunch(), 1500);
    } else {
        app.build.fail = true;
        logLine(r.code === -3 ? 'ERROR: ' + tx('notIpa') : 'EXIT CODE: ' + r.code);
        setStatus('need', r.code === -3 ? tx('notIpa') : tx('buildFail'));
        failChord();
    }
}

export async function browseIpa() {
    const p = await api.browseIpa();
    if (p) await save({ ipa: p });
}

/* ------------------------------------------------------------ updates -- */

let rel = null;

function paintUpdate(r) {
    const u = app.upd;
    u.canGet = false;
    if (!r) { u.cls = ''; u.text = tx('updLooking'); return; }
    if (!r.ok || !r.rel) {
        u.cls = 'need';
        u.text = tx('updFail') + (r.error ? ' (' + r.error + ')' : '') + ' — ' + tx('updYours') + ' ' + r.current;
        return;
    }
    rel = r.rel;
    u.notesLbl = rel.name;
    u.notes = rel.notes || '';
    if (rel.newer) {
        u.cls = 'need';
        u.text = tx('updNew') + ': ' + rel.tag + ' (' + (rel.size / 1048576).toFixed(0) + ' MB) — ' + tx('updYours') + ' ' + r.current;
        u.canGet = true;
        u.news = true;
    } else {
        u.cls = 'ok';
        u.text = tx('updNone') + ' (' + r.current + ')';
    }
}

export async function checkUpdate() {
    if (app.upd.busy) return;
    app.upd.checking = true;
    paintUpdate(null);
    paintUpdate(await api.checkUpdate());
    app.upd.checking = false;
}

export async function installUpdate() {
    if (app.upd.busy || app.build.on || !rel) return;
    selectBlip();
    app.upd.busy = true;
    app.upd.bar = true;
    app.upd.pct = 0;
    const r = await api.installUpdate();
    if (!r.ok) {
        app.upd.busy = false;
        app.upd.cls = 'need';
        app.upd.text = 'ERROR: ' + r.error;
        failChord();
    }
}

export function handleUpdateProgress(p) {
    if (p.phase === 'install') {
        app.upd.pct = 100;
        app.upd.label = tx('updInst');
        return;
    }
    app.upd.pct = p.total ? p.got * 100 / p.total : 0;
    app.upd.label = tx('updDl') + ' ' + (p.got / 1048576).toFixed(1) + ' / ' + (p.total / 1048576).toFixed(1) + ' MB';
}

/* Opened again by the updater: say so and compile the new sources. */
async function afterUpdate() {
    const r = await api.updateResult();
    if (!r) return false;
    setSection('build', false);
    if (!r.ok) {
        app.upd.cls = 'need';
        app.upd.text = tx('updFailed');
        return true;
    }
    app.upd.cls = 'ok';
    app.upd.text = tx('updDone') + ' ' + r.tag;
    app.upd.notesLbl = r.name || r.tag;
    app.upd.notes = r.notes || '';
    if (app.cfg.ipa && app.cfg.ipa.trim()) {
        setStatus('building', tx('updRebuild'));
        startBuild();
    }
    return true;
}

/* --------------------------------------------------------------- play -- */

export async function doPlay() {
    if (app.build.on || !app.ready) return;
    selectBlip();
    const r = await api.play();
    if (!r.ok) {
        setStatus('need', tx('playFail'));
        failChord();
        return;
    }
    setStatus('playing', tx('statusPlaying'));
}

/* ------------------------------------------------------------ profile -- */

/* profiles.ini (lib/profiles.js), shared with the game's settings menu.
 * One per installation; the id never changes. */
async function loadProfArt() {
    for (let i = 0; i < 24; i++) {
        if (profArt[i]) continue;
        const url = await api.profileIcon(i);
        if (!url) continue;
        const im = new Image();
        im.src = url;
        await im.decode().catch(() => {});
        profArt[i] = im;
    }
    for (let i = 0; i < 24; i++) {
        if (profArtAlt[i]) continue;
        const b = await api.profileIconAlt(i);
        const img = b && decodePvr(new Uint8Array(b));
        if (img) profArtAlt[i] = altArt(img);
    }
    app.artTick++;
}

export async function updateProf(fields) {
    app.prof = await api.updateProfile($state.snapshot(fields));
}

export async function refreshProfile() {
    app.prof = await api.loadProfile(tx('profDefault'));
    await loadProfArt();
}

export async function resetProf() {
    if (!app.profResetArmed) { app.profResetArmed = true; return; }
    app.profResetArmed = false;
    failChord();
    app.prof = await api.resetProfile(tx('profDefault'));
}

/* ----------------------------------------------------------- keyboard -- */

function onKey(e) {
    if (app.waitingKey) {                   /* binding a key */
        e.preventDefault();
        e.stopPropagation();
        const w = app.waitingKey;
        app.waitingKey = null;
        if (e.keyCode !== 27 && e.keyCode < 256) {
            keySet(w, e.keyCode);
            selectBlip();
        }
        return;
    }
    if (e.key === 'F11') {
        e.preventDefault();
        api.fullscreen();
        return;
    }
    const tag = (document.activeElement && document.activeElement.tagName) || '';
    if (tag === 'SELECT' || tag === 'INPUT') return;
    if (navKey(e.key))
        e.preventDefault();
}

/* The menu's keys, from the keyboard or the pad: true when one acted. */
function navKey(key) {
    const idle = !document.activeElement || document.activeElement === document.body;
    if (app.full) {
        if (key === 'Escape') { selectBlip(); closeFull(); return true; }
        if (key === 'SecPrev' || key === 'q' || key === 'Q') { stepSection(-1); return true; }
        if (key === 'SecNext' || key === 'e' || key === 'E') { stepSection(1); return true; }
        if ((key === 'ArrowLeft' || key === 'ArrowRight') && idle) {
            stepPage(key === 'ArrowLeft' ? -1 : 1);
            return true;
        }
        return false;
    }
    const vis = SECTIONS.filter(n => !(app.locked && n !== 'build'));
    const i = Math.max(0, vis.indexOf(app.sec));
    if (key === 'ArrowDown' || key === 'ArrowUp') {
        if (vis.length < 2) return true;
        hoverBlip();
        setSection(vis[(i + (key === 'ArrowDown' ? 1 : vis.length - 1)) % vis.length], false);
        return true;
    }
    if (key === 'Enter' && idle) {
        selectBlip();
        setSection(app.sec, true);
        return true;
    }
    if (key === 'Escape') {
        closePicker();
        setSection('build', false);
        return true;
    }
    return false;
}

/* ---------------------------------------------------------- the pad, here
 * The launcher follows a pad too: up / down the menu, south to pick, east
 * back, the triggers or bumpers through the sections, left / right through
 * a section's pages. The command bar shows the pictures of whichever was
 * used last, the pad's or the keyboard's -- not both. */
let padWas = [];

function padTick() {
    const g = livePad();
    if (g) {
        const b = g.buttons.map(x => x.pressed), ay = g.axes[1] || 0, ax = g.axes[0] || 0;
        b[12] = b[12] || ay < -0.6;
        b[13] = b[13] || ay > 0.6;
        b[14] = b[14] || ax < -0.6;
        b[15] = b[15] || ax > 0.6;
        const hit = i => b[i] && !padWas[i];
        if (b.some(Boolean)) app.usingPad = true;
        if (!app.picker && !app.waitingKey) {
            if (hit(12)) navKey('ArrowUp');
            if (hit(13)) navKey('ArrowDown');
            if (hit(0)) navKey('Enter');
            if (hit(1)) navKey('Escape');
            if (hit(14)) navKey('ArrowLeft');
            if (hit(15)) navKey('ArrowRight');
            if (hit(4) || hit(6)) navKey('SecPrev');
            if (hit(5) || hit(7)) navKey('SecNext');
        }
        padWas = b;
    } else if (app.usingPad)
        app.usingPad = false;
    requestAnimationFrame(padTick);
}

/* --------------------------------------------------------------- init -- */

export async function init() {
    const cfg = await api.loadConfig();
    app.ui = cfg.ui === 'ES' ? 'ES' : 'EN';
    app.cfg = cfg;
    await refreshRoot();
    app.glyphs = (await api.glyphs()) || {};
    requestAnimationFrame(padTick);
    refreshProfile();
    fillMonitors();

    /* The game's settings menu (Esc) writes umk3.ini and profiles.ini too:
     * read them again whenever the launcher comes back to the front. */
    window.addEventListener('focus', async () => {
        if (app.waitingKey || app.build.on)
            return;
        app.cfg = await api.loadConfig();
        if (document.activeElement?.id !== 'profName')
            refreshProfile();
    });
    window.addEventListener('keydown', onKey, true);
    window.addEventListener('keydown', () => { app.usingPad = false; }, true);
    window.addEventListener('mousemove', () => { app.usingPad = false; });
    window.addEventListener('click', closePicker);
    /* pads come and go: asked again every three seconds on the GAMEPAD tab */
    setInterval(scanPads, 3000);
    api.onBuildData(handleBuildData);
    api.onUpdateProgress(handleUpdateProgress);
    /* the game closed: the launcher is back where it was */
    api.onGameClosed(async () => {
        await refreshRoot();
        paintState();
        app.gameClosed++;
    });

    app.logo = await api.logo();
    app.hero = await api.hero();

    /* UMK3_LAUNCHER_SEC=<section>[:<controls tab>], the capture aid */
    const [want, sub] = (new URLSearchParams(location.search).get('sec') || '').split(':');
    const open = SECTIONS.includes(want) && want !== 'quit';
    setSection(open ? want : app.ready ? 'play' : 'build', open);
    if (sub && MODS_TABS.some(t => t[0] === sub))
        ms.tab = sub;
    else if (sub && CTL_TABS.some(t => t[0] === sub))
        setCtlTab(sub);
    else if (sub && GFX_TABS.some(t => t[0] === sub))
        app.gfxTab = sub;

    /* an update just installed is compiled first; else ask GitHub, without
     * holding up the window */
    if (!(await afterUpdate()))
        setTimeout(checkUpdate, 800);

    /* development aid: UMK3_LAUNCHER_SHOT=<png> captures the window (the
     * MODS viewer, given something to open, captures once it is open) */
    if (!new URLSearchParams(location.search).get('pick'))
        setTimeout(() => api.shot(), 600);
}
