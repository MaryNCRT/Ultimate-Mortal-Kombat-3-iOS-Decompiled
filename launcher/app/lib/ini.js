/* umk3.ini reader/writer -- byte-for-byte the same format launcher.c wrote
 * and runtime/game_main.c reads:
 *
 *     width=960      height=640      fullscreen=0   borderless=0
 *     hide_controls=1   (no on-screen stick and buttons)
 *     texture_quality=iphone|ipad|max
 *     ui_size=iphone|ipad   (ipad: the game lays out on 960x640, a smaller HUD)
 *     antialiasing=0|2|4|8|16   aspect=3:2|4:3|16:9|21:9 (3:2 the iPhone's, 4:3 the iPad's;
 *       wider: the fight fills it, the menus stay 3:2)
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
 *     pad_hp=WEST ... pad_moves=SELECT, pad5_p=WEST+NORTH ...   (the pad's
 *       button for each action, PB_* names joined by "+", empty = none)
 *     p2pad= p2pad_name= p2pad_<action>= p2pad5_<x>= p2key_<name>=
 *       p2key5_<x>=   (player two's, for DUELO; same values)
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
    { w: 1024, h: 768 },
    { w: 1600, h: 1200 },
    { w: 2048, h: 1536 },
    { w: 2560, h: 1080 },
    { w: 3440, h: 1440 },
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

/* The pad's button for each action (pad_<name>= and pad5_<name>=): the
 * game's PB_* names joined by "+" (any one presses it), "" for none.
 * The defaults are the game's own, by position (runtime/game_main.c). */
const PAD_NAMES = ['hp', 'lp', 'block', 'hk', 'lk', 'run', 'special', 'pause', 'moves'];
const PAD_DEFAULTS = {
    hp: 'WEST', lp: 'SOUTH', block: 'R1', hk: 'NORTH', lk: 'EAST', run: 'L1+L2',
    special: 'R2', pause: 'START', moves: 'SELECT',
};
const PAD5_NAMES = ['p', 'b', 'k', 'r'];
const PAD5_DEFAULTS = { p: 'WEST+NORTH', b: 'R1', k: 'SOUTH+EAST', r: 'L1+L2' };
const PAD_VALUE = /^([A-Z0-9]+(\+[A-Z0-9]+)*)?$/i;

/* Player two (DUELO, two players on one PC): p2key_<name>= for the moves
 * and the six buttons and S, p2key5_<p|b|k|r>=, p2pad=, p2pad_name=,
 * p2pad_<action>= and p2pad5_<p|b|k|r>= -- the same as player one's. The
 * keys default to the game's second set: arrows and the numeric keypad. */
const P2_KEY_NAMES = ['up', 'down', 'left', 'right',
    'hp', 'lp', 'block', 'hk', 'lk', 'run', 'special'];
