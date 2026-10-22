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

const isWin = (window.umk3 && window.umk3.platform) ? window.umk3.platform === 'win32' : true;

const T = {
    ES: {
        ui: 'ENGLISH', ttlSub: 'PORT DE PC',
        mBuild: 'Compilar tu .ipa', mVideo: 'Gráficos', mControls: 'Controles',
        mPlay: 'Jugar', mQuit: 'Salir', mProfile: 'Perfil',
        monitorLbl: 'MONITOR', monitorAuto: 'AUTOMÁTICO (EL PRINCIPAL)', monitorMain: 'PRINCIPAL',
        padIconsLbl: 'ICONOS DE BOTONES', padIcons: ['AUTOMÁTICO (LOS DEL MANDO)', 'XBOX', 'PLAYSTATION 5', 'PLAYSTATION 4', 'NINTENDO'],
        dProfile: 'Tu perfil de jugador: nombre, icono e ID de esta instalación. El juego lo edita también (Esc > PERFIL).',
        hProfile: 'PERFIL DE JUGADOR',
        profName: 'NOMBRE DE JUGADOR', profId: 'ID DE JUGADOR', profColor: 'COLOR DE FONDO', profIconLbl: 'ICONO',
        profLight: 'CLARO', profDark: 'OSCURO', profCustom: 'PERSONALIZADO', profShadeLbl: 'DEGRADADO', profColor2: 'HACIA EL COLOR', profColorLight: 'COLOR CLARO (ARRIBA)', profColorDark: 'COLOR OSCURO (ABAJO)',
        profDefault: 'JUGADOR', profReset: 'REINICIAR DATOS', profResetAsk: '¿SEGURO? PULSA OTRA VEZ',
        dBuild: 'El juego se compila en tu PC a partir de tu propio .ipa de UMK3 para iPhone (1.2.59).',
        dVideo: 'Resolución de ventana, resolución 3D, modo de pantalla y el marco de las barras.',
        dControls: 'Teclado y mando de los jugadores 1 y 2 (Xbox, PlayStation, Switch y otros), para 5 y 6 botones, y las teclas del modo debug.',
        dPlay: 'Arranca Ultimate Mortal Kombat 3: idioma del juego, intros y modo debug.',
        dQuit: 'Cierra el launcher.',
        hBuild: 'COMPILAR TU .IPA', ipaLbl: 'TU .IPA DE UMK3 (IPHONE 1.2.59)',
        archLbl: isWin ? 'WINDOWS' : 'LINUX', arches: isWin ? ['32 BITS (UMK3-GAME.EXE)', '64 BITS (UMK3-GAME64.EXE)'] : ['64 BITS (UMK3-GAME)'], playArchLbl: 'VERSIÓN A ABRIR',
        updCheck: 'BUSCAR ACTUALIZACIONES', updGet: 'DESCARGAR E INSTALAR', notesLbl: 'NOVEDADES DE LA ÚLTIMA VERSIÓN',
        updLooking: 'BUSCANDO ACTUALIZACIONES…', updNone: 'ESTÁS EN LA ÚLTIMA VERSIÓN', updNew: 'NUEVA VERSIÓN DISPONIBLE',
        updFail: 'NO SE PUDO CONSULTAR GITHUB', updYours: 'TU VERSIÓN', updDl: 'DESCARGANDO', updInst: 'INSTALANDO: EL LAUNCHER SE CERRARÁ Y VOLVERÁ A ABRIRSE…',
        updDone: 'ACTUALIZADO A', updRebuild: 'RECOMPILANDO EL JUEGO CON LA NUEVA VERSIÓN…', updFailed: 'LA ACTUALIZACIÓN NO SE PUDO COPIAR (VER update\\apply.log)', updNoNotes: 'SIN NOTAS.',
        browse: 'BUSCAR…', build: 'COMPILAR', rebuild: 'RECOMPILAR', cancel: 'CANCELAR', clear: 'QUITAR',
        builtAt: 'JUEGO COMPILADO', notBuilt: 'TODAVÍA NO ESTÁ COMPILADO: ELIGE TU .IPA Y PULSA COMPILAR.',
        gVideo: 'VIDEO', gTex: 'TEXTURAS', gUi: 'INTERFAZ', hGVideo: 'VIDEO', hGTex: 'TEXTURAS', hGUi: 'INTERFAZ',
        gScreen: 'PANTALLA', gImage: 'IMAGEN', gUpdates: 'ACTUALIZACIONES',
        texNote: 'IPHONE: LAS TEXTURAS REDUCIDAS DEL IPHONE (_LOW), COMO EL ORIGINAL. IPAD: LAS COMPLETAS. SIN LÍMITE: LAS COMPLETAS EN PNG, CON MIPMAPS Y FILTRO ANISÓTROPO. SE CAMBIA SIN REINICIAR DESDE EL MENÚ DEL JUEGO (ESC).',
        hVideo: 'GRÁFICOS', res: 'RESOLUCIÓN DE VENTANA', rres: 'RESOLUCIÓN 3D', mode: 'MODO DE PANTALLA',
        modes: ['VENTANA', 'PANTALLA COMPLETA', 'PANTALLA COMPLETA SIN BORDES'], sameRes: 'IGUAL QUE LA VENTANA', tabDbg: 'DEBUG', tabKb: 'TECLADO', tabPad: 'MANDO',
        aspect: 'RELACIÓN DE ASPECTO', aspects: ['3:2 (ORIGINAL, IPHONE)', '4:3 (IPAD)', '16:9 (PANORÁMICA)', '21:9 (ULTRA PANORÁMICA)'], aa: 'ANTIALIASING', aaOff: 'DESACTIVADO', texq: 'CALIDAD DE TEXTURAS', texqs: ['IPHONE (ACTUAL)', 'IPAD', 'SIN LÍMITE'], uisize: 'TAMAÑO DE LA INTERFAZ', uisizes: ['IPHONE (ACTUAL)', 'IPAD (HUD MÁS PEQUEÑO)'], hidectl: 'OCULTAR LOS CONTROLES EN PANTALLA (JOYSTICK Y BOTONES)',
        lang: 'IDIOMA DEL JUEGO', frame: 'MARCO (PANTALLA COMPLETA)',
        dbg: 'MODO DEBUG (MENÚ Y ATAJOS, VER CONTROLES)', hDbgKeys: 'TECLAS Y ATAJOS DEL MODO DEBUG', gDbgMenu: 'MENÚ DEBUG', gDbgFight: 'PELEA', gDbgScreens: 'PANTALLAS', dbgKeysNote: 'SOLO ACTÚAN CON EL MODO DEBUG ACTIVADO (SECCIÓN JUGAR).', wsNote: '4:3 ES LA VISTA DEL IPAD. EN 16:9 Y 21:9 LA PELEA OCUPA TODA LA PANTALLA (SE VE MÁS ESCENARIO, SIN ESTIRAR) Y LOS MENÚS QUEDAN EN 3:2. EN PANTALLA COMPLETA, EL MARCO RELLENA LAS BARRAS.',
        hControls: 'TECLADO DEL JUGADOR 1', hPad: 'MANDO DEL JUGADOR 1',
        hControls2: 'TECLADO DEL JUGADOR 2 (DUELO)', hPad2: 'MANDO DEL JUGADOR 2 (DUELO)',
        tabP1: 'JUGADOR 1', tabP2: 'JUGADOR 2', gMove: 'MOVIMIENTO', gButtons: 'BOTONES', gSystem: 'SISTEMA',
        schemes: 'EL JUEGO EMPIEZA CON EL ESQUEMA ELEGIDO; CADA UNO GUARDA SUS TECLAS. EN PAUSA SE PUEDE CAMBIAR DURANTE LA PARTIDA.',
        tab6: '6 BOTONES', tab5: '5 BOTONES', tabNote: 'EL JUEGO EMPIEZA CON ESTE ESQUEMA',
        reskey: 'RESTABLECER', press: 'PULSA UNA TECLA…  (ESC CANCELA)',
        hint: 'CLIC EN UNA ACCIÓN Y PULSA LA TECLA',
        hPlay: 'JUGAR', play: 'JUGAR', skip: 'SALTAR LAS INTROS (LOGOS DEL INICIO)',
        cSelect: 'SELECCIONAR', cBack: 'ATRÁS', cFull: 'PANTALLA COMPLETA',
        auto: isWin ? 'Automático (Windows)' : 'Automático (Linux)', original: '(IPHONE)',
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
        needIpa: 'Primero elige tu archivo .ipa de UMK3.', playFail: isWin ? 'No se pudo iniciar umk3-game.exe.' : 'No se pudo iniciar umk3-game.',
        buildDone: 'JUEGO COMPILADO. YA PUEDES JUGAR.', buildFail: 'FALLÓ LA COMPILACIÓN: MIRA EL REGISTRO.',
        notIpa: 'El archivo elegido no existe.',
        padDbgLbl: 'MANDO: ABRIR EL MENÚ DEBUG (PULSAR A LA VEZ)', padLbl: 'MANDO A USAR', gPad: 'BOTONES DEL MANDO',
        padOff: 'Ninguno (solo teclado)', padAuto: 'Mando: el primero que se conecte', padX: 'Mando de Xbox (XInput)',
        padSeen: isWin ? 'NINGÚN MANDO CONECTADO EN WINDOWS' : 'NINGÚN MANDO CONECTADO EN LINUX', padLook: 'BUSCANDO MANDOS…', padNow: 'CONECTADO', padGone: '(no conectado)',
        padNote: 'EL TECLADO SIGUE FUNCIONANDO CON EL MANDO. LOS MENÚS SE USAN CON EL RATÓN.',
        pads: { dir: 'Moverse', start: 'Pausa', select: 'Combos', settings: 'Abrir ajustes' },
    },
    EN: {
        ui: 'ESPAÑOL', ttlSub: 'PC PORT',
        mBuild: 'Compile your .ipa', mVideo: 'Graphics', mControls: 'Controls',
        monitorLbl: 'MONITOR', monitorAuto: 'AUTOMATIC (THE MAIN ONE)', monitorMain: 'MAIN',
        padIconsLbl: 'BUTTON ICONS', padIcons: ['AUTOMATIC (THE PAD\u2019S OWN)', 'XBOX', 'PLAYSTATION 5', 'PLAYSTATION 4', 'NINTENDO'],
        mPlay: 'Play', mQuit: 'Quit', mProfile: 'Profile',
        dProfile: 'Your player profile: name, icon and the ID of this installation. The game edits it too (Esc > PROFILE).',
        hProfile: 'PLAYER PROFILE',
        profName: 'PLAYER NAME', profId: 'PLAYER ID', profColor: 'BACKGROUND COLOUR', profIconLbl: 'ICON',
        profLight: 'LIGHT', profDark: 'DARK', profCustom: 'CUSTOM', profShadeLbl: 'GRADIENT', profColor2: 'INTO THE COLOUR', profColorLight: 'LIGHT COLOUR (TOP)', profColorDark: 'DARK COLOUR (BOTTOM)',
        profDefault: 'PLAYER', profReset: 'RESET DATA', profResetAsk: 'SURE? PRESS AGAIN',
        dBuild: 'The game is compiled on your PC from your own UMK3 iPhone .ipa (1.2.59).',
        dVideo: 'Window resolution, 3D resolution, display mode and the bars frame.',
        dControls: 'Players 1 and 2 keyboard and gamepad (Xbox, PlayStation, Switch and others), for 5 and 6 buttons, and debug mode\'s keys.',
        dPlay: 'Starts Ultimate Mortal Kombat 3: game language, intros and debug mode.',
        dQuit: 'Closes the launcher.',
        hBuild: 'COMPILE YOUR .IPA', ipaLbl: 'YOUR UMK3 .IPA (IPHONE 1.2.59)',
        archLbl: isWin ? 'WINDOWS' : 'LINUX', arches: isWin ? ['32 BITS (UMK3-GAME.EXE)', '64 BITS (UMK3-GAME64.EXE)'] : ['64 BITS (UMK3-GAME)'], playArchLbl: 'VERSION TO OPEN',
        updCheck: 'CHECK FOR UPDATES', updGet: 'DOWNLOAD AND INSTALL', notesLbl: 'WHAT IS NEW IN THE LATEST VERSION',
        updLooking: 'CHECKING FOR UPDATES…', updNone: 'YOU HAVE THE LATEST VERSION', updNew: 'NEW VERSION AVAILABLE',
        updFail: 'COULD NOT REACH GITHUB', updYours: 'YOUR VERSION', updDl: 'DOWNLOADING', updInst: 'INSTALLING: THE LAUNCHER WILL CLOSE AND OPEN AGAIN…',
        updDone: 'UPDATED TO', updRebuild: 'RECOMPILING THE GAME WITH THE NEW VERSION…', updFailed: 'THE UPDATE COULD NOT BE COPIED (SEE update\\apply.log)', updNoNotes: 'NO NOTES.',
        browse: 'BROWSE…', build: 'COMPILE', rebuild: 'REBUILD', cancel: 'CANCEL', clear: 'CLEAR',
        builtAt: 'GAME COMPILED', notBuilt: 'NOT COMPILED YET: CHOOSE YOUR .IPA AND PRESS COMPILE.',
        gVideo: 'VIDEO', gTex: 'TEXTURES', gUi: 'INTERFACE', hGVideo: 'VIDEO', hGTex: 'TEXTURES', hGUi: 'INTERFACE',
        gScreen: 'SCREEN', gImage: 'IMAGE', gUpdates: 'UPDATES',
        texNote: 'IPHONE: THE IPHONE\u2019S REDUCED TEXTURES (_LOW), AS THE ORIGINAL. IPAD: THE FULL ONES. NO LIMIT: THE FULL ONES AS PNG, WITH MIPMAPS AND ANISOTROPIC FILTERING. THE GAME\u2019S MENU (ESC) CHANGES IT WITHOUT RESTARTING.',
        hVideo: 'GRAPHICS', res: 'WINDOW RESOLUTION', rres: '3D RESOLUTION', mode: 'DISPLAY MODE',
        modes: ['WINDOW', 'FULLSCREEN', 'BORDERLESS FULLSCREEN'], sameRes: 'SAME AS THE WINDOW', tabDbg: 'DEBUG', tabKb: 'KEYBOARD', tabPad: 'GAMEPAD',
        aspect: 'ASPECT RATIO', aspects: ['3:2 (ORIGINAL, IPHONE)', '4:3 (IPAD)', '16:9 (WIDESCREEN)', '21:9 (ULTRAWIDE)'], aa: 'ANTIALIASING', aaOff: 'OFF', texq: 'TEXTURE QUALITY', texqs: ['IPHONE (CURRENT)', 'IPAD', 'NO LIMIT'], uisize: 'INTERFACE SIZE', uisizes: ['IPHONE (CURRENT)', 'IPAD (SMALLER HUD)'], hidectl: 'HIDE THE ON-SCREEN CONTROLS (STICK AND BUTTONS)',
        lang: 'GAME LANGUAGE', frame: 'FRAME (FULLSCREEN)',
        dbg: 'DEBUG MODE (MENU AND SHORTCUTS, SEE CONTROLS)', hDbgKeys: 'DEBUG MODE KEYS AND SHORTCUTS', gDbgMenu: 'DEBUG MENU', gDbgFight: 'FIGHT', gDbgScreens: 'SCREENS', dbgKeysNote: 'THEY ONLY ACT WITH DEBUG MODE ON (PLAY SECTION).', wsNote: '4:3 IS THE IPAD VIEW. IN 16:9 AND 21:9 THE FIGHT FILLS THE SCREEN (MORE OF THE STAGE, NO STRETCH) AND THE MENUS STAY 3:2. IN FULLSCREEN THE FRAME FILLS THE BARS.',
        hControls: 'PLAYER 1 KEYBOARD', hPad: 'PLAYER 1 GAMEPAD',
        hControls2: 'PLAYER 2 KEYBOARD (DUEL)', hPad2: 'PLAYER 2 GAMEPAD (DUEL)',
        tabP1: 'PLAYER 1', tabP2: 'PLAYER 2', gMove: 'MOVEMENT', gButtons: 'BUTTONS', gSystem: 'SYSTEM',
        schemes: 'THE GAME STARTS WITH THE CHOSEN LAYOUT; EACH KEEPS ITS OWN KEYS. THE PAUSE MENU CAN SWITCH IT DURING A GAME.',
        tab6: '6 BUTTONS', tab5: '5 BUTTONS', tabNote: 'THE GAME STARTS WITH THIS LAYOUT',
        reskey: 'RESET', press: 'PRESS A KEY…  (ESC CANCELS)',
        hint: 'CLICK AN ACTION, THEN PRESS A KEY',
        hPlay: 'PLAY', play: 'PLAY', skip: 'SKIP THE INTROS (STARTUP LOGOS)',
        cSelect: 'SELECT', cBack: 'BACK', cFull: 'FULLSCREEN',
        auto: isWin ? 'Automatic (Windows)' : 'Automatic (Linux)', original: '(IPHONE)',
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
        needIpa: 'Choose your UMK3 .ipa file first.', playFail: isWin ? 'Could not start umk3-game.exe.' : 'Could not start umk3-game.',
        buildDone: 'GAME COMPILED. YOU CAN PLAY NOW.', buildFail: 'BUILD FAILED: SEE THE LOG.',
        notIpa: 'The chosen file does not exist.',
        padDbgLbl: 'GAMEPAD: OPEN THE DEBUG MENU (HOLD TOGETHER)', padLbl: 'GAMEPAD TO USE', gPad: 'GAMEPAD BUTTONS',
        padOff: 'None (keyboard only)', padAuto: 'Gamepad: the first one connected', padX: 'Xbox pad (XInput)',
        padSeen: isWin ? 'NO GAMEPAD CONNECTED TO WINDOWS' : 'NO GAMEPAD CONNECTED TO LINUX', padLook: 'LOOKING FOR GAMEPADS…', padNow: 'CONNECTED', padGone: '(not connected)',
        padNote: 'THE KEYBOARD STILL WORKS WITH A PAD. THE MENUS ARE USED WITH THE MOUSE.',
        pads: { dir: 'Move', start: 'Pause', select: 'Moves', settings: 'Open settings' },
    },
};

