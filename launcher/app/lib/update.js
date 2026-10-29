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
 *                         unpacks it, writes update/apply.ps1 (Linux:
 *                         apply.sh) and starts it: the script waits for this
 *                         launcher to close, copies the new files over the
 *                         folder and starts UMK3-Launcher again, which then
 *                         recompiles
 *
 * Windows takes the release's .zip, Linux its linux .tar.gz -- the archive
 * of its own launcher. Up to 0.1.7 the Linux launcher took the .zip too and
 * could not unpack it ("could not unpack the update").
 *
 * What an update leaves alone: umk3.ini, res\, the compiled umk3-game.exe
 * (rebuilt right after), toolchain\, build-game\, tools\eos\ (the online SDK)
 * and anything the player put in the folder. src\, launcher\ and
 * tools\build\ are mirrored, so a source the new release dropped does not
 * stay behind to be compiled; the release's other files are copied over
 * without deleting anything, and a mirrored folder the release lacks is
 * skipped. Up to 0.1.10b the list held decomp\ and runtime\, which no
 * release has had since 0.1.6: robocopy answered 16 for them and every
 * update ended as "could not be copied". tools\ was mirrored whole, which
 * deleted the EOS SDK and eos.ini an earlier release had put in tools\eos\.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { spawn, spawnSync } = require('child_process');

const REPO = 'MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Recomp';
const API = 'https://api.github.com/repos/' + REPO + '/releases?per_page=10';
const MIRROR = ['src', 'launcher', 'tools\\build'];
const LINUX = process.platform === 'linux';

/* this system's archive among a release's files */
const assetFor = assets => (assets || []).find(a => (LINUX
    ? /linux.*\.tar\.gz$/i.test(a.name)
    : /\.zip$/i.test(a.name) && !/linux/i.test(a.name)));

/* "v0.0.10-alpha", "0.0.7b-alpha" -> [[0,''],[0,''],[10,'']] / [..,[7,'b']]
 * The launcher's own "0.1.10-beta.b" (package.json must be semver) reads as "0.1.10b". */
