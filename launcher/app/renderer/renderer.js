/* UMK3 Launcher renderer: the game-folder / compiler / settings logic of the
 * old launcher.c, driven by the contextBridge API (window.umk3) -- nothing
 * here touches Node or the filesystem directly.
 *
 *   - every change writes umk3.ini at once (config:save)
 *   - Compile runs launcher\build_game.ps1 and streams its log and progress
 *   - controls: click an action, press a key; Esc cancels
 *   - the menu works with the mouse or the keyboard (arrows, Enter, Esc);
 *     F11 toggles fullscreen
 */
'use strict';

const T = {
    ES: {
        ui: 'ENGLISH', ttlSub: 'PORT DE PC',
        mBuild: 'COMPILAR TU .IPA', mVideo: 'GRÁFICOS', mControls: 'CONTROLES',
        mPlay: 'JUGAR', mQuit: 'SALIR',
        dBuild: 'El juego se compila en tu PC a partir de tu propio .ipa de UMK3 para iPhone (1.2.59).',
        dVideo: 'Resolución de ventana, resolución 3D, modo de pantalla y el marco de las barras.',
        dControls: 'Las teclas del jugador 1, para los esquemas de 5 y de 6 botones, y las del modo debug.',
        dPlay: 'Arranca Ultimate Mortal Kombat 3: idioma del juego, intros y modo debug.',
        dQuit: 'Cierra el launcher.',
        hBuild: 'COMPILAR TU .IPA', ipaLbl: 'TU .IPA DE UMK3 (IPHONE 1.2.59)',
        browse: 'BUSCAR…', build: 'COMPILAR', rebuild: 'RECOMPILAR', cancel: 'CANCELAR', clear: 'QUITAR',
        builtAt: 'JUEGO COMPILADO', notBuilt: 'TODAVÍA NO ESTÁ COMPILADO: ELIGE TU .IPA Y PULSA COMPILAR.',
        hVideo: 'GRÁFICOS', res: 'RESOLUCIÓN DE VENTANA', rres: 'RESOLUCIÓN 3D', mode: 'MODO DE PANTALLA',
        modes: ['VENTANA', 'PANTALLA COMPLETA', 'PANTALLA COMPLETA SIN BORDES'], sameRes: 'IGUAL QUE LA VENTANA', tabDbg: 'DEBUG', tabP1: 'JUGADOR 1',
        aspect: 'RELACIÓN DE ASPECTO', aspects: ['3:2 (ORIGINAL)'], aa: 'ANTIALIASING', aaOff: 'DESACTIVADO', hidectl: 'OCULTAR LOS CONTROLES EN PANTALLA (JOYSTICK Y BOTONES)',
        lang: 'IDIOMA DEL JUEGO', frame: 'MARCO (PANTALLA COMPLETA)',
        dbg: 'MODO DEBUG (MENÚ Y ATAJOS, VER CONTROLES)', hDbgKeys: 'TECLAS Y ATAJOS DEL MODO DEBUG', gDbgMenu: 'MENÚ DEBUG', gDbgFight: 'PELEA', gDbgScreens: 'PANTALLAS', dbgKeysNote: 'SOLO ACTÚAN CON EL MODO DEBUG ACTIVADO (SECCIÓN JUGAR).', wsNote: 'PANTALLA ANCHA (16:9): EN DESARROLLO. EN PANTALLA COMPLETA, EL MARCO RELLENA LAS BARRAS.',
        hControls: 'CONTROLES DEL JUGADOR 1', gMove: 'MOVIMIENTO', gButtons: 'BOTONES', gSystem: 'SISTEMA',
        schemes: 'EL JUEGO EMPIEZA CON EL ESQUEMA ELEGIDO; CADA UNO GUARDA SUS TECLAS. EN PAUSA SE PUEDE CAMBIAR DURANTE LA PARTIDA.',
        tab6: '6 BOTONES', tab5: '5 BOTONES', tabNote: 'EL JUEGO EMPIEZA CON ESTE ESQUEMA',
        reskey: 'RESTABLECER', press: 'PULSA UNA TECLA…  (ESC CANCELA)',
        hint: 'CLIC EN UNA ACCIÓN Y PULSA LA TECLA',
        hPlay: 'JUGAR', play: 'JUGAR', skip: 'SALTAR LAS INTROS (LOGOS DEL INICIO)',
        cSelect: 'SELECCIONAR', cBack: 'ATRÁS', cFull: 'PANTALLA COMPLETA',
        auto: 'Automático (Windows)', original: '(IPHONE)',
        keys: { up: 'Arriba', down: 'Abajo', left: 'Izquierda', right: 'Derecha',
                hp: 'Puño alto', lp: 'Puño bajo', block: 'Bloqueo', hk: 'Patada alta',
                lk: 'Patada baja', run: 'Correr', special: 'Especial (S)',
                '5:p': 'Puño (P)', '5:b': 'Bloqueo (B)', '5:k': 'Patada (K)', '5:r': 'Correr (R)',
                pause: 'Pausa', moves: 'Combos',
                dbg_menu: 'Abrir menú', dbg_info: 'Línea de info', dbg_page_prev: 'Página anterior', dbg_page_next: 'Página siguiente',
                dbg_ko_p2: 'Ganar ronda', dbg_ko_p1: 'Perder ronda', dbg_win: 'Ganar combate', dbg_lose: 'Perder combate',
                dbg_scr_prev: 'Pantalla anterior', dbg_scr_next: 'Pantalla siguiente', dbg_scr_menu: 'Menú principal' },
        statusNeed: 'FALTA COMPILAR', statusReady: 'JUEGO LISTO',
        statusBuilding: 'COMPILANDO…', statusPlaying: 'JUGANDO…',
        needIpa: 'Primero elige tu archivo .ipa de UMK3.', playFail: 'No se pudo iniciar umk3-game.exe.',
        buildDone: 'JUEGO COMPILADO. YA PUEDES JUGAR.', buildFail: 'FALLÓ LA COMPILACIÓN: MIRA EL REGISTRO.',
        notIpa: 'El archivo elegido no existe.',
    },
    EN: {
        ui: 'ESPAÑOL', ttlSub: 'PC PORT',
        mBuild: 'COMPILE YOUR .IPA', mVideo: 'GRAPHICS', mControls: 'CONTROLS',
        mPlay: 'PLAY', mQuit: 'QUIT',
        dBuild: 'The game is compiled on your PC from your own UMK3 iPhone .ipa (1.2.59).',
        dVideo: 'Window resolution, 3D resolution, display mode and the bars frame.',
        dControls: 'Player 1 keys, for the 5- and 6-button layouts, and debug mode\'s keys.',
        dPlay: 'Starts Ultimate Mortal Kombat 3: game language, intros and debug mode.',
        dQuit: 'Closes the launcher.',
        hBuild: 'COMPILE YOUR .IPA', ipaLbl: 'YOUR UMK3 .IPA (IPHONE 1.2.59)',
        browse: 'BROWSE…', build: 'COMPILE', rebuild: 'REBUILD', cancel: 'CANCEL', clear: 'CLEAR',
        builtAt: 'GAME COMPILED', notBuilt: 'NOT COMPILED YET: CHOOSE YOUR .IPA AND PRESS COMPILE.',
        hVideo: 'GRAPHICS', res: 'WINDOW RESOLUTION', rres: '3D RESOLUTION', mode: 'DISPLAY MODE',
        modes: ['WINDOW', 'FULLSCREEN', 'BORDERLESS FULLSCREEN'], sameRes: 'SAME AS THE WINDOW', tabDbg: 'DEBUG', tabP1: 'PLAYER 1',
        aspect: 'ASPECT RATIO', aspects: ['3:2 (ORIGINAL)'], aa: 'ANTIALIASING', aaOff: 'OFF', hidectl: 'HIDE THE ON-SCREEN CONTROLS (STICK AND BUTTONS)',
        lang: 'GAME LANGUAGE', frame: 'FRAME (FULLSCREEN)',
        dbg: 'DEBUG MODE (MENU AND SHORTCUTS, SEE CONTROLS)', hDbgKeys: 'DEBUG MODE KEYS AND SHORTCUTS', gDbgMenu: 'DEBUG MENU', gDbgFight: 'FIGHT', gDbgScreens: 'SCREENS', dbgKeysNote: 'THEY ONLY ACT WITH DEBUG MODE ON (PLAY SECTION).', wsNote: 'WIDESCREEN (16:9): IN DEVELOPMENT. IN FULLSCREEN THE FRAME FILLS THE BARS.',
        hControls: 'PLAYER 1 CONTROLS', gMove: 'MOVEMENT', gButtons: 'BUTTONS', gSystem: 'SYSTEM',
        schemes: 'THE GAME STARTS WITH THE CHOSEN LAYOUT; EACH KEEPS ITS OWN KEYS. THE PAUSE MENU CAN SWITCH IT DURING A GAME.',
        tab6: '6 BUTTONS', tab5: '5 BUTTONS', tabNote: 'THE GAME STARTS WITH THIS LAYOUT',
        reskey: 'RESET', press: 'PRESS A KEY…  (ESC CANCELS)',
        hint: 'CLICK AN ACTION, THEN PRESS A KEY',
        hPlay: 'PLAY', play: 'PLAY', skip: 'SKIP THE INTROS (STARTUP LOGOS)',
        cSelect: 'SELECT', cBack: 'BACK', cFull: 'FULLSCREEN',
        auto: 'Automatic (Windows)', original: '(IPHONE)',
        keys: { up: 'Up', down: 'Down', left: 'Left', right: 'Right',
                hp: 'High punch', lp: 'Low punch', block: 'Block', hk: 'High kick',
                lk: 'Low kick', run: 'Run', special: 'Special (S)',
                '5:p': 'Punch (P)', '5:b': 'Block (B)', '5:k': 'Kick (K)', '5:r': 'Run (R)',
                pause: 'Pause', moves: 'Moves',
                dbg_menu: 'Open menu', dbg_info: 'Info line', dbg_page_prev: 'Previous page', dbg_page_next: 'Next page',
                dbg_ko_p2: 'Win round', dbg_ko_p1: 'Lose round', dbg_win: 'Win match', dbg_lose: 'Lose match',
                dbg_scr_prev: 'Previous screen', dbg_scr_next: 'Next screen', dbg_scr_menu: 'Main menu' },
        statusNeed: 'NOT COMPILED', statusReady: 'GAME READY',
        statusBuilding: 'COMPILING…', statusPlaying: 'PLAYING…',
        needIpa: 'Choose your UMK3 .ipa file first.', playFail: 'Could not start umk3-game.exe.',
        buildDone: 'GAME COMPILED. YOU CAN PLAY NOW.', buildFail: 'BUILD FAILED: SEE THE LOG.',
        notIpa: 'The chosen file does not exist.',
    },
};

