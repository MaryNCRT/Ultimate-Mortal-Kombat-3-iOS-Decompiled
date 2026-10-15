/* umk3.ini reader/writer -- byte-for-byte the same format launcher.c wrote
 * and runtime/game_main.c reads:
 *
 *     width=960      height=640      fullscreen=0   borderless=0
 *     hide_controls=1   (no on-screen stick and buttons)
 *     antialiasing=0|2|4|8|16   aspect=3:2 (the game's 3:2, the only one so far)
 *     render_width=1920  render_height=1280   (the 3D resolution; absent =
 *       the window's size)
 *     language=ES    ui=EN           ipa=C:\x\UMK3.ipa
 *     debug_keys=1   skip_intro=1   key_up=87 ... key_moves=77   key_special=72
 *     marco=C:\x\picture.png   (the picture behind the bars in fullscreen)
 *     buttons=5|6    key5_p=85 key5_b=79 key5_k=74 key5_r=76
 *       (the layout, and the five-button layout's own keys; its S is
 *        key_special, the six-button layout's keys are key_hp .. key_run)
 *     pad=auto|off|xinput|054c:0ce6   (player one's pad: the first found,
 *       none, an Xbox pad, or one pad by USB vendor:product)
 *     pad_name=DualSense Wireless Controller   (the launcher's own label)
 *
 * The game only reads width/height/render_width/render_height/fullscreen/borderless/
 * language/debug_keys/skip_intro and the
 * key_<name> lines (see runtime/game_main.c:read_config); ui= and ipa= are
 * the launcher's own.  Missing keys keep the defaults, exactly like the C
 * launcher did.  Pure Node, no Electron -- tested in tests/launcher.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { StringDecoder } = require('string_decoder');

/* The resolutions the launcher offers, index = g_cfg_res in launcher.c.
 * The first seven are the game's native 3:2; the last five are 16:9 for the
 * widescreen mode (the engine letterboxes/extends around them). */
const RES = [
    { w: 480,  h: 320 },
    { w: 960,  h: 640 },
    { w: 1440, h: 960 },
    { w: 1920, h: 1280 },
    { w: 2400, h: 1600 },
    { w: 2880, h: 1920 },
    { w: 3840, h: 2560 },
    { w: 1280, h: 720 },
    { w: 1600, h: 900 },
    { w: 1920, h: 1080 },
    { w: 2560, h: 1440 },
    { w: 3840, h: 2160 },
];

/* The languages Info.plist maps (LANGUAGE_TEXT_xx); "" follows Windows. */
const LANGS = [
    { code: '',   name: null },
    { code: 'EN', name: 'English' },
    { code: 'ES', name: 'Español' },
    { code: 'FR', name: 'Français' },
    { code: 'DE', name: 'Deutsch' },
    { code: 'IT', name: 'Italiano' },
    { code: 'KO', name: '한국어' },
    { code: 'ZH', name: '中文' },
];

/* Player 1's keys, in the same order game_main.c binds them by name. */
const KEY_NAMES = [
    'up', 'down', 'left', 'right',
    'hp', 'lp', 'block', 'hk', 'lk', 'run',
    'pause', 'moves', 'special',
    /* debug mode's keys (debug_keys=1): the menu, its info line and pages,
     * the four fight keys and the three front-end screen keys */
    'dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next',
    'dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose',
    'dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu',
];
/* The five-button layout's own keys: punch, block, kick, run. */
const KEY5_NAMES = ['p', 'b', 'k', 'r'];
const KEY5_DEFAULTS = { p: 'U', b: 'O', k: 'J', r: 'L' };
const KEY_DEFAULTS = {
    up: 'W', down: 'S', left: 'A', right: 'D',
    hp: 'U', lp: 'I', block: 'O', hk: 'J', lk: 'K', run: 'L',
    pause: 'P', moves: 'M', special: 'H',
    /* virtual-key codes: F2 F3 Q E, F9..F12, F6 F7 F8 */
    dbg_menu: 113, dbg_info: 114, dbg_page_prev: 81, dbg_page_next: 69,
    dbg_ko_p2: 120, dbg_ko_p1: 121, dbg_win: 122, dbg_lose: 123,
    dbg_scr_prev: 117, dbg_scr_next: 118, dbg_scr_menu: 119,
};

