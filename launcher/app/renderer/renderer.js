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
        dControls: 'Teclado y mando del jugador 1 (Xbox, PlayStation, Switch y otros), para 5 y 6 botones, y las teclas del modo debug.',
        dPlay: 'Arranca Ultimate Mortal Kombat 3: idioma del juego, intros y modo debug.',
        dQuit: 'Cierra el launcher.',
        hBuild: 'COMPILAR TU .IPA', ipaLbl: 'TU .IPA DE UMK3 (IPHONE 1.2.59)',
        updCheck: 'BUSCAR ACTUALIZACIONES', updGet: 'DESCARGAR E INSTALAR', notesLbl: 'NOVEDADES DE LA ÚLTIMA VERSIÓN',
        updLooking: 'BUSCANDO ACTUALIZACIONES…', updNone: 'ESTÁS EN LA ÚLTIMA VERSIÓN', updNew: 'NUEVA VERSIÓN DISPONIBLE',
        updFail: 'NO SE PUDO CONSULTAR GITHUB', updYours: 'TU VERSIÓN', updDl: 'DESCARGANDO', updInst: 'INSTALANDO: EL LAUNCHER SE CERRARÁ Y VOLVERÁ A ABRIRSE…',
        updDone: 'ACTUALIZADO A', updRebuild: 'RECOMPILANDO EL JUEGO CON LA NUEVA VERSIÓN…', updFailed: 'LA ACTUALIZACIÓN NO SE PUDO COPIAR (VER update\\apply.log)', updNoNotes: 'SIN NOTAS.',
        browse: 'BUSCAR…', build: 'COMPILAR', rebuild: 'RECOMPILAR', cancel: 'CANCELAR', clear: 'QUITAR',
        builtAt: 'JUEGO COMPILADO', notBuilt: 'TODAVÍA NO ESTÁ COMPILADO: ELIGE TU .IPA Y PULSA COMPILAR.',
        hVideo: 'GRÁFICOS', res: 'RESOLUCIÓN DE VENTANA', rres: 'RESOLUCIÓN 3D', mode: 'MODO DE PANTALLA',
        modes: ['VENTANA', 'PANTALLA COMPLETA', 'PANTALLA COMPLETA SIN BORDES'], sameRes: 'IGUAL QUE LA VENTANA', tabDbg: 'DEBUG', tabKb: 'TECLADO', tabPad: 'MANDO',
        aspect: 'RELACIÓN DE ASPECTO', aspects: ['3:2 (ORIGINAL, IPHONE)', '4:3 (IPAD)', '16:9 (PANORÁMICA)', '21:9 (ULTRA PANORÁMICA)'], aa: 'ANTIALIASING', aaOff: 'DESACTIVADO', texq: 'CALIDAD DE TEXTURAS', texqs: ['IPHONE (ACTUAL)', 'IPAD', 'SIN LÍMITE'], uisize: 'TAMAÑO DE LA INTERFAZ', uisizes: ['IPHONE (ACTUAL)', 'IPAD (HUD MÁS PEQUEÑO)'], hidectl: 'OCULTAR LOS CONTROLES EN PANTALLA (JOYSTICK Y BOTONES)',
        lang: 'IDIOMA DEL JUEGO', frame: 'MARCO (PANTALLA COMPLETA)',
        dbg: 'MODO DEBUG (MENÚ Y ATAJOS, VER CONTROLES)', hDbgKeys: 'TECLAS Y ATAJOS DEL MODO DEBUG', gDbgMenu: 'MENÚ DEBUG', gDbgFight: 'PELEA', gDbgScreens: 'PANTALLAS', dbgKeysNote: 'SOLO ACTÚAN CON EL MODO DEBUG ACTIVADO (SECCIÓN JUGAR).', wsNote: '4:3 ES LA VISTA DEL IPAD. EN 16:9 Y 21:9 LA PELEA OCUPA TODA LA PANTALLA (SE VE MÁS ESCENARIO, SIN ESTIRAR) Y LOS MENÚS QUEDAN EN 3:2. EN PANTALLA COMPLETA, EL MARCO RELLENA LAS BARRAS.',
        hControls: 'TECLADO DEL JUGADOR 1', hPad: 'MANDO DEL JUGADOR 1', gMove: 'MOVIMIENTO', gButtons: 'BOTONES', gSystem: 'SISTEMA',
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
        padDbgLbl: 'MANDO: ABRIR EL MENÚ DEBUG (PULSAR A LA VEZ)', padLbl: 'MANDO A USAR', gPad: 'BOTONES DEL MANDO',
        padOff: 'Ninguno (solo teclado)', padAuto: 'Mando: el primero que se conecte', padX: 'Mando de Xbox (XInput)',
        padSeen: 'NINGÚN MANDO CONECTADO EN WINDOWS', padLook: 'BUSCANDO MANDOS…', padNow: 'CONECTADO', padGone: '(no conectado)',
        padNote: 'EL TECLADO SIGUE FUNCIONANDO CON EL MANDO. LOS MENÚS SE USAN CON EL RATÓN.',
        pads: { dir: 'Moverse', start: 'Pausa', select: 'Combos' },
    },
    EN: {
        ui: 'ESPAÑOL', ttlSub: 'PC PORT',
        mBuild: 'COMPILE YOUR .IPA', mVideo: 'GRAPHICS', mControls: 'CONTROLS',
        mPlay: 'PLAY', mQuit: 'QUIT',
        dBuild: 'The game is compiled on your PC from your own UMK3 iPhone .ipa (1.2.59).',
        dVideo: 'Window resolution, 3D resolution, display mode and the bars frame.',
        dControls: 'Player 1 keyboard and gamepad (Xbox, PlayStation, Switch and others), for 5 and 6 buttons, and debug mode\'s keys.',
        dPlay: 'Starts Ultimate Mortal Kombat 3: game language, intros and debug mode.',
        dQuit: 'Closes the launcher.',
        hBuild: 'COMPILE YOUR .IPA', ipaLbl: 'YOUR UMK3 .IPA (IPHONE 1.2.59)',
        updCheck: 'CHECK FOR UPDATES', updGet: 'DOWNLOAD AND INSTALL', notesLbl: 'WHAT IS NEW IN THE LATEST VERSION',
        updLooking: 'CHECKING FOR UPDATES…', updNone: 'YOU HAVE THE LATEST VERSION', updNew: 'NEW VERSION AVAILABLE',
        updFail: 'COULD NOT REACH GITHUB', updYours: 'YOUR VERSION', updDl: 'DOWNLOADING', updInst: 'INSTALLING: THE LAUNCHER WILL CLOSE AND OPEN AGAIN…',
        updDone: 'UPDATED TO', updRebuild: 'RECOMPILING THE GAME WITH THE NEW VERSION…', updFailed: 'THE UPDATE COULD NOT BE COPIED (SEE update\\apply.log)', updNoNotes: 'NO NOTES.',
        browse: 'BROWSE…', build: 'COMPILE', rebuild: 'REBUILD', cancel: 'CANCEL', clear: 'CLEAR',
        builtAt: 'GAME COMPILED', notBuilt: 'NOT COMPILED YET: CHOOSE YOUR .IPA AND PRESS COMPILE.',
        hVideo: 'GRAPHICS', res: 'WINDOW RESOLUTION', rres: '3D RESOLUTION', mode: 'DISPLAY MODE',
        modes: ['WINDOW', 'FULLSCREEN', 'BORDERLESS FULLSCREEN'], sameRes: 'SAME AS THE WINDOW', tabDbg: 'DEBUG', tabKb: 'KEYBOARD', tabPad: 'GAMEPAD',
        aspect: 'ASPECT RATIO', aspects: ['3:2 (ORIGINAL, IPHONE)', '4:3 (IPAD)', '16:9 (WIDESCREEN)', '21:9 (ULTRAWIDE)'], aa: 'ANTIALIASING', aaOff: 'OFF', texq: 'TEXTURE QUALITY', texqs: ['IPHONE (CURRENT)', 'IPAD', 'NO LIMIT'], uisize: 'INTERFACE SIZE', uisizes: ['IPHONE (CURRENT)', 'IPAD (SMALLER HUD)'], hidectl: 'HIDE THE ON-SCREEN CONTROLS (STICK AND BUTTONS)',
        lang: 'GAME LANGUAGE', frame: 'FRAME (FULLSCREEN)',
        dbg: 'DEBUG MODE (MENU AND SHORTCUTS, SEE CONTROLS)', hDbgKeys: 'DEBUG MODE KEYS AND SHORTCUTS', gDbgMenu: 'DEBUG MENU', gDbgFight: 'FIGHT', gDbgScreens: 'SCREENS', dbgKeysNote: 'THEY ONLY ACT WITH DEBUG MODE ON (PLAY SECTION).', wsNote: '4:3 IS THE IPAD VIEW. IN 16:9 AND 21:9 THE FIGHT FILLS THE SCREEN (MORE OF THE STAGE, NO STRETCH) AND THE MENUS STAY 3:2. IN FULLSCREEN THE FRAME FILLS THE BARS.',
        hControls: 'PLAYER 1 KEYBOARD', hPad: 'PLAYER 1 GAMEPAD', gMove: 'MOVEMENT', gButtons: 'BUTTONS', gSystem: 'SYSTEM',
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
        padDbgLbl: 'GAMEPAD: OPEN THE DEBUG MENU (HOLD TOGETHER)', padLbl: 'GAMEPAD TO USE', gPad: 'GAMEPAD BUTTONS',
        padOff: 'None (keyboard only)', padAuto: 'Gamepad: the first one connected', padX: 'Xbox pad (XInput)',
        padSeen: 'NO GAMEPAD CONNECTED TO WINDOWS', padLook: 'LOOKING FOR GAMEPADS…', padNow: 'CONNECTED', padGone: '(not connected)',
        padNote: 'THE KEYBOARD STILL WORKS WITH A PAD. THE MENUS ARE USED WITH THE MOUSE.',
        pads: { dir: 'Move', start: 'Pause', select: 'Moves' },
    },
};