/* index = res in umk3.ini (lib/ini.js): the game's own 3:2 sizes and 16:9 ones */
const RES = [
    { w: 480, h: 320 }, { w: 960, h: 640 }, { w: 1440, h: 960 },
    { w: 1920, h: 1280 }, { w: 2400, h: 1600 }, { w: 2880, h: 1920 },
    { w: 3840, h: 2560 },
    { w: 1280, h: 720 }, { w: 1600, h: 900 }, { w: 1920, h: 1080 },
    { w: 2560, h: 1440 }, { w: 3840, h: 2160 },
];
/* antialiasing= in umk3.ini: the multisampling the game asks the card for */
const AA = [0, 2, 4, 8, 16];
const LANGS = [null, 'English', 'Español', 'Français', 'Deutsch', 'Italiano', '한국어', '中文'];
/* "5:x" names a key of the five-button layout (umk3.ini key5_x); the rest
 * are key_<name>. The special button (S) exists only in the five. */
const KGROUPS = {
    'k-move': () => ['up', 'down', 'left', 'right'],
    'k-buttons': () => (cfg.buttons !== 6
        ? ['5:p', '5:b', '5:k', '5:r', 'special']
        : ['hp', 'lp', 'block', 'hk', 'lk', 'run']),
    'k-system': () => ['pause', 'moves'],
    'k-dbgmenu': () => ['dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next'],
    'k-dbgfight': () => ['dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose'],
    'k-dbgscr': () => ['dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu'],
};
const DBG_KEYS = ['dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next',
    'dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose',
    'dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu'];