/* The pad's buttons for each action (umk3.ini pad_<name>= / pad5_<name>=,
 * runtime/game_main.c): up to two buttons each, either one presses it. The
 * defaults are the game's own, by position. "5:x" is the five-button
 * layout's; special, pause and moves serve both layouts. */
const PAD_ROWS = {
    6: ['hp', 'lp', 'hk', 'lk', 'block', 'run', 'special', 'pause', 'moves', 'settings'],
    5: ['5:p', '5:k', '5:b', '5:r', 'special', 'pause', 'moves', 'settings'],
};
const PAD_DEFAULT = {
    hp: 'WEST', lp: 'SOUTH', block: 'R1', hk: 'NORTH', lk: 'EAST', run: 'L1+L2',
    special: 'R2', pause: 'START', moves: 'SELECT', settings: 'HOME',
};
const PAD5_DEFAULT = { p: 'WEST+NORTH', b: 'R1', k: 'SOUTH+EAST', r: 'L1+L2' };
/* The player the KEYBOARD and GAMEPAD tabs edit: 1, or 2 (DUELO, umk3.ini
 * p2key_*, p2pad*). */
let pl = 1;
const PK = () => (pl === 2 ? 'p2PadKeys' : 'padKeys');
function padGet(act) {
    if (act.startsWith('5:')) {
        const v = cfg[PK() + '5'] && cfg[PK() + '5'][act.slice(2)];
        return v === undefined ? PAD5_DEFAULT[act.slice(2)] : v;
    }
    const v = cfg[PK()] && cfg[PK()][act];
    return v === undefined ? PAD_DEFAULT[act] : v;
}
function padSet(act, v) {
    selectBlip();
    const k = act.startsWith('5:') ? PK() + '5' : PK();
    const n = act.startsWith('5:') ? act.slice(2) : act;
    cfg[k] = Object.assign({}, cfg[k], { [n]: v });
    save({ [k]: { [n]: v } });
}
const padWant = () => (pl === 2 ? cfg.p2Pad : cfg.pad) || 'auto';

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
function fillPadDbg() {
    const box = $('padDbgSlots');
    box.innerHTML = '';
    padSlots(box, cfg.padDbgMenu || '', padKindOf(1), v => {
        cfg.padDbgMenu = v;
        save({ padDbgMenu: v });
        fillPadDbg();
    });
}

