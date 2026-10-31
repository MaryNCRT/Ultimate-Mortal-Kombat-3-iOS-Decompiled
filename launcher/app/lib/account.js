/* account.js -- the player's account: log in, and the profile and the
 * game's progress kept the same on every computer. Port, not in the binary.
 * The server is a Cloudflare Worker kept in a private repository; only this
 * client is public. It speaks JSON over HTTPS: /v1/register, /v1/login,
 * /v1/recover, /v1/logout, /v1/me, /v1/password, /v1/email, /v1/progress
 * (GET, and PUT with the version it builds on: 409 when the cloud moved),
 * /v1/scores (the game's leaderboards: boards() and flushScores() below),
 * /v1/profile, /v1/users and /v1/friends (claim() and the friends below).
 *
 * Without an account the player is a GUEST: the profile code alone, as
 * before, for playing online, and no leaderboards.
 *
 * account.ini, beside umk3.ini, keeps the login:
 *
 *     server=https://umk3-accounts.example.workers.dev   (absent: SERVER)
 *     user=Mary
 *     token=enc:<base64>      the session, sealed by the system (Electron's
 *                             safeStorage: DPAPI on Windows, the keyring on
 *                             Linux); plain:<token> only where there is none
 *     version=7               the cloud's version this computer last had
 *     hash=<sha256>           the local progress as it was then
 *
 * The password and the recovery code are never written anywhere here.
 *
 * What is synced: the profiles.ini line, and the save files that are the
 * game's progress (PROGRESS below; the button layouts and the settings stay
 * on each computer). The rule, with `version` and `hash`:
 *   pull  (before playing) the cloud is newer and this computer has not
 *         changed since: the cloud's progress is taken. Both changed: a
 *         conflict, which the player settles (THIS COMPUTER or THE CLOUD).
 *   push  (after playing)  this computer changed: uploaded on top of
 *         `version`; the cloud moved meanwhile: a conflict.
 * Whatever is replaced is copied first to save/backup/<date>/. */
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const prof = require('./profiles');

/* the public server, set once it is deployed (server/accounts/README.md) */
const SERVER = 'https://umk3-accounts.umk3-accounts.workers.dev';

const PROGRESS = ['savedata', 'achievementsdata', 'statsdata', 'survival', 'towerdata',
                  'unclaimedtreasure', 'unclaimedtreasure_lastdestiny', 'unclaimedtreasure_playedas'];

/* Electron's safeStorage, handed in by main.js (none in the tests) */
let g_vault = null;
function setVault(v) { g_vault = v; }

/* ------------------------------------------------------------- files */

function iniPath(root) { return path.join(root, 'account.ini'); }

function readState(root) {
    const s = {};
    try {
        for (const line of fs.readFileSync(iniPath(root), 'utf8').split(/\r?\n/)) {
            const m = /^([a-z]+)=(.*)$/.exec(line.trim());
            if (m) s[m[1]] = m[2];
        }
    } catch { /* none yet */ }
    s.version = Number(s.version) || 0;
    return s;
}

function writeState(root, s) {
    const lines = ['; UMK3 account: written by the launcher. No password is kept here.'];
    for (const k of ['server', 'user', 'token', 'version', 'hash'])
        if (s[k] !== undefined && s[k] !== '' && s[k] !== null) lines.push(`${k}=${s[k]}`);
    fs.writeFileSync(iniPath(root), lines.join('\n') + '\n', 'utf8');
}

function sealToken(t) {
    if (g_vault && g_vault.isEncryptionAvailable())
        return 'enc:' + g_vault.encryptString(t).toString('base64');
    return 'plain:' + t;
}

function openToken(s) {
    if (!s) return '';
    if (s.startsWith('plain:')) return s.slice(6);
    if (s.startsWith('enc:') && g_vault && g_vault.isEncryptionAvailable()) {
        try {
            return g_vault.decryptString(Buffer.from(s.slice(4), 'base64'));
        } catch { return ''; }
    }
    return '';
}

/* the game's save folder: lime_menu.c's save_dir */
function saveDir(root) {
    if (process.env.UMK3_SAVE_DIR) return process.env.UMK3_SAVE_DIR;
    if (process.platform === 'win32') return path.join(root, 'save');
    return path.join(process.env.XDG_DATA_HOME || path.join(os.homedir(), '.local', 'share'), 'umk3');
}

function profileLine(root) {
    try {
        const l = fs.readFileSync(path.join(root, 'profiles.ini'), 'utf8').split(/\r?\n/)
            .find(x => x.startsWith('profile='));
        return l || null;
    } catch { return null; }
}

