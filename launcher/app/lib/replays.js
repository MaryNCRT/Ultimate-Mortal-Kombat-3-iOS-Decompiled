'use strict';

/*
 * REPLAYS: the fights the game recorded (src/runtime/rollback_probe.c).
 *
 *   <root>/replays/recent/   the game's own: the last five fights, the
 *                            oldest dropped as a new one comes
 *   <root>/replays/saved/    kept for good: a recent one moved here, or one
 *                            imported from another player
 *
 * A file is "UMK3RPL4" (or the older "UMK3RPL3"), the build, the setup's word count and words (the two
 * fighters first, the level sixth), the two seeds, then 120 bytes a tick.
 * The game plays one with `--replay replays/<folder>/<file>`.
 */

const fs = require('fs');
const path = require('path');

/* RPL4 since the rival is the one actually fought (rollback_probe.c
 * rb_fight_rival); RPL3 files still play */
const MAGICS = ['UMK3RPL4', 'UMK3RPL3'];
const TICK_BYTES = 120;
const FOLDERS = ['recent', 'saved'];

function dir(root, folder) {
    return path.join(root, 'replays', folder);
}

/* Only a plain file name, so nothing reaches outside replays/. */
function safeName(name) {
    return typeof name === 'string' && /^[\w .()'+-]+\.umkr$/i.test(name) && !name.includes('..');
}

/* What the header says, or null when the file is not a replay. */
function readHeader(file) {
    let fd;
    try {
        fd = fs.openSync(file, 'r');
        const b = Buffer.alloc(16 + 4 * 24);
        const got = fs.readSync(fd, b, 0, b.length, 0);
        if (got < 16 || !MAGICS.includes(b.toString('latin1', 0, 8)))
            return null;
        const n = b.readUInt32LE(12);
        if (n < 7 || n > 24 || got < 16 + 4 * n)
            return null;
        const setup = [];
        for (let i = 0; i < n; i++)
            setup.push(b.readInt32LE(16 + 4 * i));
        const dataAt = 16 + 4 * n + 8;
        const size = fs.fstatSync(fd).size;
        const ticks = Math.max(0, Math.floor((size - dataAt) / TICK_BYTES));
        return { p1: setup[0], p2: setup[1], level: setup[5], mode: setup[6], ticks, size };
    } catch (_) {
        return null;
    } finally {
        if (fd !== undefined)
            fs.closeSync(fd);
    }
}

/* "20261029-120102_sub-zero_vs_kitana.umkr": the date and the two names */
function fromName(name) {
    const m = /^(\d{8})-(\d{6})_([\w-]+)_vs_([\w-]+)\.umkr$/i.exec(name);
    if (!m)
        return {};
    const [d, t] = [m[1], m[2]];
    return {
        when: `${d.slice(0, 4)}-${d.slice(4, 6)}-${d.slice(6)} ${t.slice(0, 2)}:${t.slice(2, 4)}`,
        n1: m[3], n2: m[4],
    };
}

function list(root) {
    const out = {};
    for (const folder of FOLDERS) {
        let names = [];
        try {
            names = fs.readdirSync(dir(root, folder)).filter(safeName);
        } catch (_) {}
        out[folder] = names.map(name => {
            const file = path.join(dir(root, folder), name);
            const h = readHeader(file);
            if (!h)
                return null;
            let mtime = 0;
            try { mtime = fs.statSync(file).mtimeMs; } catch (_) {}
            return { folder, name, rel: `replays/${folder}/${name}`, mtime, seconds: h.ticks / 60, ...h, ...fromName(name) };
        }).filter(Boolean).sort((a, b) => b.mtime - a.mtime);
    }
    return out;
}

/* A name not taken in `folder`: "x.umkr", then "x (2).umkr" ... */
function freeName(folder, name) {
    const ext = path.extname(name), base = path.basename(name, ext);
    let k = 1, cand = name;
    while (fs.existsSync(path.join(folder, cand)))
        cand = `${base} (${++k})${ext}`;
    return cand;
}

/* A recent one, kept for good. */
function keep(root, name) {
    if (!safeName(name))
        return null;
    const from = path.join(dir(root, 'recent'), name), to = dir(root, 'saved');
    if (!fs.existsSync(from))
        return null;
    fs.mkdirSync(to, { recursive: true });
    const dest = freeName(to, name);
    fs.copyFileSync(from, path.join(to, dest));
    fs.unlinkSync(from);
    return dest;
}

/* Files from anywhere into saved/; the ones that are not replays are left
 * out. Returns how many came in. */
function importFiles(root, files) {
    const to = dir(root, 'saved');
    let n = 0;
    fs.mkdirSync(to, { recursive: true });
    for (const f of files) {
        if (!readHeader(f))
            continue;
        let name = path.basename(f).replace(/[^\w .()'+-]/g, '_');
        if (!/\.umkr$/i.test(name))
            name += '.umkr';
        fs.copyFileSync(f, path.join(to, freeName(to, name)));
        n++;
    }
    return n;
}

function filePath(root, folder, name) {
    return FOLDERS.includes(folder) && safeName(name) ? path.join(dir(root, folder), name) : null;
}

function remove(root, folder, name) {
    const p = filePath(root, folder, name);
    if (!p || !fs.existsSync(p))
        return false;
    fs.unlinkSync(p);
    return true;
}

module.exports = { list, keep, importFiles, filePath, remove, readHeader, dir, FOLDERS };
