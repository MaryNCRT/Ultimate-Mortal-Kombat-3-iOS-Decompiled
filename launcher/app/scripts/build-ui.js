/* Builds the window (ui/, SvelteKit) into renderer/: run by `npm run ui`,
 * and by electron-builder before it packs (package.json, beforePack), so a
 * package never ships an old or missing renderer.
 *
 * SvelteKit writes the page with root paths (/_app/...), which from disk
 * would point at the drive's root: they are made relative here, and the
 * hash of the start-up script, which the Content-Security-Policy allows by
 * that hash, is worked out again for the changed script. */
'use strict';
const { execFileSync } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const APP = path.join(__dirname, '..');

function buildUi() {
    const vite = path.join(APP, 'node_modules', 'vite', 'bin', 'vite.js');
    execFileSync(process.execPath, [vite, 'build'], { cwd: path.join(APP, 'ui'), stdio: 'inherit' });

    const page = path.join(APP, 'renderer', 'index.html');
    let html = fs.readFileSync(page, 'utf8')
        .replace(/(href|src)="\/_app\//g, '$1="./_app/')
        .replace(/import\("\/_app\//g, 'import("./_app/');
    const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)]
        .map(m => "'sha256-" + crypto.createHash('sha256').update(m[1]).digest('base64') + "'");
    /* 'wasm-unsafe-eval': the viewer's SMOOTH MODEL (ui/src/lib/mods/subsurf.js) */
    html = html.replace(/script-src 'self'[^;"]*/, "script-src 'self' 'wasm-unsafe-eval' " + scripts.join(' '));
    /* SvelteKit's route announcer hides itself with a style attribute: that
     * one style, by its hash, and no other */
    const styles = new Set();
    const imm = path.join(APP, 'renderer', '_app', 'immutable');
    for (const dir of fs.readdirSync(imm))
        for (const f of fs.readdirSync(path.join(imm, dir)).filter(n => n.endsWith('.js')))
            for (const m of fs.readFileSync(path.join(imm, dir, f), 'utf8').matchAll(/svelte-announcer"[^>]*?style="([^"]*)"/g))
                styles.add("'sha256-" + crypto.createHash('sha256').update(m[1]).digest('base64') + "'");
    if (styles.size)
        html = html.replace(/style-src 'self'/, "style-src 'self' 'unsafe-hashes' " + [...styles].join(' '));
    if (/"\/_app\//.test(html))
        throw new Error('renderer/index.html still has a root path');
    fs.writeFileSync(page, html);
}

module.exports = async () => buildUi();
if (require.main === module) buildUi();