const KEY5_DEFAULT = { p: 85, b: 79, k: 74, r: 76 };
function keyGet(name) {
    if (name.startsWith('5:'))
        return (cfg.keys5 && cfg.keys5[name.slice(2)]) || KEY5_DEFAULT[name.slice(2)];
    return (cfg.keys && cfg.keys[name]) || KEY_DEFAULT[name];
}
function keySet(name, vk) {
    if (name.startsWith('5:')) {
        cfg.keys5[name.slice(2)] = vk;
        save({ keys5: { [name.slice(2)]: vk } });
    } else {
        cfg.keys[name] = vk;
        save({ keys: { [name]: vk } });
    }
}
const KEY_DEFAULT = {
    up: 87, down: 83, left: 65, right: 68,
    hp: 85, lp: 73, block: 79, hk: 74, lk: 75, run: 76,
    pause: 80, moves: 77, special: 72,
    dbg_menu: 113, dbg_info: 114, dbg_page_prev: 81, dbg_page_next: 69,
    dbg_ko_p2: 120, dbg_ko_p1: 121, dbg_win: 122, dbg_lose: 123,
    dbg_scr_prev: 117, dbg_scr_next: 118, dbg_scr_menu: 119,
};
const SECTIONS = ['build', 'video', 'controls', 'play', 'quit'];
const DESC = { build: 'dBuild', video: 'dVideo', controls: 'dControls', play: 'dPlay', quit: 'dQuit' };