function readLocal(root) {
    const dir = saveDir(root), save = {};
    for (const f of PROGRESS) {
        try { save[f] = fs.readFileSync(path.join(dir, f)).toString('base64'); } catch { /* none */ }
    }
    return { profile: profileLine(root), save };
}

function hashOf(local) {
    const h = crypto.createHash('sha256');
    h.update(local.profile || '');
    for (const f of PROGRESS) h.update(`\0${f}\0${local.save[f] || ''}`);
    return h.digest('hex');
}

function backup(root) {
    const dir = saveDir(root);
    const to = path.join(dir, 'backup', new Date().toISOString().replace(/[:.]/g, '-'));
    let any = false;
    for (const f of PROGRESS) {
        const from = path.join(dir, f);
        if (!fs.existsSync(from)) continue;
        fs.mkdirSync(to, { recursive: true });
        fs.copyFileSync(from, path.join(to, f));
        any = true;
    }
    const prof = path.join(root, 'profiles.ini');
    if (fs.existsSync(prof)) {
        fs.mkdirSync(to, { recursive: true });
        fs.copyFileSync(prof, path.join(to, 'profiles.ini'));
        any = true;
    }
    return any ? to : null;
}

/* the cloud's progress onto this computer */
function applyCloud(root, cloud) {
    backup(root);
    const dir = saveDir(root);
    fs.mkdirSync(dir, { recursive: true });
    const save = cloud.save || {};
    for (const f of PROGRESS) {
        const p = path.join(dir, f);
        if (typeof save[f] === 'string') fs.writeFileSync(p, Buffer.from(save[f], 'base64'));
        else if (fs.existsSync(p)) fs.unlinkSync(p);
    }
    if (cloud.profile && /^profile=[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{4}\|/.test(cloud.profile)) {
        fs.writeFileSync(path.join(root, 'profiles.ini'),
            '; UMK3 player profile: the launcher and the game both edit this;\n'
            + '; the id follows the account it came from\n' + cloud.profile + '\n', 'utf8');
    }
}

/* -------------------------------------------------------------- http */

function server(root) {
    return (readState(root).server || process.env.UMK3_ACCOUNT_SERVER || SERVER).replace(/\/+$/, '');
}

async function api(root, method, p, body, token) {
    const base = server(root);
    if (!base) return { ok: false, error: 'no_server' };
    const headers = { 'content-type': 'application/json' };
    if (token) headers.authorization = 'Bearer ' + token;
    let r;
    try {
        r = await fetch(base + p, { method, headers, body: body ? JSON.stringify(body) : undefined,
                                    signal: AbortSignal.timeout(15000) });
    } catch {
        return { ok: false, error: 'offline' };
    }
    let j = {};
    try { j = await r.json(); } catch { /* empty */ }
    return { ...j, ok: r.ok, status: r.status, error: r.ok ? undefined : j.error || 'server_error' };
}

function session(root) {
    const s = readState(root);
    return { s, token: openToken(s.token) };
}

/* ------------------------------------------------------------ public */

function status(root) {
    const { s, token } = session(root);
    return { server: !!server(root), user: token ? s.user : '', guest: !token };
}

async function loggedIn(root, r) {
    if (!r.ok) return r;
    const s = readState(root);
    s.user = r.username;
    s.token = sealToken(r.token);
    s.version = 0;                      /* a new account or computer: sync from scratch */
    s.hash = '';
    writeState(root, s);
    const sync = await pull(root, true);
    return { ok: true, user: r.username, recovery: r.recovery, sync };
}

async function register(root, f) {
    return loggedIn(root, await api(root, 'POST', '/v1/register',
        { username: f.username, password: f.password, email: f.email || undefined }));
}

async function login(root, f) {
    return loggedIn(root, await api(root, 'POST', '/v1/login', { username: f.username, password: f.password }));
}

async function recover(root, f) {
    return loggedIn(root, await api(root, 'POST', '/v1/recover',
        { username: f.username, recovery: f.recovery, password: f.password }));
}

async function logout(root) {
    const { s, token } = session(root);
    if (token) await api(root, 'POST', '/v1/logout', null, token);
    writeState(root, { server: s.server });
    fs.rmSync(path.join(root, 'leaderboards.txt'), { force: true });
    return { ok: true };
}

async function me(root) {
    const { token } = session(root);
    if (!token) return { ok: false, error: 'not_logged_in' };
    return api(root, 'GET', '/v1/me', null, token);
}

async function setEmail(root, email) {
    const { token } = session(root);
    return api(root, 'POST', '/v1/email', { email }, token);
}

async function setPassword(root, password, next) {
    const { token } = session(root);
    return api(root, 'POST', '/v1/password', { password, new: next }, token);
}

async function remove(root, password) {
    const { s, token } = session(root);
    const r = await api(root, 'DELETE', '/v1/me', { password }, token);
    if (r.ok) {
        writeState(root, { server: s.server });
        fs.rmSync(path.join(root, 'leaderboards.txt'), { force: true });
    }
    return r;
}

/* first: just logged in -- an empty cloud takes this computer's progress,
 * a full one is taken unless this computer has progress of its own (then
 * the player chooses) */
async function pullProgress(root, first = false) {
    const { s, token } = session(root);
    if (!token) return { ok: true, guest: true };
    const cloud = await api(root, 'GET', '/v1/progress', null, token);
    if (!cloud.ok) return cloud;
    const local = readLocal(root), h = hashOf(local);
    if (cloud.version === 0) return push(root);
    if (cloud.version === s.version && h === s.hash) return { ok: true, same: true };
    const localChanged = first ? Object.keys(local.save).length > 0 : h !== s.hash;
    if (cloud.version > s.version && localChanged)
        return { ok: false, error: 'conflict', cloud: cloud.updated };
    if (cloud.version > s.version) {
        applyCloud(root, cloud);
        const st = readState(root);
        st.version = cloud.version;
        st.hash = hashOf(readLocal(root));
        writeState(root, st);
        return { ok: true, took: 'cloud' };
    }
    return push(root);
}

async function push(root, force = false) {
    const { s, token } = session(root);
    if (!token) return { ok: true, guest: true };
    const local = readLocal(root), h = hashOf(local);
    if (!force && h === s.hash && s.version) return { ok: true, same: true };
    let base = s.version;
    if (force) {
        const cloud = await api(root, 'GET', '/v1/progress', null, token);
        if (!cloud.ok) return cloud;
        base = cloud.version;
    }
    const r = await api(root, 'PUT', '/v1/progress', { base, profile: local.profile, save: local.save }, token);
    if (r.status === 409) return { ok: false, error: 'conflict' };
    if (!r.ok) return r;
    const st = readState(root);
    st.version = r.version;
    st.hash = h;
    writeState(root, st);
    return { ok: true, took: 'local' };
}

/* the progress, then the profile's code and name on the account */
async function pull(root, first = false) {
    const r = await pullProgress(root, first);
    if (r.ok && !r.guest) await claim(root).catch(() => {});
    return r;
}

/* a conflict settled: 'local' uploads this computer's, 'cloud' takes the
 * cloud's (this computer's copied to save/backup first) */
async function resolve(root, which) {
    if (which === 'local') return push(root, true);
    const st = readState(root);
    st.version = 0;
    st.hash = hashOf(readLocal(root));
    writeState(root, st);
    return pull(root);
}

async function scores(root, board, limit = 50) {
    return api(root, 'GET', `/v1/scores/${encodeURIComponent(board)}?limit=${limit}`);
}

async function postScore(root, board, score, name) {
    const { token } = session(root);
    if (!token) return { ok: false, error: 'guest' };
    return api(root, 'POST', '/v1/scores', { board, score, name }, token);
}

/* ------------------------------------------------- the game's leaderboards
 * The game has no HTTPS; it reads the boards from leaderboards.txt and
 * leaves its scores in scores.txt (src/runtime/gamecode_stubs.c). Only an
 * account gets the file -- without it the game is offline, and a guest has
 * no leaderboards. */

const BOARDS = ['shaokahn_med', 'survival_easy'];

/* the profile's name (profiles.ini: profile=<id>|<name>|...), which is what
 * the boards show; the account's username when there is none */
function profileName(root, fallback) {
    const l = profileLine(root);
    const n = l ? l.slice(8).split('|')[1] || '' : '';
    return n.replace(/[^\x20-\x7e]/g, '').trim().slice(0, 24) || fallback;
}

/* before the game starts: the boards down. Offline, the last ones stay. */
async function boards(root) {
    const file = path.join(root, 'leaderboards.txt');
    const { s, token } = session(root);
    if (!token) {
        fs.rmSync(file, { force: true });
        return { ok: true, guest: true };
    }
    const lines = ['; UMK3 leaderboards: written by the launcher before each game',
                   `user=${profileName(root, s.user)}`];
    for (const b of BOARDS) {
        const r = await scores(root, b, 100);
        if (!r.ok) return r;
        lines.push(`board=${b}`);
        for (const row of r.rows || [])
            lines.push(`${String(row.name || row.username).replace(/[|\r\n]/g, '')}|${Number(row.score) || 0}`);
    }
    /* whole or not at all: the game may be reading it */
    fs.writeFileSync(file + '.tmp', lines.join('\n') + '\n', 'utf8');
    try {
        fs.renameSync(file + '.tmp', file);
    } catch {
        fs.rmSync(file + '.tmp', { force: true });
    }
    return { ok: true };
}

/* while the game runs: its new scores up and everyone's boards down, so a
 * run shows on the other players' boards as soon as it ends */
async function live(root) {
    await flushScores(root);
    return boards(root);
}

/* the game's scores up (live() while it runs, and once more after). What
 * did not go stays for next time. */
async function flushScores(root) {
    const file = path.join(root, 'scores.txt');
    let text;
    try { text = fs.readFileSync(file, 'utf8'); } catch { return { ok: true, sent: 0 }; }
    const { s, token } = session(root);
    if (!token) return { ok: true, guest: true };
    const name = profileName(root, s.user);
    const best = new Map();
    for (const line of text.split(/\r?\n/)) {
        const m = /^([a-z0-9_]{1,32}) (\d{1,15})$/.exec(line.trim());
        if (m) best.set(m[1], Math.max(best.get(m[1]) || 0, Number(m[2])));
    }
    const left = [];
    for (const [b, v] of best) {
        const r = await postScore(root, b, v, name);
        if (!r.ok && r.error !== 'bad_board' && r.error !== 'bad_score') left.push(`${b} ${v}`);
    }
    if (left.length) fs.writeFileSync(file, left.join('\n') + '\n', 'utf8');
    else fs.rmSync(file, { force: true });
    return { ok: !left.length, sent: best.size - left.length };
}

/* ------------------------------------------------- profile and friends
 * The profile's code and name belong to the account (PUT /v1/profile). The
 * code is stored the first time and every computer takes it back; one that
 * is another account's already (two accounts on one computer) is replaced
 * by a new one. The name is one per account: a name another player has is
 * not stored, and one changed in the game goes back to the stored one. */
async function claim(root, name) {
    const { token } = session(root);
    if (!token) return { ok: true, guest: true };
    const p = prof.load(root), before = p.id + '|' + p.name;
    const want = name === undefined ? p.name : name;
    let r = await api(root, 'PUT', '/v1/profile', { code: p.id, name: want }, token);
    if (r.error === 'code_taken') {
        p.id = prof.newId();
        r = await api(root, 'PUT', '/v1/profile', { code: p.id, name: want }, token);
    }
    if (!r.ok) return r;
    p.id = r.code;
    if (name === undefined && r.taken && r.name) p.name = r.name;
    if (p.id + '|' + p.name !== before) {
        prof.save(root, p);
        await push(root);
    }
    return { ok: true, code: r.code, name: r.name, taken: r.taken };
}

/* the profile's name changed in the launcher: kept only when free */
async function rename(root, name) {
    const r = await claim(root, prof.cleanName(name).trim());
    if (r.ok && r.taken) return { ok: false, error: 'name_taken', name: r.name };
    return r;
}

function authedApi(root, method, p, body) {
    const { token } = session(root);
    if (!token) return Promise.resolve({ ok: false, error: 'not_logged_in' });
    return api(root, method, p, body, token);
}

async function friends(root) {
    let r = await authedApi(root, 'GET', '/v1/friends');
    if (r.ok && !r.code) {
        await claim(root);
        r = await authedApi(root, 'GET', '/v1/friends');
    }
    return r;
}

const findPlayers = (root, q) =>
    authedApi(root, 'GET', '/v1/users?q=' + encodeURIComponent(String(q || '').trim().slice(0, 24)));
const addFriend = (root, code) => authedApi(root, 'POST', '/v1/friends', { code });
const removeFriend = (root, code) => authedApi(root, 'DELETE', '/v1/friends', { code });

module.exports = { setVault, status, register, login, recover, logout, me, setEmail, setPassword,
                   remove, pull, push, resolve, scores, postScore, boards, flushScores, live, saveDir,
                   claim, rename, friends, findPlayers, addFriend, removeFriend, PROGRESS, BOARDS };