function fillPads() {
    fillPadDbg();
    const s = $('pad');
    if (!s || !cfg) return;
    const opts = [['off', T[ui].padOff], ['auto', T[ui].padAuto],
                  ['xinput', seenPads.xinput ? T[ui].padX + ': ' + seenPads.xinput : T[ui].padX]];
    for (const [v, n] of Object.entries(seenPads))
        if (v !== 'xinput') opts.push([v, n + '  (' + v.toUpperCase() + ')']);
    const want = padWant(), wantName = pl === 2 ? cfg.p2PadName : cfg.padName;
    if (!opts.some(o => o[0] === want))
        opts.push([want, (wantName || want.toUpperCase()) + '  ' + T[ui].padGone]);
    s.innerHTML = '';
    for (const [v, n] of opts) {
        const o = el('option', null, (seenPads[v] ? '● ' : '') + n);   /* ● = connected */
        o.value = v;
        s.appendChild(o);
    }
    s.value = want;
    const any = Object.keys(seenPads).length;
    $('padSeen').textContent = any ? T[ui].padNow + ': ' + Object.values(seenPads).join(', ') : T[ui].padSeen;
    const ic = $('padIcons');
    ic.innerHTML = '';
    T[ui].padIcons.forEach((n, i) => {
        const o = el('option', null, n);
        o.value = PAD_ICON_NAMES[i];
        ic.appendChild(o);
    });
    ic.value = (pl === 2 ? cfg.p2PadIcons : cfg.padIcons) || 'auto';
    paintPadMap();
}
function paintPadMap() {
    const on = padWant() !== 'off';
    $('padMap').classList.toggle('hidden', !on);
    $('respad').classList.toggle('hidden', !on || ctlTab !== 'pad');
    const b = $('k-pad');
    b.innerHTML = '';
    const dir = el('div', 'keybtn ro');
    dir.appendChild(el('span', 'kname', T[ui].pads.dir));
    const kind = padKindOf(pl);
    const dk = el('span', 'kkey');
    for (const f of ['Dpad', kind === 'xbox' ? 'L' : kind === 'nintendo' ? 'L' : 'L3']) {
        const im = glyphImg(PAD_SETS[kind].dir + '/' + PAD_SETS[kind].extra[f]);
        if (im) dk.appendChild(im);
    }
    if (!dk.childNodes.length) dk.textContent = 'D-PAD / L-STICK';
    dir.appendChild(dk);
    b.appendChild(dir);
    for (const act of PAD_ROWS[cfg.buttons === 6 ? 6 : 5]) {
        const k = el('div', 'keybtn ro');
        const name = act === 'pause' ? T[ui].pads.start : act === 'moves' ? T[ui].pads.select
                   : act === 'settings' ? T[ui].pads.settings : T[ui].keys[act];
        k.appendChild(el('span', 'kname', name));
        const box = el('span', 'padsels');
        padSlots(box, padGet(act), kind, v => { padSet(act, v); paintPadMap(); });
        k.appendChild(box);
        b.appendChild(k);
    }
}