const $ = id => document.getElementById(id);
const el = (tag, cls, text) => {
    const n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text !== undefined) n.textContent = text;
    return n;
};

let cfg = null, ready = false, builtAt = null, building = false;
let ui = 'ES', waitingKey = null, sec = 'build';

/* ------------------------------------------------------------- sounds -- */

let g_ctx = null;
function beep(freq, dur, wave, vol) {
    try {
        g_ctx = g_ctx || new (window.AudioContext || window.webkitAudioContext)();
        const o = g_ctx.createOscillator(), g = g_ctx.createGain();
        o.type = wave || 'square';
        o.frequency.value = freq;
        g.gain.value = vol || 0.03;
        o.connect(g).connect(g_ctx.destination);
        o.start(g_ctx.currentTime);
        o.stop(g_ctx.currentTime + dur);
    } catch (err) { /* decoration only */ }
}
const hoverBlip = () => beep(660, 0.035, 'square', 0.02);
const selectBlip = () => { beep(440, 0.05, 'square', 0.035); beep(880, 0.045, 'square', 0.02); };
const doneChord = () => { beep(523, 0.09); setTimeout(() => beep(784, 0.14), 90); };
const failChord = () => { beep(220, 0.18, 'sawtooth'); setTimeout(() => beep(160, 0.18, 'sawtooth'), 120); };

/* -------------------------------------------------------------- text -- */

const tx = k => (T[ui][k] !== undefined ? T[ui][k] : k);

function applyTexts() {
    document.querySelectorAll('[data-t]').forEach(n => { n.textContent = tx(n.dataset.t); });
    $('langSw').textContent = T[ui].ui;
    $('keyhint').textContent = waitingKey ? T[ui].press : T[ui].hint;
    fillSelects();
    applyConfig();
    paintKeys();
    paintState();
    $('desc').textContent = tx(DESC[sec]);
}

function setStatus(kind, text) {
    const s = $('status');
    s.className = kind;
    s.textContent = text || '';
}

/* Whether the game is built, and when: the Compile and Play pages say it. */
function paintState() {
    const when = builtAt ? new Date(builtAt).toLocaleString(ui === 'ES' ? 'es' : 'en') : '';
    for (const id of ['buildstate', 'playstate']) {
        const b = $(id);
        b.className = 'state ' + (ready ? 'ok' : 'need');
        b.textContent = ready ? T[ui].builtAt + ' — ' + when : T[ui].notBuilt;
    }
    $('build').textContent = ready ? T[ui].rebuild : T[ui].build;
    $('play').disabled = !ready || building;
    if (building) setStatus('building', T[ui].statusBuilding);
    else setStatus(ready ? 'ready' : 'need', ready ? T[ui].statusReady : T[ui].statusNeed);
}

/* ---------------------------------------------------------- sections -- */

function setSection(next, run) {
    if (next === 'quit') {
        if (run) window.umk3.close();
    } else {
        sec = next;
        document.querySelectorAll('.sec').forEach(s =>
            s.classList.toggle('active', s.dataset.sec === next));
    }
    document.querySelectorAll('.mi').forEach(b =>
        b.classList.toggle('active', b.dataset.sec === next));
    $('desc').textContent = tx(DESC[next]);
}

/* --------------------------------------------------------------- keys -- */

