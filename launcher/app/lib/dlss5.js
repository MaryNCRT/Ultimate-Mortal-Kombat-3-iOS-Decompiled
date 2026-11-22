'use strict';
/*
 * dlss5.js -- VIDEO > DLSS 5 (src/runtime/neural_mod.c, docs/DLSS5-MOD.md).
 *
 * The game ships ReShade (our build) and DLSS5-Feeder in graphics\dlss5;
 * NVIDIA's two runtimes and the neural consumer add-on are the player's to
 * supply, since nobody here may redistribute them. This file:
 *
 *   - status: asks the game (`umk3-game --dlss5-status`) what stands in the
 *     way: the same check the game makes before it loads anything;
 *   - import: copies the files the player picked into graphics\dlss5, each
 *     by its name: nvngx_dlss.dll and nvngx_dlssnr.dll only with a valid
 *     Authenticode signature from NVIDIA (PowerShell's
 *     Get-AuthenticodeSignature), the consumer add-on by its known names. A
 *     file it would replace is kept beside it as <name>.old. Anything else
 *     is refused and said so.
 */

const fs = require('fs');
const path = require('path');
const { execFile } = require('child_process');

const DIR = path.join('graphics', 'dlss5');

/* neural_mod.h NEURAL_*, in order */
const STATUS = ['active', 'ready', 'need_d3d12', 'need_nvidia_gpu', 'conflict', 'not_installed', 'need_files', 'dlaa_only'];

/* what a picked file is, by its name: 'nvngx' (signed NVIDIA DLL),
 * 'consumer' (a neural consumer add-on), or null (refused) */
function classify(file) {
    const n = String(file).split(/[\\/]/).pop().toLowerCase();   /* either separator, on any OS */
    if (n === 'nvngx_dlss.dll' || n === 'nvngx_dlssnr.dll')
        return 'nvngx';
    if (/^renodx-dlss5.*\.addon64$/.test(n) || /^deep-fried-chicken.*\.addon64$/.test(n))
        return 'consumer';
    return null;
}

/* the game's answer to --dlss5-status: "dlss5<TAB>n<TAB>name<TAB>on" */
function parseStatus(out) {
    for (const line of String(out || '').split(/\r?\n/)) {
        const m = line.match(/^dlss5\t(\d+)\t([^\t]*)\t(\d)/);
        if (m)
            return { status: STATUS[Number(m[1])] || 'unknown', text: m[2], on: m[3] === '1' };
    }
    return { status: 'unknown', text: '', on: false };
}

function status(root, exe) {
    return new Promise(resolve => {
        if (!exe)
            return resolve({ status: 'no_game', text: '', on: false, files: present(root) });
        execFile(exe, ['--dlss5-status'], { cwd: root, timeout: 15000, windowsHide: true },
            (err, out) => resolve({ ...parseStatus(out), files: present(root) }));
    });
}

/* which of the player's files are already there */
function present(root) {
    const dir = path.join(root, DIR);
    let names = [];
    try { names = fs.readdirSync(dir); } catch { /* none yet */ }
    const low = names.map(n => n.toLowerCase());
    return {
        installed: low.includes('reshade64.dll') && low.includes('dlss5-feed.addon64'),
        dlss: low.includes('nvngx_dlss.dll'),
        dlssnr: low.includes('nvngx_dlssnr.dll'),
        consumer: names.find(n => classify(n) === 'consumer') || '',
    };
}

/* Authenticode: Valid, and NVIDIA in the signer's subject */
function signedByNvidia(file) {
    return new Promise(resolve => {
        if (process.platform !== 'win32')
            return resolve({ ok: false, why: 'Windows only' });
        const ps = '$s = Get-AuthenticodeSignature -LiteralPath $env:UMK3_SIG_FILE; '
                 + '"$($s.Status)|$($s.SignerCertificate.Subject)"';
        execFile('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', ps],
            { timeout: 20000, windowsHide: true, env: { ...process.env, UMK3_SIG_FILE: file } },
            (err, out) => {
                const [st, subj] = String(out || '').trim().split('|');
                if (st === 'Valid' && /NVIDIA/i.test(subj || ''))
                    resolve({ ok: true, why: subj });
                else
                    resolve({ ok: false, why: st === 'Valid' ? 'signed, but not by NVIDIA' : 'no valid signature (' + (st || 'unknown') + ')' });
            });
    });
}

async function importFiles(root, files) {
    const dir = path.join(root, DIR);
    const results = [];
    fs.mkdirSync(dir, { recursive: true });
    for (const file of files || []) {
        const name = path.basename(file);
        const kind = classify(file);
        if (!kind) {
            results.push({ name, ok: false, why: 'not a DLSS 5 file (nvngx_dlss.dll, nvngx_dlssnr.dll, renodx-dlss5*.addon64, deep-fried-chicken*.addon64)' });
            continue;
        }
        if (kind === 'nvngx') {
            const sig = await signedByNvidia(file);
            if (!sig.ok) {
                results.push({ name, ok: false, why: sig.why });
                continue;
            }
        }
        if (kind === 'consumer')        /* exactly one consumer: the feeder says two conflict */
            for (const n of fs.readdirSync(dir))
                if (classify(n) === 'consumer' && n.toLowerCase() !== name.toLowerCase())
                    fs.renameSync(path.join(dir, n), path.join(dir, n + '.old'));
        const dest = path.join(dir, name.toLowerCase().startsWith('nvngx') ? name.toLowerCase() : name);
        if (path.resolve(dest) === path.resolve(file)) {
            results.push({ name, ok: true, why: 'already in place' });
            continue;
        }
        if (fs.existsSync(dest))
            fs.copyFileSync(dest, dest + '.old');
        fs.copyFileSync(file, dest);
        results.push({ name, ok: true, why: kind === 'nvngx' ? 'signed by NVIDIA' : 'neural consumer' });
    }
    return results;
}

module.exports = { DIR, STATUS, classify, parseStatus, status, present, importFiles };