/* ------------------------------------------------------------ glyphs --
 * The key and button pictures (Julio Cacko's Free Input Prompts, CC0), the
 * same the game's menus draw (runtime/platform/win32_gl.c): the Vintage
 * keyboard, the Retro Xbox and PlayStation 5 pads, the Stylized PlayStation
 * 4 and the Default Nintendo ones. */
let GLYPH = {};
function glyphImg(name) {
    if (!GLYPH[name]) return null;
    const im = el('img', 'glyph');
    im.src = GLYPH[name];
    im.alt = '';
    im.draggable = false;
    return im;
}
/* a virtual key's picture: kb/T_<file>_Key_Vintage */
const KB_FILES = {
    8: 'BackSpace', 9: 'Tab', 13: 'Enter', 16: 'Shift', 17: 'Crtl', 18: 'Alt', 20: 'CapsLock',
    27: 'Esc', 32: 'Space', 33: 'PageUp', 34: 'PageDown', 35: 'End', 36: 'Home',
    37: 'Left', 38: 'Up', 39: 'Right', 40: 'Down', 44: 'PrtScrn', 45: 'Ins', 46: 'Del',
    106: 'Asterisk', 107: 'Plus', 109: 'Minus', 110: 'Del', 111: 'Slash', 144: 'NumLock',
    186: 'Semicolon', 187: 'Plus', 189: 'Minus', 191: 'Slash', 192: 'Tilde',
    219: 'Brackets_L', 221: 'Brackets_R', 222: 'Quotation',
};
function kbFile(vk) {
    if ((vk >= 65 && vk <= 90) || (vk >= 48 && vk <= 57)) return String.fromCharCode(vk);
    if (vk >= 96 && vk <= 105) return String(vk - 96);
    if (vk >= 112 && vk <= 123) return 'F' + (vk - 111);
    return KB_FILES[vk] || null;
}
function keyFace(vk) {
    const f = kbFile(vk), span = el('span', 'kkey');
    const im = f && glyphImg('kb/T_' + f + '_Key_Vintage');
    if (!im) {
        span.textContent = vkLabel(vk);
        return span;
    }
    if (vk >= 96 && vk <= 111) span.appendChild(el('span', 'knum', 'NUM'));
    span.appendChild(im);
    return span;
}
/* The game's PB_* buttons, by position, and each make's pictures. */
const PB = ['SOUTH', 'EAST', 'WEST', 'NORTH', 'L1', 'R1', 'L2', 'R2', 'SELECT', 'START', 'L3', 'R3', 'HOME'];
const PAD_ICON_NAMES = ['auto', 'xbox', 'ps5', 'ps4', 'nintendo'];
const PAD_SETS = {
    xbox: { dir: 'xb', files: ['T_X_A_Color_Retro', 'T_X_B_Color_Retro', 'T_X_X_Color_Retro',
        'T_X_Y_Color_Retro', 'T_X_LB_Retro', 'T_X_RB_Retro', 'T_X_LT_Retro', 'T_X_RT_Retro',
        'T_X_Share_Retro', 'T_X_X_Retro', 'T_X_Left_Stick_Click_Retro', 'T_X_Right_Stick_Click_Retro', null],
        names: { HOME: 'XBOX' }, extra: { Dpad: 'T_X_Dpad_Retro', L: 'T_X_L_Retro' } },
    ps5: { dir: 'ps', files: ['T_P5_Cross_Color_Retro', 'T_P5_Circle_Color_Retro',
        'T_P5_Square_Color_Retro', 'T_P5_Triangle_Color_Retro', 'T_P5_L1_Retro', 'T_P5_R1_Retro',
        'T_P5_L2_Retro', 'T_P5_R2_Retro', 'T_P5_Share_Retro', 'T_P5_Options_Retro',
        'T_P5_L3_Retro', 'T_P5_R3_Retro', null],
        names: { HOME: 'PS' }, extra: { Dpad: 'T_P5_Dpad_Retro', L3: 'T_P5_L_Retro' } },
    ps4: { dir: 'p4', files: ['T_P4_Cross_Color_Stylized', 'T_P4_Circle_Color_Stylized',
        'T_P4_Square_Color_Stylized', 'T_P4_Triangle_Color_Stylized', 'T_P4_L1_Stylized',
        'T_P4_R1_Stylized', 'T_P4_L2_Stylized', 'T_P4_R2_Stylized', 'T_P4_Share_Stylized',
        'T_P4_Options_Stylized', 'T_P4_L3_Stylized', 'T_P4_R3_Stylized', null],
        names: { HOME: 'PS' }, extra: { Dpad: 'T_P4_Dpad_Stylized', L3: 'T_P4_l_Stylized' } },
    /* by position: a Nintendo pad's south button is its B */
    nintendo: { dir: 'ns', files: ['T_S_B', 'T_S_A', 'T_S_Y', 'T_S_X', 'T_S_LB', 'T_S_RB',
        'T_S_LT', 'T_S_RT', 'T_S_Minus', 'T_S_Plus', 'T_S_L', 'T_S_R', 'T_S_Home'],
        names: {}, extra: { Dpad: 'T_S_Dpad', L: 'T_S_L' } },
};
/* A pad= value's make: 054c Sony (the DualShock 4's products apart), 057e
 * Nintendo, anything else Xbox. */
function padMake(dev, id) {
    const s = String(dev || '') + ' ' + String(id || '');
    if (/054c[:\s-]*(product:\s*)?(05c4|09cc|0ba0)/i.test(s)) return 'ps4';
    if (/054c/i.test(s)) return 'ps5';
    if (/057e/i.test(s)) return 'nintendo';
    return 'xbox';
}
/* The pictures for a player's pad: pad_icons= / p2pad_icons=, else the
 * pad's own make (the chosen one, or for auto the one connected). */