const SPECIAL = {
    8: 'BACKSPACE', 9: 'TAB', 13: 'ENTER', 27: 'ESC', 32: 'SPACE',
    16: 'SHIFT', 17: 'CTRL', 18: 'ALT', 20: 'CAPS', 33: 'PGUP', 34: 'PGDN',
    35: 'END', 36: 'HOME', 37: '←', 38: '↑', 39: '→', 40: '↓',
    45: 'INS', 46: 'DEL', 144: 'NUMLOCK', 145: 'SCROLL',
    186: ';', 187: '=', 188: ',', 189: '-', 190: '.', 191: '/', 192: '`',
    219: '[', 220: '\\', 221: ']', 222: "'",
};
function vkLabel(vk) {
    if ((vk >= 65 && vk <= 90) || (vk >= 48 && vk <= 57)) return String.fromCharCode(vk);
    if (vk >= 112 && vk <= 135) return 'F' + (vk - 111);
    if (vk >= 96 && vk <= 105) return 'NUM ' + (vk - 96);
    if (vk >= 106 && vk <= 111) return 'NUM ' + ['*', '+', '', '-', '.', '/'][vk - 106];
    return SPECIAL[vk] || '#' + vk;
}

/* CONTROLS has two categories: PLAYER 1 -- with the 6- and 5-button layouts
 * (each its keys, and the one the game starts with) -- and DEBUG (the debug
 * mode's keys). */
let ctlTab = 'p';

function paintKeys() {
    document.querySelectorAll('.tab').forEach(t =>
        t.classList.toggle('active', t.dataset.c ? t.dataset.c === ctlTab
            : Number(t.dataset.b) === (cfg.buttons === 6 ? 6 : 5)));
    for (const [id, on] of [['hCtlP', 'p'], ['ctlP', 'p'], ['reskeys', 'p'], ['ctlLayouts', 'p'],
                            ['hCtlD', 'dbg'], ['ctlD', 'dbg'], ['resdbg', 'dbg']])
        $(id).classList.toggle('hidden', ctlTab !== on);
    for (const [box, names] of Object.entries(KGROUPS)) {
        const b = $(box);
        b.innerHTML = '';
        for (const name of names()) {
            const vk = keyGet(name);
            const k = el('button', 'keybtn' + (waitingKey === name ? ' waiting' : ''));
            k.appendChild(el('span', 'kname', T[ui].keys[name]));
            k.appendChild(el('span', 'kkey', vkLabel(vk)));
            k.addEventListener('pointerenter', hoverBlip);
            k.addEventListener('click', () => {
                selectBlip();
                waitingKey = name;
                $('keyhint').textContent = T[ui].press;
                paintKeys();
            });
            b.appendChild(k);
        }
    }
}

/* ------------------------------------------------------------ config -- */

function fillSelects() {
    const res = $('res'), rres = $('rres'), mode = $('mode'), lang = $('lang');
    const label = (r, i) => r.w + ' × ' + r.h + (i === 0 ? '  ' + T[ui].original : '');
    res.innerHTML = '';
    RES.forEach((r, i) => res.appendChild(el('option', null, label(r, i))));
    rres.innerHTML = '';
    rres.appendChild(el('option', null, T[ui].sameRes));
    RES.forEach((r, i) => rres.appendChild(el('option', null, label(r, i))));
    mode.innerHTML = '';
    T[ui].modes.forEach(m => mode.appendChild(el('option', null, m)));
    $('aspect').innerHTML = '';
    T[ui].aspects.forEach(a => $('aspect').appendChild(el('option', null, a)));
    $('aa').innerHTML = '';
    AA.forEach(n => $('aa').appendChild(el('option', null, n ? 'x' + n : T[ui].aaOff)));
    lang.innerHTML = '';
    LANGS.forEach(n => lang.appendChild(el('option', null, n || T[ui].auto)));
}

function applyConfig() {
    $('ipa').value = cfg.ipa || '';
    $('marco').value = cfg.marco || '';
    $('res').selectedIndex = cfg.res >= 0 ? cfg.res : 1;
    $('lang').selectedIndex = cfg.language || 0;
    $('rres').selectedIndex = cfg.rres > 0 ? cfg.rres : 0;
    $('mode').selectedIndex = cfg.fullscreen ? 1 : cfg.borderless ? 2 : 0;
    $('aspect').selectedIndex = 0;
    $('aa').selectedIndex = Math.max(0, AA.indexOf(cfg.antialiasing || 0));
    $('dbg').checked = !!cfg.debug;
    $('skip').checked = !!cfg.skipIntro;
    $('hidectl').checked = !!cfg.hideControls;
}

