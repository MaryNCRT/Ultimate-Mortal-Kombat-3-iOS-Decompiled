/* profiles.ini -- the player's profile, shared with the game's settings menu
 * (runtime/profiles.c reads and writes the same file). Port, not in the
 * binary: kept for the leaderboards to come.
 *
 *     profile=7KQ2-9XMD-4TPA|MARY|18|3A8EE6|L
 *
 * One profile per installation: the id, the name (anything but "|" and line
 * breaks, up to 24 characters), the icon (a fighter, 0..23, the bio and
 * endings art: res/Textures/<NAME>_VERSUS.PNG), the colour behind it
 * (RRGGBB) and the gradient: L from white to the colour, D from black to it
 * (absent: L). UTF-8.
 *
 * The id is given once, when the installation first has no profile: 60
 * random bits from the system's cryptographic generator as twelve Crockford
 * base-32 digits, XXXX-XXXX-XXXX -- about a billion billion possible. It is
 * never changed or deleted; RESET puts the name, icon and colour back and
 * keeps it. */
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const ALPHA = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';
const NAME_MAX = 24;
const ICONS = 24;
const DEFAULTS = { name: 'PLAYER', icon: 18, color: '3A8EE6', dark: false };

/* The bio art, in CharacterNames order (Versus_Names, 0x000ffbf4). */
const ICON_FILES = [
    'KANO', 'SONYA', 'JAXX', 'NIGHTWOLF', 'SUBZERO_HUMAN', 'STRYKER', 'SINDEL',
    'SEKTOR', 'CYRAX', 'KUNGLAO', 'KABAL', 'SHEEVA', 'SHANGTSUNG', 'LUIKANG',
    'SMOKE', 'KITANA', 'JADE', 'MILEENA', 'SCORPION', 'REPTILE', 'ERMAC',
    'SUBZERO', 'SMOKE_SECRET', 'NOOBSAIBOT',
].map(n => n + '_VERSUS.PNG');

function newId() {
    let v = crypto.randomBytes(8).readBigUInt64BE(0) >> 4n;   /* 60 bits */
    let s = '';
    for (let i = 0; i < 12; i++) {
        s = ALPHA[Number(v & 31n)] + s;
        v >>= 5n;
    }
    return s.slice(0, 4) + '-' + s.slice(4, 8) + '-' + s.slice(8);
}

function cleanName(n) {
    return Array.from(String(n || '').replace(/[|\r\n\t]/g, ' '))
        .slice(0, NAME_MAX).join('');
}

function cleanColor(c) {
    c = String(c || '').replace('#', '').toUpperCase();
    return /^[0-9A-F]{6}$/.test(c) ? c : DEFAULTS.color;
}

function read(root) {
    let text = '';
    try {
        text = fs.readFileSync(path.join(root, 'profiles.ini'), 'utf8');
    } catch (err) {
        return null;
    }
    for (const raw of text.split(/\r?\n/)) {
        if (!raw.startsWith('profile=')) continue;
        const f = raw.slice(8).split('|');
        if (f.length < 4 || !/^[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{4}$/.test(f[0]))
            continue;
        const icon = Number(f[2]);
        return { id: f[0], name: cleanName(f[1]),
                 icon: icon >= 0 && icon < ICONS ? icon : DEFAULTS.icon,
                 color: cleanColor(f[3]), dark: (f[4] || '').trim().toUpperCase() === 'D' };
    }
    return null;
}

function save(root, p) {
    const lines = ['; UMK3 player profile: the launcher and the game both edit this;',
                   '; the id belongs to this installation and never changes',
                   'profile=' + [p.id, cleanName(p.name), p.icon | 0, cleanColor(p.color),
                                 p.dark ? 'D' : 'L'].join('|')];
    fs.writeFileSync(path.join(root, 'profiles.ini'), lines.join('\n') + '\n', 'utf8');
}

/* The profile, made (and its id given) the first time there is none. */
function load(root, defaultName) {
    let p = read(root);
    if (!p) {
        p = { id: newId(), name: defaultName || DEFAULTS.name,
              icon: DEFAULTS.icon, color: DEFAULTS.color, dark: DEFAULTS.dark };
        save(root, p);
    }
    return p;
}

/* Name, icon and colour back to the defaults; the id stays. */
function reset(root, defaultName) {
    const p = load(root, defaultName);
    p.name = defaultName || DEFAULTS.name;
    p.icon = DEFAULTS.icon;
    p.color = DEFAULTS.color;
    p.dark = DEFAULTS.dark;
    save(root, p);
    return p;
}

/* Only the editable fields come from the renderer: never the id. */
function update(root, fields) {
    const p = load(root);
    if (fields.name !== undefined) p.name = cleanName(fields.name);
    if (fields.icon !== undefined && fields.icon >= 0 && fields.icon < ICONS) p.icon = fields.icon | 0;
    if (fields.color !== undefined) p.color = cleanColor(fields.color);
    if (fields.dark !== undefined) p.dark = !!fields.dark;
    save(root, p);
    return p;
}

/* The icon's art as a data: URL, or null before the game is compiled. */
function iconData(root, i) {
    const f = ICON_FILES[i];
    if (!f) return null;
    try {
        return 'data:image/png;base64,' +
            fs.readFileSync(path.join(root, 'res', 'Textures', f)).toString('base64');
    } catch (err) {
        return null;
    }
}

module.exports = { load, update, reset, newId, cleanName, cleanColor, iconData, ICONS, NAME_MAX };