function padKindOf(p) {
    const st = (p === 2 ? cfg.p2PadIcons : cfg.padIcons) || 'auto';
    if (st !== 'auto' && PAD_SETS[st]) return st;
    const dev = (p === 2 ? cfg.p2Pad : cfg.pad) || 'auto';
    if (/^[0-9a-f]{4}:/i.test(dev)) return padMake(dev);
    const gp = livePad();
    return gp ? padMake('', gp.id) : 'xbox';
}
function padFace(kind, b) {
    const set = PAD_SETS[kind] || PAD_SETS.xbox, i = PB.indexOf(b);
    const im = i >= 0 && set.files[i] && glyphImg(set.dir + '/' + set.files[i]);
    return im || el('span', 'gname', set.names[b] || b);
}
function comboFace(kind, v, into) {
    const parts = String(v || '').split('+').filter(Boolean);
    if (!parts.length) into.appendChild(el('span', 'gname', '—'));
    parts.forEach((b, i) => {
        if (i) into.appendChild(el('span', 'gplus', '+'));
        into.appendChild(padFace(kind, b));
    });
}
/* Two slots, either button of the two presses it; each opens the picker. */
function padSlots(box, value, kind, onChange) {
    const cur = String(value || '').split('+').filter(Boolean);
    [0, 1].forEach(i => {
        const s = el('button', 'padslot');
        comboFace(kind, cur[i] || '', s);
        s.addEventListener('click', ev => {
            ev.stopPropagation();
            selectBlip();
            openPicker(s, kind, b => {
                const next = cur.slice();
                next[i] = b;
                onChange(next.filter(Boolean).filter((x, j, a) => a.indexOf(x) === j).join('+'));
            });
        });
        box.appendChild(s);
    });
}
function closePicker() {
    const p = $('picker');
    if (p) p.remove();
}
function openPicker(anchor, kind, pick) {
    closePicker();
    const box = el('div', 'picker');
    box.id = 'picker';
    for (const b of [''].concat(PB)) {
        const o = el('button', 'pick');
        if (b) o.appendChild(padFace(kind, b));
        else o.appendChild(el('span', 'gname', '—'));
        o.title = b || '—';
        o.addEventListener('click', ev => {
            ev.stopPropagation();
            selectBlip();
            closePicker();
            pick(b);
        });
        box.appendChild(o);
    }
    document.body.appendChild(box);
    const r = anchor.getBoundingClientRect(), w = box.offsetWidth, h = box.offsetHeight;
    box.style.left = Math.max(4, Math.min(window.innerWidth - w - 4, r.right - w)) + 'px';
    box.style.top = (r.bottom + h + 4 < window.innerHeight ? r.bottom + 4 : r.top - h - 4) + 'px';
}
document.addEventListener('click', closePicker);

function paintGfxTab(g) {
    document.querySelectorAll('.tab[data-g]').forEach(t => t.classList.toggle('active', t.dataset.g === g));
    document.querySelectorAll('.gpage').forEach(p => p.classList.toggle('hidden', p.dataset.g !== g));
}

/* The menu's selection, the band and the orb, slides to the chosen line
 * instead of jumping there. */
function moveMenuSel() {
    const a = document.querySelector('.mi.active'), s = $('menuSel');
    if (!a || !s) return;
    const orb = a.querySelector('.orb');
    const x = orb ? orb.offsetLeft : a.offsetLeft;
    s.style.transform = 'translate(' + x + 'px, ' + a.offsetTop + 'px)';
    s.style.width = (a.offsetLeft + a.offsetWidth - x) + 'px';
    s.style.height = a.offsetHeight + 'px';
    s.classList.add('on');
}
window.addEventListener('resize', moveMenuSel);

/* ------------------------------------------------------------ ring --
 * The backdrop's rings: thick segments of dark blue marble with white rims
 * that glow, a big ring round the whole window and a smaller one behind the
 * title, both turning slowly, and the circuit traces under them. In a
 * 1600 x 900 box the window crops to cover. The marble is made once, here,
 * as a picture (a tiling turbulence, no game art) and the segments are
 * filled with it, so it turns with them and costs nothing per frame. */
function marbleURL(S) {
    let seed = 11;
    const rnd = () => {                 /* mulberry32 */
        seed = (seed + 0x6D2B79F5) | 0;
        let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
    const oct = [];
    for (let o = 0, P = 4; o < 5; o++, P *= 2) {
        const v = new Float32Array(P * P);
        for (let i = 0; i < v.length; i++) v[i] = rnd();
        oct.push({ P, v });
    }
    const sm = t => t * t * (3 - 2 * t);
    const noise = ({ P, v }, x, y) => {     /* tiles: the lattice wraps */
        const fx = x * P, fy = y * P, ix = Math.floor(fx), iy = Math.floor(fy);
        const tx = sm(fx - ix), ty = sm(fy - iy);
        const at = (a, b) => v[((b % P) + P) % P * P + ((a % P) + P) % P];
        const a = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * tx;
        const b = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * tx;
        return a + (b - a) * ty;
    };
    const TABLE = [0, 0.04, 0.1, 0.22, 0.55, 1];
    const cv = document.createElement('canvas');
    cv.width = cv.height = S;
    const g = cv.getContext('2d'), im = g.createImageData(S, S);
    for (let y = 0; y < S; y++)
        for (let x = 0; x < S; x++) {
            let t = 0, w = 0.5;
            for (const o of oct) {
                t += Math.abs(noise(o, x / S, y / S) * 2 - 1) * w;
                w *= 0.5;
            }
            t = Math.min(1, t * 1.05);
            const k = t * (TABLE.length - 1), i = Math.min(TABLE.length - 2, Math.floor(k));
            const v = TABLE[i] + (TABLE[i + 1] - TABLE[i]) * (k - i);
            const p = (y * S + x) * 4;
            im.data[p] = (0.55 * v + 0.02) * 255;
            im.data[p + 1] = (0.62 * v + 0.05) * 255;
            im.data[p + 2] = Math.min(1, 0.85 * v + 0.17) * 255;
            im.data[p + 3] = 255;
        }
    g.putImageData(im, 0, 0);
    return cv.toDataURL('image/png');
}

function buildRing() {
    const svg = $('ring'), NS = 'http://www.w3.org/2000/svg';
    const mk = (tag, attrs, parent) => {
        const n = document.createElementNS(NS, tag);
        for (const [k, v] of Object.entries(attrs)) n.setAttribute(k, v);
        (parent || svg).appendChild(n);
        return n;
    };
    const defs = mk('defs', {});
    const pat = mk('pattern', { id: 'marble', patternUnits: 'userSpaceOnUse',
                                width: '512', height: '512' }, defs);
    mk('image', { href: marbleURL(512), width: '512', height: '512' }, pat);

    /* the circuit traces: dark grooves with a faint light edge */
    const traces = ['M0 250 H260 L330 320 H640', 'M0 610 H180 L260 530 H560 L640 450',
        'M1600 170 H1380 L1310 240 H1120', 'M1600 690 H1400 L1320 610 H1150',
        'M380 900 L480 800 H860 L920 740', 'M0 700 H90 L140 650 H300',
        'M1600 420 H1500 L1450 470 H1260', 'M760 0 L820 60 H1040'];
    const tg = mk('g', { class: 'traces' });
    for (const d of traces) {
        mk('path', { d, class: 'trace' }, tg);
        mk('path', { d, class: 'trace-lit', transform: 'translate(0 -3)' }, tg);
    }

    const rad = a => a * Math.PI / 180;
    const seg = (cx, cy, r0, r1, a0, a1) => {
        const p = (r, a) => (cx + r * Math.cos(rad(a))).toFixed(1) + ' ' + (cy + r * Math.sin(rad(a))).toFixed(1);
        const big = a1 - a0 > 180 ? 1 : 0;
        return 'M' + p(r1, a0) + ' A' + r1 + ' ' + r1 + ' 0 ' + big + ' 1 ' + p(r1, a1) +
               ' L' + p(r0, a1) + ' A' + r0 + ' ' + r0 + ' 0 ' + big + ' 0 ' + p(r0, a0) + ' Z';
    };
    const ring = (cx, cy, r0, r1, spans, cls) => {
        const g = mk('g', { class: cls });
        g.style.transformOrigin = cx + 'px ' + cy + 'px';  /* it turns on its centre */
        for (const [a0, a1] of spans) {
            const d = seg(cx, cy, r0, r1, a0, a1);
            mk('path', { d, class: 'seg-body' }, g);
            /* the rim's glow: wide faint strokes under the sharp one */
            mk('path', { d, class: 'seg-glow2' }, g);
            mk('path', { d, class: 'seg-glow' }, g);
            mk('path', { d, class: 'seg-rim' }, g);
        }
    };
    /* the big ring: open at the top, where the title is, and at the bottom */
    ring(820, 520, 690, 790, [[-140, -110], [-68, -26], [-18, 22], [30, 70],
                              [110, 150], [158, 198]], 'ring-big');
    /* the small one, behind the title and the panel */
    const small = [];
    for (let a = -100; a < 250; a += 44) small.push([a, a + 36]);
    ring(1060, 400, 300, 342, small, 'ring-small');
}

/* ---------------------------------------------------- the pad, here --
 * The launcher follows a pad too: up / down the menu, south to pick, east
 * back. The command bar shows the pictures of whichever was used last,
 * the pad's or the keyboard's -- not both. */
let usingPad = false, padWas = [];
function livePad() {
    const all = navigator.getGamepads ? Array.from(navigator.getGamepads()).filter(Boolean) : [];
    return all.find(g => g.connected) || null;
}
function paintCmdbar() {
    const kind = padKindOf(1);
    const put = (id, node) => { const s = $(id); s.innerHTML = ''; s.appendChild(node); };
    if (usingPad) {
        put('cgA', padFace(kind, 'SOUTH'));
        put('cgB', padFace(kind, 'EAST'));
    } else {
        put('cgA', keyFace(13));
        put('cgB', keyFace(27));
        put('cgF', keyFace(122));
    }
    $('cgF').classList.toggle('hidden', usingPad);
    $('cgFl').classList.toggle('hidden', usingPad);
}
function useDevice(pad) {
    if (pad === usingPad) return;
    usingPad = pad;
    paintCmdbar();
}
function padTick() {
    const g = livePad();
    if (g) {
        const b = g.buttons.map(x => x.pressed), ay = g.axes[1] || 0;
        b[12] = b[12] || ay < -0.6;
        b[13] = b[13] || ay > 0.6;
        const hit = i => b[i] && !padWas[i];
        if (b.some(Boolean)) useDevice(true);
        if (!$('picker') && !waitingKey) {
            if (hit(12)) navKey('ArrowUp');
            if (hit(13)) navKey('ArrowDown');
            if (hit(0)) navKey('Enter');
            if (hit(1)) navKey('Escape');
        }
        padWas = b;
    } else if (usingPad)
        useDevice(false);
    requestAnimationFrame(padTick);
}
window.addEventListener('gamepadconnected', () => { paintKeys(); fillPads(); });
window.addEventListener('keydown', () => useDevice(false), true);
window.addEventListener('mousemove', () => useDevice(false));

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
/* arch= in umk3.ini: which game COMPILAR builds and JUGAR opens, 32 bits
 * (umk3-game.exe) or 64 bits (umk3-game64.exe); both can be kept */
const ARCH = isWin ? ['x86', 'x64'] : ['x64'];
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
    'k-system': () => (pl === 2 ? [] : ['pause', 'moves']),
    'k-dbgmenu': () => ['dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next'],
    'k-dbgfight': () => ['dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose'],
    'k-dbgscr': () => ['dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu'],
};
const DBG_KEYS = ['dbg_menu', 'dbg_info', 'dbg_page_prev', 'dbg_page_next',
    'dbg_ko_p2', 'dbg_ko_p1', 'dbg_win', 'dbg_lose',
    'dbg_scr_prev', 'dbg_scr_next', 'dbg_scr_menu'];