/* The pad, by position (runtime/game_main.c keyboard_buttons): what each
 * button does in either layout. [PlayStation / Xbox label, action key]. */
const PAD_ROWS = {
    6: [['D-PAD / L-STICK', 'dir'], ['□ / X', 'hp'], ['✕ / A', 'lp'], ['△ / Y', 'hk'],
        ['○ / B', 'lk'], ['R1 / RB', 'block'], ['L1 L2 / LB LT', 'run'], ['R2 / RT', 'special'],
        ['OPTIONS / START', 'start'], ['SHARE / BACK', 'select']],
    5: [['D-PAD / L-STICK', 'dir'], ['□ △ / X Y', '5:p'], ['✕ ○ / A B', '5:k'],
        ['R1 / RB', '5:b'], ['L1 L2 / LB LT', '5:r'], ['R2 / RT', 'special'],
        ['OPTIONS / START', 'start'], ['SHARE / BACK', 'select']],
};

/* The pads Windows has attached, by the name it gives them, as pad= values:
 * "xinput" or the USB "vendor:product". They come from the game itself
 * (`umk3-game --list-pads`, the same search it plays with), asked again
 * every few seconds while the GAMEPAD tab is open. */
let seenPads = {}, padScanBusy = false;
async function scanPads() {
    if (padScanBusy || sec !== 'controls' || ctlTab !== 'pad') return;
    padScanBusy = true;
    try {
        const now = {};
        for (const p of await window.umk3.listPads())
            if (!now[p.value]) now[p.value] = p.name;
        if (JSON.stringify(now) !== JSON.stringify(seenPads)) {
            seenPads = now;
            fillPads();
        }
    } finally {
        padScanBusy = false;
    }
}
/* The debug menu's pad combination (pad_dbg_menu=): two buttons held
 * together, the game's PB_* names. */
