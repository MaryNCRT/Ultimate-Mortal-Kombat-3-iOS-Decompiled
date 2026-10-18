/* Updates from the repository's GitHub releases. Port tooling, nothing of the
 * game: a release zip holds the port's sources and the launcher, never game
 * data, so after an update the game is compiled again from the player's .ipa.
 *
 *   check(current)        the newest release (pre-releases count: every
 *                         release so far is one), its notes, and whether it
 *                         is newer than `current`
 *   download(rel, dir, onProgress)
 *                         the release's zip into <root>/update/, its SHA-256
 *                         checked against the one GitHub states
 *   install(root, zip, rel)
 *                         unpacks it, writes update/apply.ps1 and starts it:
 *                         the script waits for this launcher to close, copies
 *                         the new files over the folder and starts
 *                         UMK3-Launcher.exe again, which then recompiles
 *
 * What an update leaves alone: umk3.ini, res\, the compiled umk3-game.exe
 * (rebuilt right after), toolchain\, build-game\ and anything the player put
 * in the folder. decomp\, runtime\, tools\ and launcher\ are mirrored, so a
 * source the new release dropped does not stay behind to be compiled.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { spawn, spawnSync } = require('child_process');

const REPO = 'MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp';
const API = 'https://api.github.com/repos/' + REPO + '/releases?per_page=10';
const MIRROR = ['src', 'decomp', 'runtime', 'tools', 'launcher'];

/* "v0.0.10-alpha", "0.0.7b-alpha" -> [[0,''],[0,''],[10,'']] / [..,[7,'b']] */
function parseVersion(v) {
    const core = String(v || '').trim().replace(/^v/i, '').split('-')[0];
    return core.split('.').map(p => {
        const m = p.match(/^(\d+)([a-z]*)$/i);
        return m ? [parseInt(m[1], 10), m[2].toLowerCase()] : [0, ''];
    });
}

/* >0 when a is newer than b */
function compareVersions(a, b) {
    const x = parseVersion(a), y = parseVersion(b);
    for (let i = 0; i < Math.max(x.length, y.length); i++) {
        const p = x[i] || [0, ''], q = y[i] || [0, ''];
        if (p[0] !== q[0]) return p[0] - q[0];
        if (p[1] !== q[1]) return p[1] < q[1] ? -1 : 1;
    }
    return 0;
}

/* The newest published release with a .zip, or null. `fetchFn` is Electron's
 * net.fetch (system proxy, redirects) or anything with the same shape. */
async function check(fetchFn, current) {
    const r = await fetchFn(API, { headers: { 'User-Agent': 'UMK3-Launcher', Accept: 'application/vnd.github+json' } });
    if (!r.ok)
        throw new Error('GitHub ' + r.status);
    const list = await r.json();
    for (const rel of list) {
        if (rel.draft)
            continue;
        const zip = (rel.assets || []).find(a => /\.zip$/i.test(a.name));
        if (!zip)
            continue;
        return {
            tag: rel.tag_name,
            name: rel.name || rel.tag_name,
            notes: rel.body || '',
            date: rel.published_at,
            page: rel.html_url,
            url: zip.browser_download_url,
            file: zip.name,
            size: zip.size,
            sha256: /^sha256:[0-9a-f]{64}$/i.test(zip.digest || '') ? zip.digest.slice(7).toLowerCase() : '',
            current,
            newer: compareVersions(rel.tag_name, current) > 0,
        };
    }
    return null;
}

/* Streams the zip to dir/<file>; resolves with its path. */
async function download(fetchFn, rel, dir, onProgress) {
    if (!/^[\w.\-]+\.zip$/i.test(rel.file))
        throw new Error('unexpected file name');
    fs.mkdirSync(dir, { recursive: true });
    const out = path.join(dir, rel.file);
    const part = out + '.part';
    const r = await fetchFn(rel.url, { headers: { 'User-Agent': 'UMK3-Launcher' } });
    if (!r.ok || !r.body)
        throw new Error('download: HTTP ' + r.status);
    const total = Number(r.headers.get('content-length')) || rel.size || 0;
    const hash = crypto.createHash('sha256');
    const fd = fs.openSync(part, 'w');
    let got = 0;
    try {
        const reader = r.body.getReader();
        for (;;) {
            const { done, value } = await reader.read();
            if (done) break;
            fs.writeSync(fd, value);
            hash.update(value);
            got += value.length;
            if (onProgress) onProgress(got, total);
        }
    } finally {
        fs.closeSync(fd);
    }
    const sum = hash.digest('hex');
    if (rel.sha256 && sum !== rel.sha256) {
        fs.rmSync(part, { force: true });
        throw new Error('download SHA-256 does not match GitHub digest');
    }
    fs.renameSync(part, out);
    return out;
}

