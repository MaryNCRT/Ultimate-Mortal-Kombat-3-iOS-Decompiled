/* Compile (launcher\build_game.ps1) and Play (umk3-game.exe), the same
 * two actions launcher.c performed with CreateProcess -- only the output
 * is captured here and streamed to the renderer instead of a console.
 *
 * Progress is pieced together from what the script prints:
 *   "== N/5 <name>"           the five steps
 *   "  [i/total] file.c"      one clang per source during step 4
 * and mapped onto a 0..100 scale the renderer shows as a block bar.
 */
'use strict';

const { spawn } = require('child_process');
const path = require('path');

let g_child = null;

function sq(s) {
    return s.replace(/'/g, "''");
}

/* Step boundaries as percentages of the build (5 steps from the script). */
const STEPS = [
    { lo: 0,  hi: 18, title: '' },         /* 1 toolchain (downloads) */
    { lo: 18, hi: 24, title: 'Leyendo el binario del .ipa' },
    { lo: 24, hi: 42, title: 'Extrayendo las tablas del juego' },
    { lo: 42, hi: 90, title: 'Compilando el juego' },
    { lo: 90, hi: 100, title: 'Copiando res desde el .ipa' },
];

function parseProgress(text) {
    let pct = null;
    let step = null;
    let phase = '';
    const stepMatch = text.match(/^\s*==\s*(\d+)\/(\d+)\b/);
    if (stepMatch) {
        step = parseInt(stepMatch[1], 10);
        const s = STEPS[step - 1];
        if (s)
            phase = s.title;
    }
    const fileMatch = text.match(/^\s*\[\s*(\d+)\s*\/\s*(\d+)\s*\]/);
    if (fileMatch) {
        const done = parseInt(fileMatch[1], 10);
        const total = parseInt(fileMatch[2], 10);
        if (total > 0) {
            const s = STEPS[3];
            pct = Math.round(s.lo + (done / total) * (s.hi - s.lo));
        }
    } else if (step) {
        const s = STEPS[step - 1];
        pct = Math.round(s.lo + (s.hi - s.lo) * 0.5);
    }
    return { pct, step, phase };
}

/* Runs the compile.  Calls onData({ text, pct, step, phase }) per chunk (one
 * chunk per write line, because the script writes whole lines per Write-Host)
 * and resolves with the exit code. */
function build(root, ipa, arch, onData) {
    return new Promise(resolve => {
        const ps1 = path.join(root, 'launcher', 'build_game.ps1');
        const cmd =
            "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; " +
            "& '" + sq(ps1) + "' -Ipa '" + sq(ipa) + "' -Arch " + (arch === 'x64' ? 'x64' : 'x86');
        const child = spawn('powershell.exe',
            ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', cmd],
            { cwd: root, windowsHide: true });
        g_child = child;

        /* Line-split a raw stream, emitting whole lines (partial chunks are
         * buffered until their newline arrives). */
        const reader = buf => {
            let i;
            while ((i = buf.indexOf('\n')) >= 0) {
                const line = buf.slice(0, i).replace(/\r$/, '');
                buf = buf.slice(i + 1);
                if (line)
                    onData({ text: line, ...parseProgress(line) });
            }
            if (buf.length > 8192) {          /* unremitting \r output (curl) */
                onData({ text: buf, ...parseProgress(buf) });
                buf = '';
            }
            return buf;
        };
        let out = '', err = '';
        child.stdout.on('data', d => { out = reader(out + d.toString('utf8')); });
        child.stderr.on('data', d => { err = reader(err + d.toString('utf8')); });
        const flush = () => {
            if (out) onData({ text: out, ...parseProgress(out) });
            if (err) onData({ text: err, ...parseProgress(err) });
        };
        child.on('error', errEv => {
            g_child = null;
            onData({ text: 'Error al iniciar PowerShell: ' + errEv.message, pct: -1 });
            resolve(-1);
        });
        child.on('close', code => {
            g_child = null;
            flush();
            onData({ text: 'Proceso terminado (código ' + code + ')', pct: 100 });
            resolve(code);
        });
    });
}

function cancelBuild() {
    const child = g_child;
    if (!child)
        return false;
    try {
        spawn('taskkill.exe', ['/pid', String(child.pid), '/T', '/F'],
            { windowsHide: true });
        return true;
    } catch (err) {
        return false;
    }
}

function play(root, exe) {
    exe = path.join(root, exe);
    const child = spawn(exe, [], {
        cwd: root,
        detached: true,
        stdio: 'ignore',
        windowsHide: false,
    });
    child.unref();
    return true;
}

module.exports = { build, cancelBuild, play, parseProgress, STEPS };