/* Finds the game folder (the release/repository root) that holds
 * launcher\build_game.ps1, umk3-game.exe, res\ and umk3.ini.
 *
 * The packaged launcher lives in launcher\bin\ and is started by the stub
 * UMK3-Launcher.exe in the game folder, which sets UMK3_ROOT and the working
 * directory to that folder; process.cwd() and the exe's own directory are the
 * fallbacks.
 */
'use strict';

const fs = require('fs');
const path = require('path');

function looksLikeRoot(dir) {
    return fs.existsSync(path.join(dir, 'src', 'runtime'))
        || fs.existsSync(path.join(dir, 'tools', 'build'))
        || fs.existsSync(path.join(dir, 'umk3-game'))
        || fs.existsSync(path.join(dir, 'umk3-game.exe'))
        || fs.existsSync(path.join(dir, 'res', 'Info.plist'));
}

function walkUp(start) {
    let dir = path.resolve(start);
    for (;;) {
        if (looksLikeRoot(dir))
            return dir;
        const parent = path.dirname(dir);
        if (parent === dir)
            return null;
        dir = parent;
    }
}

function findRoot(candidates) {
    for (const c of candidates) {
        if (!c)
            continue;
        const hit = walkUp(c);
        if (hit)
            return hit;
    }
    return candidates[0] ? path.resolve(candidates[0]) : null;
}

module.exports = { findRoot, looksLikeRoot };