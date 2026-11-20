'use strict';
/*
 * verify.js -- VERIFY and REPAIR's checks (Mary, 0.5.6): what in the game
 * folder is missing or damaged, and the missing or damaged sources put back
 * from the repository.
 *
 *   - The sources the compile reads -- src/, the extraction's scripts in
 *     tools/build, the launcher's aditional textures and lighting -- against the repository
 *     at this launcher's own tag (v<version>): GitHub's tree lists every
 *     file with its git hash (sha1 of "blob <size>\0" + the bytes), so a
 *     file is checked without downloading it, and one that is missing or
 *     differs is downloaded again (raw.githubusercontent.com) and checked.
 *     The list is kept in build/manifest-<version>.json, so a second check
 *     needs no network. A text file is also taken with its line ends as
 *     LF, which is how the repository holds it.
 *   - What a compile leaves in build/: objects without their response,
 *     stamp or dependency file, or empty (lib/build.js scanBroken), files
 *     left by another version's compile, the tables made from the .ipa, the
 *     exe.
 */

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { REPO } = require('./update');

/* what the packages ship of the repository (launcher/make_release*.py):
 * the sources, the extraction's scripts, the launcher's textures and
 * lighting. The EOS SDK is not checked: each package carries its own
 * platform's part, and the game compiles without it (no internet play). */
const PREFIXES = ['src/', 'launcher/aditional textures/', 'launcher/lighting/'];
const TOOLS = ['tools/build/logic_tables.py', 'tools/build/logic_tables.map', 'tools/build/macho.py',
               'tools/build/low32_static.py', 'tools/build/level_info.py', 'tools/build/seq_data.py',
               'tools/build/check_binary.py'];

function gitHash(buf) {
    return crypto.createHash('sha1').update('blob ' + buf.length + '\0').update(buf).digest('hex');
}

function sameAs(file, sha) {
    let b;
    try { b = fs.readFileSync(file); } catch (_) { return null; }     /* missing */
    if (gitHash(b) === sha)
        return true;
    if (b.includes(13)) {                                             /* CRLF on disk */
        const lf = Buffer.from(b.toString('latin1').replace(/\r\n/g, '\n'), 'latin1');
        if (gitHash(lf) === sha)
            return true;
    }
    return false;
}

/* The repository's files at this version: [{ path, sha, size }] */
async function manifest(fetchFn, buildDir, version) {
    const keep = path.join(buildDir, `manifest-${version}.json`);
    try { return JSON.parse(fs.readFileSync(keep, 'utf8')); } catch (_) { /* not kept yet */ }
    if (!fetchFn)
        throw new Error('no network to read the file list');
    const url = `https://api.github.com/repos/${REPO}/git/trees/v${encodeURIComponent(version)}?recursive=1`;
    const r = await fetchFn(url, { headers: { 'User-Agent': 'UMK3-Launcher', Accept: 'application/vnd.github+json' } });
    if (!r.ok)
        throw new Error(`the repository's file list for v${version} could not be read (HTTP ${r.status})`);
    const j = await r.json();
    const list = (j.tree || [])
        .filter(e => e.type === 'blob'
                     && (PREFIXES.some(p => e.path.startsWith(p)) || TOOLS.includes(e.path)))
        .map(e => ({ path: e.path, sha: e.sha, size: e.size }));
    if (!list.length)
        throw new Error('the repository listed no files for this version');
    try {
        fs.mkdirSync(buildDir, { recursive: true });
        fs.writeFileSync(keep, JSON.stringify(list));
    } catch (_) { /* not kept */ }
    return list;
}

/* Which of the sources are missing or damaged under srcRoot */
function scanSources(srcRoot, list) {
    const missing = [], damaged = [];
    for (const e of list) {
        const ok = sameAs(path.join(srcRoot, ...e.path.split('/')), e.sha);
        if (ok === null)
            missing.push(e);
        else if (!ok)
            damaged.push(e);
    }
    return { missing, damaged };
}

/* Those files downloaded again, each checked against its hash */
async function restore(fetchFn, srcRoot, version, files, onText) {
    let done = 0;
    const failed = [];
    for (const e of files) {
        const url = `https://raw.githubusercontent.com/${REPO}/v${encodeURIComponent(version)}/`
                    + e.path.split('/').map(encodeURIComponent).join('/');
        try {
            const r = await fetchFn(url, { headers: { 'User-Agent': 'UMK3-Launcher' } });
            if (!r.ok)
                throw new Error('HTTP ' + r.status);
            const b = Buffer.from(await r.arrayBuffer());
            if (gitHash(b) !== e.sha)
                throw new Error('the download does not match the repository');
            const to = path.join(srcRoot, ...e.path.split('/'));
            fs.mkdirSync(path.dirname(to), { recursive: true });
            fs.writeFileSync(to, b);
            done++;
            onText(`Restored ${e.path}`);
        } catch (err) {
            failed.push(e.path);
            onText(`Could not restore ${e.path}: ${err.message}`);
        }
    }
    return { done, failed };
}

module.exports = { manifest, scanSources, restore, gitHash, PREFIXES };