async function save(delta) {
    const next = await window.umk3.saveConfig(delta);
    if (next) cfg = next;
}

/* -------------------------------------------------------------- build -- */

function setBar(pct, fail) {
    const bar = $('buildbar');
    if (!bar.firstChild) bar.appendChild(el('i'));
    bar.firstChild.style.width = Math.max(0, Math.min(100, pct || 0)) + '%';   /* CSSOM: allowed by the CSP */
    $('buildbar').classList.toggle('fail', !!fail);
}

function logLine(line) {
    const c = $('console');
    c.textContent += line + '\n';
    if (c.textContent.length > 20000) c.textContent = c.textContent.slice(-16000);
    c.scrollTop = c.scrollHeight;
}

function showBuildUI(on) {
    building = on;
    $('buildwrap').classList.toggle('hidden', !on);
    $('cancel').classList.toggle('hidden', !on);
    if (on) {
        $('console').classList.remove('hidden');
        $('console').textContent = '';
        setBar(0);
        $('buildlabel').textContent = '';
    }
    $('build').disabled = on;
    $('browse').disabled = on;
    paintState();
}

function handleBuildData(m) {
    if (m.pct !== undefined && m.pct !== null) setBar(m.pct);
    if (m.phase) $('buildlabel').textContent = m.phase;
    if (m.text) logLine(m.text);
}

async function refreshRoot() {
    const info = await window.umk3.root();
    ready = info.ready;
    builtAt = info.builtAt;
    paintState();
}

async function startBuild() {
    if (building) return;
    if (!cfg.ipa || !cfg.ipa.trim()) {
        setStatus('need', T[ui].needIpa);
        failChord();
        return;
    }
    selectBlip();
    showBuildUI(true);
    const r = await window.umk3.startBuild(cfg.ipa.trim());
    showBuildUI(false);
    $('buildwrap').classList.remove('hidden');
    await refreshRoot();
    if (r.ok) {
        setBar(100);
        logLine('OK.');
        setStatus('ready', T[ui].buildDone);
        doneChord();
    } else {
        setBar(100, true);
        logLine(r.code === -3 ? 'ERROR: ' + T[ui].notIpa : 'EXIT CODE: ' + r.code);
        setStatus('need', r.code === -3 ? T[ui].notIpa : T[ui].buildFail);
        failChord();
    }
}

/* --------------------------------------------------------------- play -- */

async function doPlay() {
    if (building || !ready) return;
    selectBlip();
    const r = await window.umk3.play();
    if (!r.ok) {
        setStatus('need', T[ui].playFail);
        failChord();
        return;
    }
    setStatus('playing', T[ui].statusPlaying);
}

/* ------------------------------------------------------------ keyboard -- */

function onKey(e) {
    if (waitingKey) {                       /* binding a key */
        e.preventDefault();
        e.stopPropagation();
        const w = waitingKey;
        waitingKey = null;
        if (e.keyCode !== 27 && e.keyCode < 256) {
            keySet(w, e.keyCode);
            selectBlip();
        }
        $('keyhint').textContent = T[ui].hint;
        paintKeys();
        return;
    }
    if (e.key === 'F11') {
        e.preventDefault();
        window.umk3.fullscreen();
        return;
    }
    const tag = (document.activeElement && document.activeElement.tagName) || '';
    if (tag === 'SELECT' || tag === 'INPUT') return;
    const i = SECTIONS.indexOf(sec);
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        e.preventDefault();
        const n = SECTIONS[(i + (e.key === 'ArrowDown' ? 1 : SECTIONS.length - 1)) % SECTIONS.length];
        hoverBlip();
        setSection(n === 'quit' ? 'quit' : n, false);
        if (n === 'quit') sec = 'quit';
    } else if (e.key === 'Enter' && document.activeElement === document.body) {
        e.preventDefault();
        selectBlip();
        setSection(sec, true);
    } else if (e.key === 'Escape') {
        setSection('build', false);
    }
}

/* -------------------------------------------------------------- init ---- */