const KEY5_DEFAULT = { p: 85, b: 79, k: 74, r: 76 };
/* Player two's defaults: the game's second set, arrows and the keypad. */
const P2_KEY_DEFAULT = {
    up: 38, down: 40, left: 37, right: 39,
    hp: 103, lp: 104, block: 105, hk: 100, lk: 101, run: 102, special: 96,
};
const P2_KEY5_DEFAULT = { p: 103, b: 105, k: 100, r: 102 };
const KEYS_OF = n => (pl === 2 && !DBG_KEYS.includes(n) ? 'p2Keys' : 'keys');
function keyGet(name) {
    if (name.startsWith('5:')) {
        const d = pl === 2 ? P2_KEY5_DEFAULT : KEY5_DEFAULT, k = pl === 2 ? 'p2Keys5' : 'keys5';
        return (cfg[k] && cfg[k][name.slice(2)]) || d[name.slice(2)];
    }
    const k = KEYS_OF(name);
    return (cfg[k] && cfg[k][name]) || (k === 'p2Keys' ? P2_KEY_DEFAULT : KEY_DEFAULT)[name];
}
function keySet(name, vk) {
    const k = name.startsWith('5:') ? (pl === 2 ? 'p2Keys5' : 'keys5') : KEYS_OF(name);
    const n = name.startsWith('5:') ? name.slice(2) : name;
    cfg[k] = Object.assign({}, cfg[k], { [n]: vk });
    save({ [k]: { [n]: vk } });
}
const KEY_DEFAULT = {
    up: 87, down: 83, left: 65, right: 68,
    hp: 85, lp: 73, block: 79, hk: 74, lk: 75, run: 76,
    pause: 80, moves: 77, special: 72,
    dbg_menu: 113, dbg_info: 114, dbg_page_prev: 81, dbg_page_next: 69,
    dbg_ko_p2: 120, dbg_ko_p1: 121, dbg_win: 122, dbg_lose: 123,
    dbg_scr_prev: 117, dbg_scr_next: 118, dbg_scr_menu: 119,
};
const SECTIONS = ['build', 'video', 'controls', 'profile', 'play', 'quit'];
const DESC = { build: 'dBuild', video: 'dVideo', controls: 'dControls', profile: 'dProfile', play: 'dPlay', quit: 'dQuit' };

const $ = id => document.getElementById(id);
const el = (tag, cls, text) => {
    const n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text !== undefined) n.textContent = text;
    return n;
};

