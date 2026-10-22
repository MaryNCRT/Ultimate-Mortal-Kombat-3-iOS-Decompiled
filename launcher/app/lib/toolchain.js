'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const https = require('https');
const crypto = require('crypto');
const { spawn, execSync } = require('child_process');
const { openZip } = require('./extract/zip');

const ZIG_VERSION = '0.13.0';

const TOOLCHAINS = {
    'x86_64-linux': {
        url: 'https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz',
        sha256: 'd45312e61ebcc48032b77bc4cf7fd6915c11fa16e4aad116b66c9468211230ea',
        folderName: 'zig-linux-x86_64-0.13.0',
        exeName: 'zig',
        format: 'tar.xz',
    },
    'x86_64-windows': {
        url: 'https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip',
        sha256: 'd859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158',
        folderName: 'zig-windows-x86_64-0.13.0',
        exeName: 'zig.exe',
        format: 'zip',
    },
};

function getToolchainBaseDir() {
    if (process.env.UMK3_TOOLCHAIN_DIR) {
        return process.env.UMK3_TOOLCHAIN_DIR;
    }

    if (process.platform === 'win32') {
        const localAppData = process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local');
        return path.join(localAppData, 'umk3-launcher', 'toolchain');
    }

    const xdgData = process.env.XDG_DATA_HOME || path.join(os.homedir(), '.local', 'share');
    return path.join(xdgData, 'umk3-launcher', 'toolchain');
}

function getPlatformKey() {
    if (process.platform === 'win32') {
        return 'x86_64-windows';
    }
    return 'x86_64-linux';
}