const P2_KEY_DEFAULTS = {
    up: 38, down: 40, left: 37, right: 39,
    hp: 103, lp: 104, block: 105, hk: 100, lk: 101, run: 102, special: 96,
};
const P2_KEY5_DEFAULTS = { p: 103, b: 105, k: 100, r: 102 };

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
        uiSize: 'iphone',            /* ui_size=: iphone (480x320) or ipad (960x640: HUD at 1.75 of 2) */
        textureQuality: 'iphone',    /* texture_quality=: iphone (the _LOW sheets), ipad (full sheets) or max (full sheets, PNG first, mipmaps) */
        aspect: '3:2',               /* aspect=: 3:2, 4:3, 16:9 or 21:9 */
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
        padKeys: Object.assign({}, PAD_DEFAULTS),
        padKeys5: Object.assign({}, PAD5_DEFAULTS),
        p2Pad: 'auto',               /* p2pad=: player two's pad */
        p2PadName: '',
        p2Keys: Object.assign({}, P2_KEY_DEFAULTS),
        p2Keys5: Object.assign({}, P2_KEY5_DEFAULTS),
        p2PadKeys: Object.assign({}, PAD_DEFAULTS),
        p2PadKeys5: Object.assign({}, PAD5_DEFAULTS),
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
        else if (key === 'aspect') cfg.aspect = ['3:2', '4:3', '16:9', '21:9'].includes(val) ? val : '3:2';
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
        } else if (key === 'p2pad') {
            cfg.p2Pad = /^(auto|off|xinput|[0-9a-f]{4}:[0-9a-f]{4})$/i.test(val)
                ? val.toLowerCase() : 'auto';
        } else if (key === 'p2pad_name') {
            cfg.p2PadName = val;
        } else if (key.startsWith('p2pad5_') && PAD5_NAMES.includes(key.slice(7))) {
            if (PAD_VALUE.test(val))
                cfg.p2PadKeys5[key.slice(7)] = val.toUpperCase();
        } else if (key.startsWith('p2pad_') && PAD_NAMES.includes(key.slice(6))) {
            if (PAD_VALUE.test(val))
                cfg.p2PadKeys[key.slice(6)] = val.toUpperCase();
        } else if (key.startsWith('p2key5_') && KEY5_NAMES.includes(key.slice(7))) {
            if (num > 0 && num < 256)
                cfg.p2Keys5[key.slice(7)] = num;
        } else if (key.startsWith('p2key_') && P2_KEY_NAMES.includes(key.slice(6))) {
            if (num > 0 && num < 256)
                cfg.p2Keys[key.slice(6)] = num;
        } else if (key === 'pad') {
            cfg.pad = /^(auto|off|xinput|[0-9a-f]{4}:[0-9a-f]{4})$/i.test(val)
                ? val.toLowerCase() : 'auto';
        } else if (key === 'ui_size') {
            cfg.uiSize = /^(iphone|ipad)$/i.test(val) ? val.toLowerCase() : 'iphone';
        } else if (key === 'texture_quality') {
            cfg.textureQuality = /^(iphone|ipad|max)$/i.test(val) ? val.toLowerCase() : 'iphone';
        } else if (key === 'pad_dbg_menu') {
            cfg.padDbgMenu = /^([A-Z0-9]+(\+[A-Z0-9]+)*)?$/i.test(val) ? val.toUpperCase() : 'L3+R3';
        } else if (key === 'pad_name') {
            cfg.padName = val;
        } else if (key.startsWith('pad5_') && PAD5_NAMES.includes(key.slice(5))) {
            if (PAD_VALUE.test(val))
                cfg.padKeys5[key.slice(5)] = val.toUpperCase();
        } else if (key.startsWith('pad_') && PAD_NAMES.includes(key.slice(4))) {
            if (PAD_VALUE.test(val))
                cfg.padKeys[key.slice(4)] = val.toUpperCase();
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
        'aspect=' + (cfg.aspect || '3:2'),
        'widescreen=' + (cfg.widescreen ? 1 : 0),
        'language=' + lang,
        'ui=' + (cfg.ui === 'EN' ? 'EN' : 'ES'),
        'ipa=' + cfg.ipa,
        'debug_keys=' + (cfg.debug ? 1 : 0),
        'skip_intro=' + (cfg.skipIntro ? 1 : 0),
        'hide_controls=' + (cfg.hideControls ? 1 : 0),
        'buttons=' + (cfg.buttons === 6 ? 6 : 5),
        'pad=' + (cfg.pad || 'auto'),
        'texture_quality=' + (cfg.textureQuality || 'iphone'),
        'ui_size=' + (cfg.uiSize || 'iphone'),
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
    const pk = cfg.padKeys || {}, pk5 = cfg.padKeys5 || {};
    for (const name of PAD_NAMES)
        lines.push('pad_' + name + '=' + (pk[name] === undefined ? PAD_DEFAULTS[name] : pk[name]));
    for (const name of PAD5_NAMES)
        lines.push('pad5_' + name + '=' + (pk5[name] === undefined ? PAD5_DEFAULTS[name] : pk5[name]));
    lines.push('p2pad=' + (cfg.p2Pad || 'auto'));
    if (cfg.p2PadName)
        lines.push('p2pad_name=' + cfg.p2PadName.replace(/[\r\n]/g, ' '));
    const k2 = cfg.p2Keys || {}, k25 = cfg.p2Keys5 || {};
    for (const name of P2_KEY_NAMES)
        lines.push('p2key_' + name + '=' + (k2[name] || P2_KEY_DEFAULTS[name]));
    for (const name of KEY5_NAMES)
        lines.push('p2key5_' + name + '=' + (k25[name] || P2_KEY5_DEFAULTS[name]));
    const p2k = cfg.p2PadKeys || {}, p2k5 = cfg.p2PadKeys5 || {};
    for (const name of PAD_NAMES)
        lines.push('p2pad_' + name + '=' + (p2k[name] === undefined ? PAD_DEFAULTS[name] : p2k[name]));
    for (const name of PAD5_NAMES)
        lines.push('p2pad5_' + name + '=' + (p2k5[name] === undefined ? PAD5_DEFAULTS[name] : p2k5[name]));
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

module.exports = { RES, LANGS, KEY_NAMES, KEY_DEFAULTS, KEY5_NAMES, KEY5_DEFAULTS,
                   PAD_NAMES, PAD_DEFAULTS, PAD5_NAMES, PAD5_DEFAULTS,
                   P2_KEY_NAMES, P2_KEY_DEFAULTS, P2_KEY5_DEFAULTS, defaultConfig,
                   load, save, gameReady, gameBuiltAt };