function defaultConfig() {
    const keys = {};
    for (const name of KEY_NAMES)
        keys[name] = typeof KEY_DEFAULTS[name] === 'number'
            ? KEY_DEFAULTS[name] : KEY_DEFAULTS[name].charCodeAt(0);
    const keys5 = {};
    for (const name of KEY5_NAMES)
        keys5[name] = KEY5_DEFAULTS[name].charCodeAt(0);
    return {
        res: 1,                      /* 960x640: the window */
        rres: 0,                     /* the 3D resolution: 0 = the window's, else RES[rres - 1] */
        fullscreen: false,
        borderless: false,           /* fullscreen without a border, at the desktop's mode */
        antialiasing: 0,             /* 0, 2, 4, 8 or 16 samples */
        widescreen: false,           /* native: widen the 3D view, no stretch */
        debug: false,
        skipIntro: false,            /* skip_intro=1: no publisher logos */
        hideControls: false,         /* hide_controls=1: no on-screen stick and buttons */
        language: 0,                 /* "" = follow Windows */
        ui: 'ES',                    /* the launcher's own language */
        ipa: '',
        marco: '',                   /* fullscreen bars picture, optional */
        buttons: 5,                  /* the layout the game starts with (its factory default, Settings[4] = 5) */
        pad: 'auto',                 /* pad=: auto, off, xinput or vvvv:pppp */
        padName: '',                 /* pad_name=: how the launcher showed it */
        padDbgMenu: 'L3+R3',         /* pad_dbg_menu=: the pad buttons that open the debug menu */
        keys,
        keys5,
    };
}

/* Decode latin-* / mixed files the way a C launcher would have written them.
 * umk3.ini from the old launcher is UTF-8 without BOM; sql defaults. */
function readFileText(p) {
    const buf = fs.readFileSync(p);
    const dec = new StringDecoder('utf8');
    return dec.write(buf) + dec.end();
}

function load(p) {
    const cfg = defaultConfig();
    let text;
    try {
        text = readFileText(p);
    } catch (err) {
        if (err.code === 'ENOENT')
            return cfg;              /* missing file, keep defaults */
        throw err;
    }
    let w = 960, h = 640, rw = 0, rh = 0;
    for (const line of text.split(/\r?\n/)) {
        if (!line.includes('='))
            continue;
        const eq = line.indexOf('=');
        const key = line.slice(0, eq);
        let val = line.slice(eq + 1).trim();
        const num = parseInt(val, 10);
        if (key === 'width')        w = Number.isFinite(num) ? num : w;
        else if (key === 'height')  h = Number.isFinite(num) ? num : h;
        else if (key === 'render_width')  rw = Number.isFinite(num) ? num : 0;
        else if (key === 'render_height') rh = Number.isFinite(num) ? num : 0;
        else if (key === 'fullscreen') cfg.fullscreen = num === 1;
        else if (key === 'borderless') cfg.borderless = num === 1;
        else if (key === 'antialiasing') cfg.antialiasing = [2, 4, 8, 16].includes(num) ? num : 0;
        else if (key === 'widescreen') cfg.widescreen = num === 1;
        else if (key === 'debug_keys') cfg.debug = num === 1;
        else if (key === 'skip_intro') cfg.skipIntro = num === 1;
        else if (key === 'hide_controls') cfg.hideControls = num === 1;
        else if (key === 'language') {
            const i = LANGS.findIndex(l => l.code.toUpperCase() === val.toUpperCase());
            if (i >= 0)
                cfg.language = i;
        } else if (key === 'ui') {
            cfg.ui = val.toUpperCase() === 'EN' ? 'EN' : 'ES';
        } else if (key === 'ipa') {
            cfg.ipa = val;
        } else if (key === 'marco') {
            cfg.marco = val;
        } else if (key === 'pad') {
            cfg.pad = /^(auto|off|xinput|[0-9a-f]{4}:[0-9a-f]{4})$/i.test(val)
                ? val.toLowerCase() : 'auto';
        } else if (key === 'pad_dbg_menu') {
            cfg.padDbgMenu = /^([A-Z0-9]+(\+[A-Z0-9]+)*)?$/i.test(val) ? val.toUpperCase() : 'L3+R3';
        } else if (key === 'pad_name') {
            cfg.padName = val;
        } else if (key === 'buttons') {
            cfg.buttons = num === 6 ? 6 : 5;
        } else if (key.startsWith('key5_') && KEY5_NAMES.includes(key.slice(5))) {
            if (num > 0 && num < 256)
                cfg.keys5[key.slice(5)] = num;
        } else if (key.startsWith('key_') && KEY_NAMES.includes(key.slice(4))) {
            const vk = Number.isFinite(num) ? num : 0;
            if (vk > 0 && vk < 256)
                cfg.keys[key.slice(4)] = vk;
        }
    }
    cfg.res = RES.findIndex(r => r.w === w && r.h === h);
    if (cfg.res < 0)
        cfg.res = 1;
    cfg.rres = RES.findIndex(r => r.w === rw && r.h === rh) + 1;
    return cfg;
}