const PAD_BTNS = [['L3', 'L3'], ['R3', 'R3'], ['L1', 'L1 / LB'], ['R1', 'R1 / RB'],
    ['L2', 'L2 / LT'], ['R2', 'R2 / RT'], ['SELECT', 'SHARE / BACK'], ['START', 'OPTIONS / START'],
    ['SOUTH', '✕ / A'], ['EAST', '○ / B'], ['WEST', '□ / X'], ['NORTH', '△ / Y'], ['HOME', 'PS / HOME']];
function fillPadDbg() {
    const parts = (cfg.padDbgMenu || '').split('+');
    [['padDbgA', parts[0] || ''], ['padDbgB', parts[1] || '']].forEach(([id, v]) => {
        const s = $(id);
        s.innerHTML = '';
        const none = el('option', null, '—');
        none.value = '';
        s.appendChild(none);
        for (const [n, label] of PAD_BTNS) {
            const o = el('option', null, label);
            o.value = n;
            s.appendChild(o);
        }
        s.value = v;
    });
}
function savePadDbg() {
    const v = [$('padDbgA').value, $('padDbgB').value].filter(Boolean)
        .filter((x, i, a) => a.indexOf(x) === i).join('+');
    cfg.padDbgMenu = v;
    selectBlip();
    save({ padDbgMenu: v });
}