async function init() {
    cfg = await window.umk3.loadConfig();
    ui = cfg.ui === 'EN' ? 'EN' : 'ES';
    await refreshRoot();
    applyTexts();

    $('closeBtn').addEventListener('click', () => window.umk3.close());
    $('minBtn').addEventListener('click', () => window.umk3.minimize());
    $('maxBtn').addEventListener('click', () => window.umk3.maximize());
    $('fullBtn').addEventListener('click', () => window.umk3.fullscreen());
    $('langSw').addEventListener('click', () => {
        ui = ui === 'ES' ? 'EN' : 'ES';
        selectBlip();
        save({ ui });
        applyTexts();
    });

    document.querySelectorAll('.mi').forEach(b => {
        b.addEventListener('pointerenter', hoverBlip);
        b.addEventListener('click', () => {
            selectBlip();
            setSection(b.dataset.sec, true);
            b.blur();
        });
    });

    $('browse').addEventListener('click', async () => {
        const p = await window.umk3.browseIpa();
        if (!p) return;
        await save({ ipa: p });
        $('ipa').value = p;
    });
    $('marcoBtn').addEventListener('click', async () => {
        const p = await window.umk3.browseFrame();
        if (!p) return;
        await save({ marco: p });
        $('marco').value = p;
    });
    $('marcoClr').addEventListener('click', async () => {
        await save({ marco: '' });
        $('marco').value = '';
    });
    $('res').addEventListener('change', () => save({ res: $('res').selectedIndex }));
    $('lang').addEventListener('change', () => save({ language: $('lang').selectedIndex }));
    $('rres').addEventListener('change', () => save({ rres: $('rres').selectedIndex }));
    $('aa').addEventListener('change', () => save({ antialiasing: AA[$('aa').selectedIndex] }));
    $('mode').addEventListener('change', () => {
        const m = $('mode').selectedIndex;
        save({ fullscreen: m === 1, borderless: m === 2 });
    });
    $('dbg').addEventListener('change', () => save({ debug: $('dbg').checked }));
    $('skip').addEventListener('change', () => save({ skipIntro: $('skip').checked }));
    $('hidectl').addEventListener('change', () => save({ hideControls: $('hidectl').checked }));

    window.umk3.onBuildData(handleBuildData);
    $('build').addEventListener('click', startBuild);
    $('cancel').addEventListener('click', () => window.umk3.cancelBuild());

    $('resdbg').addEventListener('click', () => {
        selectBlip();
        const d = {};
        for (const n of DBG_KEYS) d[n] = KEY_DEFAULT[n];
        Object.assign(cfg.keys, d);
        save({ keys: d });
        waitingKey = null;
        $('keyhint').textContent = T[ui].hint;
        paintKeys();
    });
    $('reskeys').addEventListener('click', () => {
        selectBlip();
        /* the player's keys only; the debug keys have their own reset */
        const d = Object.assign({}, KEY_DEFAULT);
        for (const n of DBG_KEYS) d[n] = keyGet(n);
        cfg.keys = d;
        cfg.keys5 = Object.assign({}, KEY5_DEFAULT);
        save({ keys: cfg.keys, keys5: cfg.keys5 });
        waitingKey = null;
        $('keyhint').textContent = T[ui].hint;
        paintKeys();
    });
    document.querySelectorAll('.tab').forEach(t => {
        t.addEventListener('pointerenter', hoverBlip);
        t.addEventListener('click', () => {
            selectBlip();
            if (t.dataset.c) {
                ctlTab = t.dataset.c;
            } else {
                cfg.buttons = Number(t.dataset.b);
                save({ buttons: cfg.buttons });
            }
            waitingKey = null;
            paintKeys();
            t.blur();
        });
    });
    const logo = await window.umk3.logo();
    if (logo) {
        $('logoImg').src = logo;
        $('logoImg').classList.remove('hidden');
        $('logo').classList.add('img');
    }
    window.addEventListener('keydown', onKey, true);
    $('play').addEventListener('click', doPlay);
    document.querySelectorAll('.btn').forEach(b => b.addEventListener('pointerenter', hoverBlip));

    const want = new URLSearchParams(location.search).get('sec');
    setSection(SECTIONS.includes(want) && want !== 'quit' ? want : ready ? 'play' : 'build', false);

    /* development aid: UMK3_LAUNCHER_SHOT=<png> captures the window */
    setTimeout(() => window.umk3.shot(), 600);
}

document.addEventListener('DOMContentLoaded', init);