function save(p, cfg) {
    if (!cfg)
        cfg = defaultConfig();
    const res = RES[cfg.res] || RES[1];
    const lang = (LANGS[cfg.language] || LANGS[0]).code;
    const lines = [
        'width=' + res.w,
        'height=' + res.h,
        'fullscreen=' + (cfg.fullscreen ? 1 : 0),
        'borderless=' + (cfg.borderless ? 1 : 0),
        'antialiasing=' + ([2, 4, 8, 16].includes(cfg.antialiasing) ? cfg.antialiasing : 0),
        'aspect=3:2',
        'widescreen=' + (cfg.widescreen ? 1 : 0),
        'language=' + lang,
        'ui=' + (cfg.ui === 'EN' ? 'EN' : 'ES'),
        'ipa=' + cfg.ipa,
        'debug_keys=' + (cfg.debug ? 1 : 0),
        'skip_intro=' + (cfg.skipIntro ? 1 : 0),
        'hide_controls=' + (cfg.hideControls ? 1 : 0),
        'buttons=' + (cfg.buttons === 6 ? 6 : 5),
        'pad=' + (cfg.pad || 'auto'),
        'pad_dbg_menu=' + (cfg.padDbgMenu === undefined ? 'L3+R3' : cfg.padDbgMenu),
    ];
    if (cfg.padName)
        lines.push('pad_name=' + cfg.padName.replace(/[\r\n]/g, ' '));
    if (cfg.rres > 0 && RES[cfg.rres - 1])
        lines.push('render_width=' + RES[cfg.rres - 1].w,
                   'render_height=' + RES[cfg.rres - 1].h);
    if (cfg.marco)
        lines.push('marco=' + cfg.marco);
    for (const name of KEY_NAMES)
        lines.push('key_' + name + '=' + cfg.keys[name]);
    for (const name of KEY5_NAMES)
        lines.push('key5_' + name + '=' + cfg.keys5[name]);
    fs.writeFileSync(p, lines.join('\n') + '\n', 'utf8');
}

/* When the game was last compiled: the exe's time, or null. */
function gameBuiltAt(root) {
    try {
        return fs.statSync(path.join(root, 'umk3-game.exe')).mtimeMs;
    } catch (err) {
        return null;
    }
}

function gameReady(root) {
    const exe = path.join(root, 'umk3-game.exe');
    const plist = path.join(root, 'res', 'Info.plist');
    return fs.existsSync(exe) && fs.existsSync(plist);
}

module.exports = { RES, LANGS, KEY_NAMES, KEY_DEFAULTS, KEY5_NAMES, KEY5_DEFAULTS, defaultConfig,
                   load, save, gameReady, gameBuiltAt };