function fillPads() {
    fillPadDbg();
    const s = $('pad');
    if (!s || !cfg) return;
    const opts = [['off', T[ui].padOff], ['auto', T[ui].padAuto],
                  ['xinput', seenPads.xinput ? T[ui].padX + ': ' + seenPads.xinput : T[ui].padX]];
    for (const [v, n] of Object.entries(seenPads))
        if (v !== 'xinput') opts.push([v, n + '  (' + v.toUpperCase() + ')']);
    if (!opts.some(o => o[0] === cfg.pad))
        opts.push([cfg.pad, (cfg.padName || cfg.pad.toUpperCase()) + '  ' + T[ui].padGone]);
    s.innerHTML = '';
    for (const [v, n] of opts) {
        const o = el('option', null, (seenPads[v] ? '● ' : '') + n);   /* ● = connected */
        o.value = v;
        s.appendChild(o);
    }
    s.value = cfg.pad || 'auto';
    const any = Object.keys(seenPads).length;
    $('padSeen').textContent = any ? T[ui].padNow + ': ' + Object.values(seenPads).join(', ') : T[ui].padSeen;
    paintPadMap();
}
function paintPadMap() {
    const on = cfg.pad !== 'off';
    $('padMap').classList.toggle('hidden', !on);
    const b = $('k-pad');
    b.innerHTML = '';
    for (const [label, act] of PAD_ROWS[cfg.buttons === 6 ? 6 : 5]) {
        const k = el('div', 'keybtn ro');
        k.appendChild(el('span', 'kname', T[ui].pads[act] || T[ui].keys[act]));
        k.appendChild(el('span', 'kkey', label));
        b.appendChild(k);
    }
}

/* index = res in umk3.ini (lib/ini.js): the game's own 3:2 sizes and 16:9 ones */
const RES = [
    { w: 480, h: 320 }, { w: 960, h: 640 }, { w: 1440, h: 960 },
    { w: 1920, h: 1280 }, { w: 2400, h: 1600 }, { w: 2880, h: 1920 },
    { w: 3840, h: 2560 },
    { w: 1280, h: 720 }, { w: 1600, h: 900 }, { w: 1920, h: 1080 },
    { w: 2560, h: 1440 }, { w: 3840, h: 2160 },
    { w: 1024, h: 768 }, { w: 1600, h: 1200 }, { w: 2048, h: 1536 },
    { w: 2560, h: 1080 }, { w: 3440, h: 1440 },
];
/* aspect= in umk3.ini */
const ASPECT = ['3:2', '4:3', '16:9', '21:9'];
/* texture_quality= in umk3.ini: IPHONE = the _LOW sheets the 480-wide phone loads, IPAD = the full
 * sheets, NO LIMIT = full sheets, PNG before PVRTC, mipmaps and anisotropic filtering */
const TEXQ = ['iphone', 'ipad', 'max'];
/* ui_size= in umk3.ini: IPAD lays the game out on 960x640, where it draws the HUD at 1.75 of 2 */
const UISIZE = ['iphone', 'ipad'];

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
    fillPads();
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

/* CONTROLS has three categories: KEYBOARD and GAMEPAD -- player one, with
 * the 6- and 5-button layouts (each its keys, and the one the game starts
 * with) -- and DEBUG (the debug mode's keys). */
let ctlTab = 'p';

