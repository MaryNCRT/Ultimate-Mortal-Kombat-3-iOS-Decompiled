/* The key and button pictures (Julio Cacko's Free Input Prompts, CC0), the
 * same the game's menus draw (runtime/platform/win32_gl.c): the Vintage
 * keyboard, the Retro Xbox and PlayStation 5 pads, the Stylized PlayStation
 * 4 and the Default Nintendo ones. Which file shows a key or a button;
 * KeyFace and PadFace draw them. */

/* a virtual key's picture: kb/T_<file>_Key_Vintage */
const KB_FILES = {
    8: 'BackSpace', 9: 'Tab', 13: 'Enter', 16: 'Shift', 17: 'Crtl', 18: 'Alt', 20: 'CapsLock',
    27: 'Esc', 32: 'Space', 33: 'PageUp', 34: 'PageDown', 35: 'End', 36: 'Home',
    37: 'Left', 38: 'Up', 39: 'Right', 40: 'Down', 44: 'PrtScrn', 45: 'Ins', 46: 'Del',
    106: 'Asterisk', 107: 'Plus', 109: 'Minus', 110: 'Del', 111: 'Slash', 144: 'NumLock',
    186: 'Semicolon', 187: 'Plus', 189: 'Minus', 191: 'Slash', 192: 'Tilde',
    219: 'Brackets_L', 221: 'Brackets_R', 222: 'Quotation',
};
export function kbFile(vk) {
    if ((vk >= 65 && vk <= 90) || (vk >= 48 && vk <= 57)) return String.fromCharCode(vk);
    if (vk >= 96 && vk <= 105) return String(vk - 96);
    if (vk >= 112 && vk <= 123) return 'F' + (vk - 111);
    return KB_FILES[vk] || null;
}

/* a key's name when it has no picture */
const SPECIAL = {
    8: 'BACKSPACE', 9: 'TAB', 13: 'ENTER', 27: 'ESC', 32: 'SPACE',
    16: 'SHIFT', 17: 'CTRL', 18: 'ALT', 20: 'CAPS', 33: 'PGUP', 34: 'PGDN',
    35: 'END', 36: 'HOME', 37: '←', 38: '↑', 39: '→', 40: '↓',
    45: 'INS', 46: 'DEL', 144: 'NUMLOCK', 145: 'SCROLL',
    186: ';', 187: '=', 188: ',', 189: '-', 190: '.', 191: '/', 192: '`',
    219: '[', 220: '\\', 221: ']', 222: "'",
};
export function vkLabel(vk) {
    if ((vk >= 65 && vk <= 90) || (vk >= 48 && vk <= 57)) return String.fromCharCode(vk);
    if (vk >= 112 && vk <= 135) return 'F' + (vk - 111);
    if (vk >= 96 && vk <= 105) return 'NUM ' + (vk - 96);
    if (vk >= 106 && vk <= 111) return 'NUM ' + ['*', '+', '', '-', '.', '/'][vk - 106];
    return SPECIAL[vk] || '#' + vk;
}

/* The game's PB_* buttons, by position, and each make's pictures. */
export const PB = ['SOUTH', 'EAST', 'WEST', 'NORTH', 'L1', 'R1', 'L2', 'R2', 'SELECT', 'START', 'L3', 'R3', 'HOME'];
export const PAD_ICON_NAMES = ['auto', 'xbox', 'ps5', 'ps4', 'nintendo'];
export const PAD_SETS = {
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
export function padMake(dev, id) {
    const s = String(dev || '') + ' ' + String(id || '');
    if (/054c[:\s-]*(product:\s*)?(05c4|09cc|0ba0)/i.test(s)) return 'ps4';
    if (/054c/i.test(s)) return 'ps5';
    if (/057e/i.test(s)) return 'nintendo';
    return 'xbox';
}

/* A button's picture name in a make's set, or null (then its name shows). */
export function padFile(kind, b) {
    const set = PAD_SETS[kind] || PAD_SETS.xbox, i = PB.indexOf(b);
    return i >= 0 && set.files[i] ? set.dir + '/' + set.files[i] : null;
}
export function padName(kind, b) {
    const set = PAD_SETS[kind] || PAD_SETS.xbox;
    return set.names[b] || b;
}

/* The pad the browser sees, for the launcher's own menus. */
export function livePad() {
    const all = navigator.getGamepads ? Array.from(navigator.getGamepads()).filter(Boolean) : [];
    return all.find(g => g.connected) || null;
}