let cfg = null, ready = false, builtAt = null, building = false, both = false;
let ui = 'EN', waitingKey = null, sec = 'build';

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
    $('arch').disabled = building;          /* the build in progress keeps its exe */
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
    moveMenuSel();
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
    document.querySelectorAll('section[data-sec="controls"] .tab').forEach(t =>
        t.classList.toggle('active', t.dataset.c ? t.dataset.c === ctlTab
            : t.dataset.pl ? Number(t.dataset.pl) === pl
            : Number(t.dataset.b) === (cfg.buttons === 6 ? 6 : 5)));
    $('hCtlP').textContent = tx(pl === 2 ? 'hControls2' : 'hControls');
    $('hCtlM').textContent = tx(pl === 2 ? 'hPad2' : 'hPad');
    $('ctlPlayers').classList.toggle('hidden', ctlTab === 'dbg');
    $('k-system').parentNode.classList.toggle('hidden', pl === 2);
    for (const [id, on] of [['hCtlP', 'p'], ['ctlP', 'p'], ['reskeys', 'p'],
                            ['hCtlM', 'pad'], ['ctlM', 'pad'], ['respad', 'pad'],
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
            k.appendChild(keyFace(vk));
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
    $('playarch').innerHTML = '';
    T[ui].arches.forEach(n => $('playarch').appendChild(el('option', null, n)));
    $('arch').innerHTML = '';
    T[ui].arches.forEach(n => $('arch').appendChild(el('option', null, n)));
    $('texq').innerHTML = '';
    T[ui].texqs.forEach(n => $('texq').appendChild(el('option', null, n)));
    $('uisize').innerHTML = '';
    T[ui].uisizes.forEach(n => $('uisize').appendChild(el('option', null, n)));
    lang.innerHTML = '';
    LANGS.forEach(n => lang.appendChild(el('option', null, n || T[ui].auto)));
}

/* The monitors the game sees (`umk3-game --list-monitors`); before the
 * game is compiled only AUTOMATIC. */
let monitors = [];
async function fillMonitors() {
    try { monitors = await window.umk3.listMonitors(); } catch (err) { monitors = []; }
    paintMonitors();
}
function paintMonitors() {
    const s = $('monitor');
    s.innerHTML = '';
    const auto = el('option', null, T[ui].monitorAuto);
    auto.value = '0';
    s.appendChild(auto);
    for (const m of monitors) {
        const o = el('option', null, T[ui].monitorLbl + ' ' + m.name.replace(' *', '  (' + T[ui].monitorMain + ')'));
        o.value = String(m.value);
        s.appendChild(o);
    }
    if (cfg.monitor && !monitors.some(m => m.value === cfg.monitor)) {
        const o = el('option', null, T[ui].monitorLbl + ' ' + cfg.monitor);
        o.value = String(cfg.monitor);
        s.appendChild(o);
    }
    s.value = String(cfg.monitor || 0);
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
    $('arch').selectedIndex = Math.max(0, ARCH.indexOf(cfg.arch || (isWin ? 'x86' : 'x64')));
    $('playarch').selectedIndex = $('arch').selectedIndex;
    $('uisize').selectedIndex = Math.max(0, UISIZE.indexOf(cfg.uiSize || 'iphone'));
    $('dbg').checked = !!cfg.debug;
    $('skip').checked = !!cfg.skipIntro;
    $('hidectl').checked = !!cfg.hideControls;
    paintMonitors();
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
    both = !!info.both;
    /* both builds compiled: JUGAR picks which one opens */
    $('playarchf').classList.toggle('hidden', !both);
    if (info.version)                   /* 0.1.3-beta: "0.1.3 beta" */
        $('ttl-ver').textContent = info.version.replace('-', ' ');
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

/* ------------------------------------------------------------ profiles -- */

/* profiles.ini (lib/profiles.js), shared with the game's settings menu. The
 * icon: a fighter's bio and endings art (the game's own <NAME>_VERSUS.PNG,
 * read from res\Textures, nothing imported) over a glossy Frutiger Aero
 * gradient of the profile's colour. runtime/profiles.c draws the same icon
 * with the same numbers. */
/* The palette, the same as the game's settings menu (settings_menu.c,
 * pal_make): 15 hues across and a grey column, seven shades from light to
 * dark; the colour picker beside it gives any other. */
function hslHex(h, s, l) {
    const c = (1 - Math.abs(2 * l - 1)) * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = l - c / 2;
    const [r, g, b] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x]
                    : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
    return [r, g, b].map(v => Math.floor((v + m) * 255 + 0.5).toString(16).padStart(2, '0')).join('').toUpperCase();
}
const PROF_COLORS = [];
{
    const L = [0.88, 0.76, 0.64, 0.52, 0.40, 0.28, 0.16];
    const GREY = ['FFFFFF', 'D4D4D4', 'AAAAAA', '808080', '555555', '2B2B2B', '000000'];
    L.forEach((l, r) => {
        for (let c = 0; c < 15; c++) PROF_COLORS.push(hslHex(c * 24, 0.85, l));
        PROF_COLORS.push(GREY[r]);
    });
}
let profArt = [];

function hexRgb(h) {
    const n = parseInt(h, 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
const mix = (a, b, t) => a.map((v, i) => Math.round(v + (b[i] - v) * t));
const css = (c, a) => 'rgba(' + c.join(',') + ',' + (a === undefined ? 1 : a) + ')';

function roundRect(g, x, y, w, h, r) {
    g.beginPath();
    g.moveTo(x + r, y);
    g.arcTo(x + w, y, x + w, y + h, r);
    g.arcTo(x + w, y + h, x, y + h, r);
    g.arcTo(x, y + h, x, y, r);
    g.arcTo(x, y, x + w, y, r);
    g.closePath();
}

function drawIcon(cv, icon, color, shade, color2) {
    const g = cv.getContext('2d'), S = cv.width, c = hexRgb(color);
    const white = [255, 255, 255], black = [0, 0, 0];
    const dark = shade === 'D', o = hexRgb(color2 || (dark ? '000000' : 'FFFFFF'));
    const c2 = shade === 'C' ? o : null;

    g.clearRect(0, 0, S, S);
    g.save();
    roundRect(g, 0, 0, S, S, S * 0.18);
    g.clip();
    /* the sky: light at the top, the colour, deeper at the bottom */
    let gr = g.createLinearGradient(0, 0, 0, S);
    /* light: the second colour (white unless chosen) into the colour,
     * deeper at the bottom; dark: the colour at the top going down to the
     * second (black unless chosen); custom: the colour to the second
     * (profiles.c, the same numbers) */
    gr.addColorStop(0, css(c2 ? mix(c, white, 0.2) : dark ? mix(c, white, 0.15) : mix(c, o, 0.55)));
    gr.addColorStop(0.5, css(c2 ? mix(c, c2, 0.5) : dark ? mix(c, o, 0.45) : c));
    gr.addColorStop(1, css(c2 ? c2 : dark ? mix(c, o, 0.92) : mix(c, black, 0.3)));
    g.fillStyle = gr;
    g.fillRect(0, 0, S, S);
    /* the glow rising from the bottom */
    gr = g.createRadialGradient(S / 2, S * 1.05, 0, S / 2, S * 1.05, S * 0.6);
    const gc = c2 ? mix(c2, white, 0.3) : dark ? c : mix(c, o, 0.6);
    gr.addColorStop(0, css(gc, c2 ? 0.45 : dark ? 0.35 : 0.75));
    gr.addColorStop(1, css(gc, 0));
    g.fillStyle = gr;
    g.fillRect(0, 0, S, S);
    /* the fighter */
    if (profArt[icon])
        g.drawImage(profArt[icon], 0, 0, S, S);
    /* the gloss: a white bubble over the top half */
    gr = g.createLinearGradient(0, 0, 0, S * 0.5);
    gr.addColorStop(0, 'rgba(255,255,255,' + (c2 ? 0.45 : dark ? 0.35 : 0.55) + ')');
    gr.addColorStop(1, 'rgba(255,255,255,' + (c2 ? 0.04 : dark ? 0.03 : 0.04) + ')');
    g.fillStyle = gr;
    g.beginPath();
    g.ellipse(S / 2, 0, S * 0.72, S * 0.5, 0, 0, Math.PI * 2);
    g.fill();
    g.restore();
    /* the rim */
    roundRect(g, S * 0.0125, S * 0.0125, S * 0.975, S * 0.975, S * 0.17);
    g.lineWidth = S * 0.025;
    g.strokeStyle = 'rgba(255,255,255,0.7)';
    g.stroke();
}

async function loadProfArt() {
    for (let i = 0; i < 24; i++) {
        if (profArt[i]) continue;
        const url = await window.umk3.profileIcon(i);
        if (!url) continue;
        const im = new Image();
        im.src = url;
        await im.decode().catch(() => {});
        profArt[i] = im;
    }
}

let prof = null, profResetArmed = false;

async function updateProf(fields) {
    prof = await window.umk3.updateProfile(fields);
    paintProfile();
}

function paintProfile() {
    const p = prof;
    if (!p) return;
    if (document.activeElement !== $('profName')) $('profName').value = p.name;
    $('profId').textContent = p.id;
    $('profReset').textContent = tx(profResetArmed ? 'profResetAsk' : 'profReset');
    $('profColor').value = '#' + p.color.toLowerCase();
    $('profColor2').value = '#' + p.color2.toLowerCase();
    $('profColor2Lbl').textContent = tx(p.shade === 'C' ? 'profColor2' : p.shade === 'D' ? 'profColorDark' : 'profColorLight');
    drawIcon($('profIcon'), p.icon, p.color, p.shade, p.color2);
    drawIcon($('profBadgeIcon'), p.icon, p.color, p.shade, p.color2);
    $('profBadgeName').textContent = p.name || tx('profDefault');
    $('profBadge').classList.remove('hidden');
    document.querySelectorAll('#profShade .tab').forEach(t =>
        t.classList.toggle('active', t.dataset.shade === p.shade));
    const swatches = (box, cur, key) => {
        box.innerHTML = '';
        PROF_COLORS.forEach(c => {
            const b = el('button', 'swatch' + (c === cur ? ' on' : ''));
            b.style.background = '#' + c;           /* CSSOM: allowed by the CSP */
            b.addEventListener('click', () => { selectBlip(); updateProf({ [key]: c }); });
            box.appendChild(b);
        });
    };
    swatches($('profSwatches'), p.color, 'color');
    swatches($('profSwatches2'), p.color2, 'color2');
    const grid = $('profIcons');
    grid.innerHTML = '';
    for (let i = 0; i < 24; i++) {
        const cv = el('canvas', i === p.icon ? 'on' : '');
        cv.width = cv.height = 64;
        drawIcon(cv, i, p.color, p.shade, p.color2);
        cv.addEventListener('click', () => { selectBlip(); updateProf({ icon: i }); });
        grid.appendChild(cv);
    }
}

async function refreshProfile() {
    prof = await window.umk3.loadProfile(tx('profDefault'));
    await loadProfArt();
    paintProfile();
}

function initProfile() {
    $('profBadge').addEventListener('click', () => { selectBlip(); setSection('profile', false); });
    $('profName').addEventListener('input', () =>
        updateProf({ name: $('profName').value.replace(/[|\r\n\t]/g, ' ') }));
    $('profName').addEventListener('change', () => {
        if (!$('profName').value.trim())
            updateProf({ name: tx('profDefault') });
    });
    $('profColor').addEventListener('input', () =>
        updateProf({ color: $('profColor').value.replace('#', '').toUpperCase() }));
    document.querySelectorAll('#profShade .tab').forEach(t =>
        t.addEventListener('click', () => {
            selectBlip();
            updateProf({ shade: t.dataset.shade });
        }));
    $('profColor2').addEventListener('input', () =>
        updateProf({ color2: $('profColor2').value.replace('#', '').toUpperCase() }));
    $('profReset').addEventListener('click', async () => {
        if (!profResetArmed) { profResetArmed = true; paintProfile(); return; }
        profResetArmed = false;
        failChord();
        prof = await window.umk3.resetProfile(tx('profDefault'));
        paintProfile();
    });
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
    if (navKey(e.key))
        e.preventDefault();
}

/* The menu's keys, from the keyboard or the pad: true when one acted. */
function navKey(key) {
    const i = SECTIONS.indexOf(sec);
    if (key === 'ArrowDown' || key === 'ArrowUp') {
        const n = SECTIONS[(i + (key === 'ArrowDown' ? 1 : SECTIONS.length - 1)) % SECTIONS.length];
        hoverBlip();
        setSection(n === 'quit' ? 'quit' : n, false);
        if (n === 'quit') sec = 'quit';
        return true;
    }
    if (key === 'Enter' && document.activeElement === document.body) {
        selectBlip();
        setSection(sec, true);
        return true;
    }
    if (key === 'Escape') {
        closePicker();
        setSection('build', false);
        return true;
    }
    return false;
}

/* -------------------------------------------------------------- init ---- */

async function init() {
    cfg = await window.umk3.loadConfig();
    ui = cfg.ui === 'ES' ? 'ES' : 'EN';
    await refreshRoot();
    buildRing();
    GLYPH = (await window.umk3.glyphs()) || {};
    paintCmdbar();
    requestAnimationFrame(padTick);
    applyTexts();
    initProfile();
    refreshProfile();
    /* The game's settings menu (Esc) writes umk3.ini and profiles.ini too:
     * read them again whenever the launcher comes back to the front. */
    window.addEventListener('focus', async () => {
        if (waitingKey || building)
            return;
        cfg = await window.umk3.loadConfig();
        applyConfig();
        paintKeys();
        fillPads();
        if (document.activeElement !== $('profName'))
            refreshProfile();
    });

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
    /* one arch= for both: COMPILAR builds it, JUGAR opens it */
    for (const id of ['arch', 'playarch'])
        $(id).addEventListener('change', async () => {
            const i = $(id).selectedIndex;
            $('arch').selectedIndex = $('playarch').selectedIndex = i;
            await save({ arch: ARCH[i] });
            await refreshRoot();      /* compiled or not: that exe's state */
        });
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
        const k = pl === 2 ? 'p2Pad' : 'pad', kn = pl === 2 ? 'p2PadName' : 'padName';
        cfg[k] = v;
        cfg[kn] = seenPads[v] || '';
        selectBlip();
        save({ [k]: v, [kn]: seenPads[v] || '' });
        paintPadMap();
    });
    $('monitor').addEventListener('change', () => {
        cfg.monitor = Number($('monitor').value) || 0;
        selectBlip();
        save({ monitor: cfg.monitor });
    });
    fillMonitors();
    $('padIcons').addEventListener('change', () => {
        const k = pl === 2 ? 'p2PadIcons' : 'padIcons';
        cfg[k] = $('padIcons').value;
        selectBlip();
        save({ [k]: cfg[k] });
        fillPads();
        paintCmdbar();
    });
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
    $('respad').addEventListener('click', () => {
        selectBlip();
        cfg[PK()] = Object.assign({}, PAD_DEFAULT);
        cfg[PK() + '5'] = Object.assign({}, PAD5_DEFAULT);
        save({ [PK()]: cfg[PK()], [PK() + '5']: cfg[PK() + '5'] });
        paintPadMap();
    });
    $('reskeys').addEventListener('click', () => {
        selectBlip();
        /* the player's keys only; the debug keys have their own reset */
        if (pl === 2) {
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
        waitingKey = null;
        $('keyhint').textContent = T[ui].hint;
        paintKeys();
    });
    /* GRAPHICS: VIDEO, TEXTURES, INTERFACE */
    document.querySelectorAll('.tab[data-g]').forEach(t => {
        t.addEventListener('pointerenter', hoverBlip);
        t.addEventListener('click', () => { selectBlip(); paintGfxTab(t.dataset.g); });
    });
    paintGfxTab('video');
    document.querySelectorAll('section[data-sec="controls"] .tab').forEach(t => {
        t.addEventListener('pointerenter', hoverBlip);
        t.addEventListener('click', () => {
            selectBlip();
            if (t.dataset.pl) {
                pl = Number(t.dataset.pl);
                fillPads();
            } else if (t.dataset.c) {
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
    if (logo)
        $('logoImg').src = logo;
    window.addEventListener('keydown', onKey, true);
    $('play').addEventListener('click', doPlay);
    document.querySelectorAll('.btn').forEach(b => b.addEventListener('pointerenter', hoverBlip));

    /* UMK3_LAUNCHER_SEC=<section>[:<controls tab>], the capture aid */
    const [want, sub] = (new URLSearchParams(location.search).get('sec') || '').split(':');
    setSection(SECTIONS.includes(want) && want !== 'quit' ? want : ready ? 'play' : 'build', false);
    if (sub && document.querySelector('.tab[data-c="' + sub + '"]'))
        document.querySelector('.tab[data-c="' + sub + '"]').click();
    else if (sub === 'end')             /* scrolled to its foot */
        setTimeout(() => { document.querySelector('.sec.active').scrollTop = 1e6; }, 300);

    /* an update just installed is compiled first; else ask GitHub, without
     * holding up the window */
    if (!(await afterUpdate()))
        setTimeout(checkUpdate, 800);

    /* development aid: UMK3_LAUNCHER_SHOT=<png> captures the window */
    setTimeout(() => window.umk3.shot(), 600);
}

document.addEventListener('DOMContentLoaded', init);