function paintKeys() {
    document.querySelectorAll('.tab').forEach(t =>
        t.classList.toggle('active', t.dataset.c ? t.dataset.c === ctlTab
            : Number(t.dataset.b) === (cfg.buttons === 6 ? 6 : 5)));
    for (const [id, on] of [['hCtlP', 'p'], ['ctlP', 'p'], ['reskeys', 'p'],
                            ['hCtlM', 'pad'], ['ctlM', 'pad'],
                            ['hCtlD', 'dbg'], ['ctlD', 'dbg'], ['resdbg', 'dbg']])
        $(id).classList.toggle('hidden', ctlTab !== on);
    $('ctlLayouts').classList.toggle('hidden', ctlTab === 'dbg');
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
    $('texq').innerHTML = '';
    T[ui].texqs.forEach(n => $('texq').appendChild(el('option', null, n)));
    $('uisize').innerHTML = '';
    T[ui].uisizes.forEach(n => $('uisize').appendChild(el('option', null, n)));
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
    $('aspect').selectedIndex = Math.max(0, ASPECT.indexOf(cfg.aspect || '3:2'));
    $('aa').selectedIndex = Math.max(0, AA.indexOf(cfg.antialiasing || 0));
    $('texq').selectedIndex = Math.max(0, TEXQ.indexOf(cfg.textureQuality || 'iphone'));
    $('uisize').selectedIndex = Math.max(0, UISIZE.indexOf(cfg.uiSize || 'iphone'));
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

/* ------------------------------------------------------------ updates -- */

let rel = null, updating = false;

function setUpdBar(pct) {
    const bar = $('updbar');
    if (!bar.firstChild) bar.appendChild(el('i'));
    bar.firstChild.style.width = Math.max(0, Math.min(100, pct || 0)) + '%';
}

/* A release's notes are GitHub markdown: headings, bullets, **bold**,
 * `code` and [links](...). Drawn as text nodes, never as HTML. */
function inlineMd(parent, text) {
    const re = /(\*\*[^*]+\*\*|`[^`]+`|\[[^\]]+\]\([^)]+\)|\*[^*\s][^*]*\*)/g;
    let at = 0, m;
    while ((m = re.exec(text))) {
        if (m.index > at) parent.appendChild(document.createTextNode(text.slice(at, m.index)));
        const t = m[0];
        if (t.startsWith('**')) parent.appendChild(el('b', null, t.slice(2, -2)));
        else if (t.startsWith('`')) parent.appendChild(el('code', null, t.slice(1, -1)));
        else if (t.startsWith('[')) parent.appendChild(el('u', null, t.slice(1, t.indexOf(']'))));
        else parent.appendChild(el('i', 'em', t.slice(1, -1)));
        at = m.index + t.length;
    }
    if (at < text.length) parent.appendChild(document.createTextNode(text.slice(at)));
}