function psq(s) {
    return "'" + String(s).replace(/'/g, "''") + "'";
}

/* Unpacks zip into <root>/update/new and returns the release folder in it. */
function unpack(root, zip) {
    const dest = path.join(root, 'update', 'new');
    fs.rmSync(dest, { recursive: true, force: true });
    fs.mkdirSync(dest, { recursive: true });
    const tar = path.join(process.env.SystemRoot || 'C:\\Windows', 'System32', 'tar.exe');
    let r = fs.existsSync(tar) ? spawnSync(tar, ['-xf', zip, '-C', dest], { windowsHide: true }) : { status: 1 };
    if (r.status !== 0)
        r = spawnSync('powershell.exe', ['-NoProfile', '-Command',
            'Expand-Archive -LiteralPath ' + psq(zip) + ' -DestinationPath ' + psq(dest) + ' -Force'],
            { windowsHide: true });
    if (r.status !== 0)
        throw new Error('could not unpack the update');
    /* the zip holds one folder, UMK3-PC-<version>; up to 0.0.9 the launcher
     * was one portable UMK3-Launcher.exe, since then a stub over launcher\bin */
    const cands = [dest].concat(fs.readdirSync(dest).map(n => path.join(dest, n)));
    const src = cands.find(d => (fs.existsSync(path.join(d, 'launcher')) || fs.existsSync(path.join(d, 'src')))
                             && (fs.existsSync(path.join(d, 'UMK3-Launcher.exe')) || fs.existsSync(path.join(d, 'UMK3-Launcher'))));
    if (!src)
        throw new Error('the update archive structure is unexpected');
    return src;
}

/* Writes update/apply.ps1, starts it detached and returns; the caller quits. */
function install(root, zip, rel) {
    const src = unpack(root, zip);
    const upd = path.join(root, 'update');
    fs.writeFileSync(path.join(upd, 'pending.json'), JSON.stringify({
        tag: rel.tag, name: rel.name, notes: rel.notes, from: rel.current,
    }));
    const ps = [
        '$ErrorActionPreference = "Continue"',
        '$root = ' + psq(root),
        '$src = ' + psq(src),
        '$log = Join-Path $root "update\\apply.log"',
        '"update " + (Get-Date) | Out-File $log -Encoding utf8',
        /* this launcher and its helper processes, all from launcher\bin */
        'try { Wait-Process -Id ' + process.pid + ' -Timeout 60 -ErrorAction SilentlyContinue } catch {}',
        '$bin = Join-Path $root "launcher\\bin"',
        'for ($i = 0; $i -lt 60; $i++) {',
        '  $left = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($bin, "OrdinalIgnoreCase") }',
        '  if (-not $left) { break }',
        '  Start-Sleep -Milliseconds 500',
        '}',
        '$fail = $false',
        'foreach ($d in @(' + MIRROR.map(psq).join(', ') + ')) {',
        '  & robocopy (Join-Path $src $d) (Join-Path $root $d) /MIR /R:5 /W:1 /NFL /NDL /NJH /NJS /NP | Out-File $log -Append -Encoding utf8',
        '  if ($LASTEXITCODE -ge 8) { $fail = $true; "robocopy $d -> $LASTEXITCODE" | Out-File $log -Append -Encoding utf8 }',
        '}',
        '& robocopy $src $root /R:5 /W:1 /NFL /NDL /NJH /NJS /NP | Out-File $log -Append -Encoding utf8',
        'if ($LASTEXITCODE -ge 8) { $fail = $true; "robocopy top -> $LASTEXITCODE" | Out-File $log -Append -Encoding utf8 }',
        'if ($fail) { Rename-Item (Join-Path $root "update\\pending.json") "failed.json" -Force }',
        'else { Rename-Item (Join-Path $root "update\\pending.json") "installed.json" -Force',
        '       Remove-Item (Join-Path $root "update\\new") -Recurse -Force -ErrorAction SilentlyContinue',
        '       Remove-Item ' + psq(zip) + ' -Force -ErrorAction SilentlyContinue }',
        'Start-Process -FilePath (Join-Path $root "UMK3-Launcher.exe") -WorkingDirectory $root',
    ].join('\r\n');
    const script = path.join(upd, 'apply.ps1');
    fs.writeFileSync(script, '\ufeff' + ps, 'utf8');
    /* Started through WMI, so it is nobody's child: a process the launcher
     * spawns itself can share its job and die with it. A detached spawn is
     * the fallback. */
    const line = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + script + '"';
    const wmi = spawnSync('powershell.exe', ['-NoProfile', '-Command',
        '$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '
        + psq(line) + '; CurrentDirectory = ' + psq(upd) + ' }; exit $r.ReturnValue'],
        { windowsHide: true });
    if (wmi.status !== 0) {
        const child = spawn('powershell.exe',
            ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', script],
            { cwd: upd, detached: true, stdio: 'ignore', windowsHide: true });
        child.unref();
    }
}

/* After a restart: what was installed (or failed), once; null otherwise. */
function takeResult(root) {
    for (const [f, ok] of [['installed.json', true], ['failed.json', false]]) {
        const p = path.join(root, 'update', f);
        if (!fs.existsSync(p))
            continue;
        let info = {};
        try { info = JSON.parse(fs.readFileSync(p, 'utf8')); } catch (e) { /* keep {} */ }
        fs.rmSync(p, { force: true });
        return Object.assign(info, { ok });
    }
    return null;
}

module.exports = { parseVersion, compareVersions, check, download, install, takeResult, REPO };