function parseVersion(v) {
    const s = String(v || '').trim().replace(/^v/i, '');
    const letter = (s.match(/^[^-]*-[a-z]+\.([a-z])$/i) || [])[1] || '';
    const core = s.split('-')[0] + letter;
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

/* The newest published release with an archive for this system, or null. `fetchFn` is Electron's
 * net.fetch (system proxy, redirects) or anything with the same shape. */
async function check(fetchFn, current) {
    const r = await fetchFn(API, { headers: { 'User-Agent': 'UMK3-Launcher', Accept: 'application/vnd.github+json' } });
    if (!r.ok)
        throw new Error('GitHub ' + r.status);
    const list = await r.json();
    for (const rel of list) {
        if (rel.draft)
            continue;
        const zip = assetFor(rel.assets);
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
    if (!/^[\w.\-]+\.(zip|tar\.gz)$/i.test(rel.file))
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

/* A folder away, false when Windows will not let it go yet: a file in it
 * still open (an antivirus reading what was just unpacked, a launcher left
 * running from it) makes rmdir answer ENOTEMPTY or EBUSY. Two seconds of
 * retries. */
function removeDir(p) {
    try {
        fs.rmSync(p, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
        return true;
    } catch (e) {
        return false;
    }
}

/* Unpacks zip into <root>/update/new and returns { src: the release folder
 * in it, dest }. What an earlier try left there goes first; when Windows
 * holds on to it (Mary: "ENOTEMPTY: directory not empty, rmdir
 * ...\update\new\UMK3-PC-0.1.11-beta\launcher\bin\resources", which
 * stopped the update), this one unpacks into update/new-<time> instead and
 * the leftovers go at the next update. */
function unpack(root, zip) {
    const upd = path.join(root, 'update');
    fs.mkdirSync(upd, { recursive: true });
    for (const n of fs.readdirSync(upd))
        if (/^new(-\d+)?$/.test(n))
            removeDir(path.join(upd, n));
    let dest = path.join(upd, 'new');
    if (fs.existsSync(dest))
        dest = path.join(upd, 'new-' + Date.now());
    fs.mkdirSync(dest, { recursive: true });
    const tar = LINUX ? 'tar' : path.join(process.env.SystemRoot || 'C:\\Windows', 'System32', 'tar.exe');
    let r = LINUX || fs.existsSync(tar) ? spawnSync(tar, ['-xf', zip, '-C', dest], { windowsHide: true }) : { status: 1 };
    if (r.status !== 0 && !LINUX)
        r = spawnSync('powershell.exe', ['-NoProfile', '-Command',
            'Expand-Archive -LiteralPath ' + psq(zip) + ' -DestinationPath ' + psq(dest) + ' -Force'],
            { windowsHide: true });
    if (r.status !== 0)
        throw new Error('could not unpack the update');
    /* the zip holds one folder, UMK3-PC-<version>; up to 0.0.9 the launcher
     * was one portable UMK3-Launcher.exe, since then a stub over launcher\bin */
    const cands = [dest].concat(fs.readdirSync(dest).map(n => path.join(dest, n)));
    /* (Linux: the launcher's own folder, the sources in resources/src) */
    const src = cands.find(d => (fs.existsSync(path.join(d, 'launcher')) || fs.existsSync(path.join(d, 'src'))
                                 || fs.existsSync(path.join(d, 'resources', 'src')))
                             && (fs.existsSync(path.join(d, 'UMK3-Launcher.exe')) || fs.existsSync(path.join(d, 'UMK3-Launcher'))));
    if (!src)
        throw new Error('the update archive structure is unexpected');
    return { src, dest };
}

const shq = s => "'" + String(s).replace(/'/g, "'\\''") + "'";

/* Linux: the archive is the launcher's folder itself. Every folder in it is
 * mirrored (resources/ holds the sources, so a dropped one goes), its files
 * copied; what the player's folder has besides -- res/, umk3.ini, build/,
 * mods/, the compiled game -- is not in the archive and stays. */
function installLinux(root, src, dest, upd) {
    const sh = [
        '#!/bin/sh',
        'root=' + shq(root), 'src=' + shq(src), 'log="$root/update/apply.log"',
        'echo "update $(date)" > "$log"',
        'i=0; while kill -0 ' + process.pid + ' 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i+1)); done',
        'fail=0',
        'for d in "$src"/*/; do',
        '  n=$(basename "$d")',
        '  rm -rf "$root/$n.old"; [ -e "$root/$n" ] && mv "$root/$n" "$root/$n.old"',
        '  if cp -a "$d" "$root/$n" >>"$log" 2>&1; then rm -rf "$root/$n.old"',
        '  else fail=1; echo "copy $n failed" >>"$log"; rm -rf "$root/$n"; [ -e "$root/$n.old" ] && mv "$root/$n.old" "$root/$n"; fi',
        'done',
        'for f in "$src"/*; do [ -f "$f" ] && { cp -af "$f" "$root/" >>"$log" 2>&1 || fail=1; }; done',
        'chmod +x "$root/UMK3-Launcher" "$root/chrome_crashpad_handler" 2>/dev/null',
        'if [ $fail -ne 0 ]; then mv "$root/update/pending.json" "$root/update/failed.json"',
        'else mv "$root/update/pending.json" "$root/update/installed.json"; fi',
        'rm -rf ' + shq(dest) + ' "$root"/update/*.tar.gz',
        'cd "$root" && nohup "$root/UMK3-Launcher" >/dev/null 2>&1 &',
    ].join('\n') + '\n';
    const script = path.join(upd, 'apply.sh');
    fs.writeFileSync(script, sh, { mode: 0o755 });
    const child = spawn('/bin/sh', [script], { cwd: upd, detached: true, stdio: 'ignore' });
    child.unref();
}

/* Writes update/apply.ps1 (apply.sh), starts it detached and returns; the
 * caller quits. */
function install(root, zip, rel) {
    const { src, dest } = unpack(root, zip);
    const upd = path.join(root, 'update');
    fs.writeFileSync(path.join(upd, 'pending.json'), JSON.stringify({
        tag: rel.tag, name: rel.name, notes: rel.notes, from: rel.current,
    }));
    if (LINUX)
        return installLinux(root, src, dest, upd);
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
        '$mirrored = @()',
        'foreach ($d in @(' + MIRROR.map(psq).join(', ') + ')) {',
        '  $from = Join-Path $src $d',
        '  if (-not (Test-Path -LiteralPath $from -PathType Container)) { continue }',
        '  $mirrored += $from',
        '  & robocopy $from (Join-Path $root $d) /MIR /R:5 /W:1 /NFL /NDL /NJH /NJS /NP | Out-File $log -Append -Encoding utf8',
        '  if ($LASTEXITCODE -ge 8) { $fail = $true; "robocopy $d -> $LASTEXITCODE" | Out-File $log -Append -Encoding utf8 }',
        '}',
        /* the rest of the release (its top files, docs\, tools\eos\): copied, nothing deleted */
        '$rc = @($src, $root, "/E", "/R:5", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS", "/NP")',
        'if ($mirrored.Count) { $rc += "/XD"; $rc += $mirrored }',
        '& robocopy @rc | Out-File $log -Append -Encoding utf8',
        'if ($LASTEXITCODE -ge 8) { $fail = $true; "robocopy rest -> $LASTEXITCODE" | Out-File $log -Append -Encoding utf8 }',
        'if ($fail) { Rename-Item (Join-Path $root "update\\pending.json") "failed.json" -Force }',
        'else { Rename-Item (Join-Path $root "update\\pending.json") "installed.json" -Force }',
        /* the unpacked copy and every archive go either way: a retry downloads again */
        'Remove-Item -LiteralPath ' + psq(dest) + ' -Recurse -Force -ErrorAction SilentlyContinue',
        'Get-ChildItem (Join-Path $root "update") -Filter "*.zip" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue',
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

module.exports = { parseVersion, compareVersions, check, download, install, takeResult, unpack, REPO };