function paintNotes(md) {
    const box = $('notes');
    box.textContent = '';
    let list = null;
    for (const raw of String(md || '').replace(/\r/g, '').split('\n')) {
        const line = raw.trimEnd();
        const h = line.match(/^#{1,6}\s+(.*)$/);
        const li = line.match(/^\s*[-*]\s+(.*)$/);
        if (li) {
            if (!list) { list = el('ul'); box.appendChild(list); }
            const n = el('li');
            inlineMd(n, li[1]);
            (/^\s{2,}/.test(line) && list.lastChild ? (list.lastChild.querySelector('ul') || list.lastChild.appendChild(el('ul'))) : list).appendChild(n);
            continue;
        }
        list = null;
        if (!line.trim()) continue;
        const n = el(h ? 'h4' : 'p');
        inlineMd(n, h ? h[1] : line);
        box.appendChild(n);
    }
    if (!box.firstChild) box.textContent = T[ui].updNoNotes;
}

function paintUpdate(r) {
    const s = $('updstate');
    $('updget').classList.add('hidden');
    if (!r) { s.className = 'state'; s.textContent = T[ui].updLooking; return; }
    if (!r.ok || !r.rel) {
        s.className = 'state need';
        s.textContent = T[ui].updFail + (r.error ? ' (' + r.error + ')' : '') + ' — ' + T[ui].updYours + ' ' + r.current;
        return;
    }
    rel = r.rel;
    $('notesLbl').textContent = T[ui].notesLbl + ' — ' + rel.name;
    paintNotes(rel.notes);
    if (rel.newer) {
        s.className = 'state need';
        s.textContent = T[ui].updNew + ': ' + rel.tag + ' (' + (rel.size / 1048576).toFixed(0) + ' MB) — ' + T[ui].updYours + ' ' + r.current;
        $('updget').classList.remove('hidden');
        document.querySelector('.mi[data-sec="build"]').classList.add('news');
    } else {
        s.className = 'state ok';
        s.textContent = T[ui].updNone + ' (' + r.current + ')';
    }
}

async function checkUpdate() {
    if (updating) return;
    $('updcheck').disabled = true;
    paintUpdate(null);
    paintUpdate(await window.umk3.checkUpdate());
    $('updcheck').disabled = false;
}

async function installUpdate() {
    if (updating || building || !rel) return;
    selectBlip();
    updating = true;
    $('updget').disabled = $('updcheck').disabled = $('build').disabled = true;
    $('updwrap').classList.remove('hidden');
    setUpdBar(0);
    const r = await window.umk3.installUpdate();
    if (!r.ok) {
        updating = false;
        $('updget').disabled = $('updcheck').disabled = $('build').disabled = false;
        $('updstate').className = 'state need';
        $('updstate').textContent = 'ERROR: ' + r.error;
        failChord();
    }
}

function handleUpdateProgress(p) {
    if (p.phase === 'install') {
        setUpdBar(100);
        $('updlabel').textContent = T[ui].updInst;
        return;
    }
    const pct = p.total ? p.got * 100 / p.total : 0;
    setUpdBar(pct);
    $('updlabel').textContent = T[ui].updDl + ' ' + (p.got / 1048576).toFixed(1) + ' / ' + (p.total / 1048576).toFixed(1) + ' MB';
}

/* Opened again by update/apply.ps1: say so and compile the new sources. */
async function afterUpdate() {
    const r = await window.umk3.updateResult();
    if (!r) return false;
    setSection('build', false);
    if (!r.ok) {
        $('updstate').className = 'state need';
        $('updstate').textContent = T[ui].updFailed;
        return true;
    }
    $('updstate').className = 'state ok';
    $('updstate').textContent = T[ui].updDone + ' ' + r.tag;
    $('notesLbl').textContent = T[ui].notesLbl + ' — ' + (r.name || r.tag);
    paintNotes(r.notes);
    if (cfg.ipa && cfg.ipa.trim()) {
        setStatus('building', T[ui].updRebuild);
        startBuild();
    }
    return true;
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
    $('texq').addEventListener('change', () => save({ textureQuality: TEXQ[$('texq').selectedIndex] }));
    $('aspect').addEventListener('change', () => save({ aspect: ASPECT[$('aspect').selectedIndex] }));
    $('uisize').addEventListener('change', () => save({ uiSize: UISIZE[$('uisize').selectedIndex] }));
    $('mode').addEventListener('change', () => {
        const m = $('mode').selectedIndex;
        save({ fullscreen: m === 1, borderless: m === 2 });
    });
    $('dbg').addEventListener('change', () => save({ debug: $('dbg').checked }));
    $('skip').addEventListener('change', () => save({ skipIntro: $('skip').checked }));
    $('hidectl').addEventListener('change', () => save({ hideControls: $('hidectl').checked }));
    $('pad').addEventListener('change', () => {
        const v = $('pad').value;
        cfg.pad = v;
        cfg.padName = seenPads[v] || '';
        selectBlip();
        save({ pad: v, padName: seenPads[v] || '' });
        paintPadMap();
    });
    $('padDbgA').addEventListener('change', savePadDbg);
    $('padDbgB').addEventListener('change', savePadDbg);
    /* pads come and go: asked again every three seconds on the GAMEPAD tab */
    setInterval(scanPads, 3000);

    window.umk3.onBuildData(handleBuildData);
    $('build').addEventListener('click', startBuild);
    $('cancel').addEventListener('click', () => window.umk3.cancelBuild());
    window.umk3.onUpdateProgress(handleUpdateProgress);
    $('updcheck').addEventListener('click', () => { selectBlip(); checkUpdate(); });
    $('updget').addEventListener('click', installUpdate);

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
                if (ctlTab === 'pad') {
                    $('padSeen').textContent = T[ui].padLook;
                    setTimeout(scanPads, 0);
                }
            } else {
                cfg.buttons = Number(t.dataset.b);
                save({ buttons: cfg.buttons });
            }
            waitingKey = null;
            paintKeys();
            paintPadMap();
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

    /* an update just installed is compiled first; else ask GitHub, without
     * holding up the window */
    if (!(await afterUpdate()))
        setTimeout(checkUpdate, 800);

    /* development aid: UMK3_LAUNCHER_SHOT=<png> captures the window */
    setTimeout(() => window.umk3.shot(), 600);
}

document.addEventListener('DOMContentLoaded', init);