function findExistingZig() {
    // 1. Explicit env var
    if (process.env.ZIG_EXE && fs.existsSync(process.env.ZIG_EXE)) {
        return process.env.ZIG_EXE;
    }

    // 2. Installed in toolchain directory
    const info = TOOLCHAINS[getPlatformKey()];
    if (info) {
        const cachedExe = path.join(getToolchainBaseDir(), info.folderName, info.exeName);
        if (fs.existsSync(cachedExe)) {
            return cachedExe;
        }
    }

    // 3. Local test / scratch path
    const scratchZig = '/tmp/zig-test/zig';
    if (process.platform !== 'win32' && fs.existsSync(scratchZig)) {
        return scratchZig;
    }

    // 4. System PATH
    try {
        const cmd = process.platform === 'win32' ? 'where zig' : 'which zig';
        const sysPath = execSync(cmd, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim().split(/\r?\n/)[0];
        if (sysPath && fs.existsSync(sysPath)) {
            const ver = execSync(`"${sysPath}" version`, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
            if (ver.startsWith('0.12.') || ver.startsWith('0.13.')) {
                return sysPath;
            }
        }
    } catch (_) {
        // Not in PATH
    }

    return null;
}

/* Node tries each of the server's addresses (IPv6, then IPv4) for only
 * 250 ms by default and, when every one runs out, fails with an
 * AggregateError whose message is empty: on Linux the build stopped with a
 * bare "Build failed:". Each address gets 5 s here, and a second try uses
 * IPv4 alone. */
const CONNECT = { autoSelectFamily: true, autoSelectFamilyAttemptTimeout: 5000 };
const CONNECT_V4 = { family: 4 };

/* the error's own text, or its parts' when it is an AggregateError */
function describeError(err) {
    if (err && err.message) return err.message;
    const parts = (err && err.errors || []).map(e => e.message || e.code).filter(Boolean);
    return parts.length ? parts.join('; ') : String((err && err.code) || err);
}

async function downloadFile(url, destPath, expectedSha, onProgress = () => {}) {
    try {
        return await downloadOnce(url, destPath, expectedSha, onProgress, CONNECT);
    } catch (err) {
        if (err.httpStatus || err.checksum) throw err;
        onProgress({ phase: `Download failed (${describeError(err)}), retrying over IPv4`, pct: 2 });
        try {
            return await downloadOnce(url, destPath, expectedSha, onProgress, CONNECT_V4);
        } catch (err2) {
            throw new Error(`Could not download Zig ${ZIG_VERSION} from ${url}: ${describeError(err2)}`);
        }
    }
}

function downloadOnce(url, destPath, expectedSha, onProgress, connect) {
    return new Promise((resolve, reject) => {
        fs.mkdirSync(path.dirname(destPath), { recursive: true });
        const fileStream = fs.createWriteStream(destPath);
        const hash = crypto.createHash('sha256');

        const request = (reqUrl) => {
            https.get(reqUrl, connect, res => {
                // Follow redirects (HTTP 301, 302, 307, 308), relative ones too
                if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                    res.resume();
                    return request(new URL(res.headers.location, reqUrl).href);
                }

                if (res.statusCode !== 200) {
                    fileStream.close();
                    fs.unlinkSync(destPath);
                    return reject(Object.assign(new Error(`Error downloading toolchain: HTTP ${res.statusCode}`), { httpStatus: res.statusCode }));
                }

                const totalBytes = parseInt(res.headers['content-length'] || '0', 10);
                let downloadedBytes = 0;

                res.on('data', chunk => {
                    downloadedBytes += chunk.length;
                    hash.update(chunk);
                    fileStream.write(chunk);

                    if (totalBytes > 0) {
                        const pct = Math.round((downloadedBytes / totalBytes) * 100);
                        onProgress({
                            phase: `Downloading Zig ${ZIG_VERSION} (${Math.round(downloadedBytes / 1048576)} MB / ${Math.round(totalBytes / 1048576)} MB)`,
                            pct,
                            downloaded: downloadedBytes,
                            total: totalBytes,
                        });
                    }
                });

                res.on('end', () => {
                    fileStream.end(() => {
                        const actualSha = hash.digest('hex');
                        if (expectedSha && actualSha.toLowerCase() !== expectedSha.toLowerCase()) {
                            fs.unlinkSync(destPath);
                            return reject(Object.assign(new Error(`Zig SHA-256 checksum mismatch: expected ${expectedSha}, got ${actualSha}`), { checksum: true }));
                        }
                        resolve(destPath);
                    });
                });

                res.on('error', err => {
                    fileStream.close();
                    try { fs.unlinkSync(destPath); } catch (_) {}
                    reject(err);
                });
            }).on('error', err => {
                fileStream.close();
                try { fs.unlinkSync(destPath); } catch (_) {}
                reject(err);
            });
        };

        request(url);
    });
}

function extractArchive(archivePath, targetDir, format) {
    fs.mkdirSync(targetDir, { recursive: true });

    if (format === 'zip') {
        const zip = openZip(archivePath);
        try {
            for (const name of zip.namelist()) {
                if (name.endsWith('/')) continue;
                const outPath = path.join(targetDir, ...name.split('/'));
                fs.mkdirSync(path.dirname(outPath), { recursive: true });
                fs.writeFileSync(outPath, zip.read(name));
            }
        } finally {
            zip.close();
        }
    } else if (format === 'tar.xz') {
        execSync(`tar -xf "${archivePath}" -C "${targetDir}"`, { stdio: 'inherit' });
    } else {
        throw new Error(`Unsupported archive format: ${format}`);
    }
}

/**
 * Ensures a working Zig compiler is available.
 * Returns the absolute path to the zig executable.
 */
async function ensureToolchain(onProgress = () => {}) {
    const existing = findExistingZig();
    if (existing) {
        onProgress({ phase: 'Compiler ready', pct: 10 });
        return existing;
    }

    const platformKey = getPlatformKey();
    const info = TOOLCHAINS[platformKey];
    if (!info) {
        throw new Error(`Platform ${process.platform} (${process.arch}) is not automatically supported.`);
    }

    const baseDir = getToolchainBaseDir();
    const archiveName = path.basename(info.url);
    const archivePath = path.join(baseDir, archiveName);

    onProgress({ phase: `Starting download of Zig ${ZIG_VERSION} compiler`, pct: 2 });
    await downloadFile(info.url, archivePath, info.sha256, onProgress);

    onProgress({ phase: 'Extracting compiler...', pct: 8 });
    extractArchive(archivePath, baseDir, info.format);

    // Delete archive after extraction to save disk space
    try {
        fs.unlinkSync(archivePath);
    } catch (_) {}

    const zigExe = path.join(baseDir, info.folderName, info.exeName);
    if (!fs.existsSync(zigExe)) {
        throw new Error(`Zig executable not found after extraction: ${zigExe}`);
    }

    if (process.platform !== 'win32') {
        fs.chmodSync(zigExe, 0o755);
    }

    onProgress({ phase: 'Compiler installed successfully', pct: 10 });
    return zigExe;
}

module.exports = {
    ensureToolchain,
    describeError,
    downloadFile,
    findExistingZig,
    getToolchainBaseDir,
    ZIG_VERSION,
};
