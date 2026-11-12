/* The launcher's STEAM page behind the IPC: the shortcut in each Steam
 * account and the chosen artwork copied into it (lib/steam.js,
 * lib/artwork.js).  Pure Node, so the tests drive it with a fake Steam
 * folder; opts carries the overrides (platform, candidates, running). */
'use strict';

const path = require('path');
const steam = require('./steam');
const art = require('./artwork');

async function allAccounts(opts) {
    const installs = await steam.findInstalls(opts);
    return installs.flatMap(i => steam.accounts(i));
}

function pick(list, ref) {
    if (!ref) return null;
    return list.find(a => a.id === String(ref.id) && (!ref.install || a.install === ref.install)) || null;
}

async function status(root, opts = {}) {
    const target = steam.launcherTarget(root, opts);
    const list = await allAccounts(opts);
    const running = await steam.isSteamRunning(opts);
    const chosen = art.loadSettings(root).account || null;
    const accounts = list.map(a => {
        const s = steam.shortcutOf(a, target, opts);
        return {
            id: a.id, install: a.install, kind: a.kind, persona: a.persona, mostRecent: a.mostRecent,
            shortcut: s.shortcut, error: s.error || null,
            inSteam: s.shortcut ? steam.artworkInSteam(a, s.shortcut.appid) : null,
            appidIfAdded: target ? steam.shortcutAppId(target.exeField, steam.NAME) : null,
        };
    });
    let account = pick(accounts, chosen);
    if (!account) account = accounts.find(a => a.mostRecent) || accounts[0] || null;
    return {
        name: steam.NAME, running, target, accounts,
        account: account ? { id: account.id, install: account.install } : null,
        hasKey: !!art.apiKey(root), keySource: art.keySource(root), platform: opts.platform || process.platform,
    };
}

function chooseAccount(root, ref) {
    art.saveSettings(root, { account: ref ? { id: String(ref.id), install: ref.install } : null });
    return { ok: true };
}

async function resolve(root, ref, opts) {
    const list = await allAccounts(opts);
    const a = pick(list, ref);
    if (!a) return { error: 'noaccount' };
    const target = steam.launcherTarget(root, opts);
    return { a, target };
}

/* the icon field wanted for the files: the grid copy of the icon, or '' */
function iconFor(applied) { return applied.icon || ''; }

/* copies the chosen artwork into the account's grid folder; the icon field
 * of the shortcut is written only with Steam closed */
async function apply(root, ref, opts = {}) {
    const r = await resolve(root, ref, opts);
    if (r.error) return { ok: false, error: r.error };
    const { a, target } = r;
    const sc = steam.shortcutOf(a, target, opts);
    if (sc.error) return { ok: false, error: 'vdf', detail: sc.error };
    if (!sc.shortcut) return { ok: false, error: 'noshortcut' };
    const appid = sc.shortcut.appid;
    const applied = steam.applyArtwork(a, appid, art.selectedFiles(root));
    const want = iconFor(applied);
    let iconPending = false;
    const ourIcon = p => p && path.basename(p).startsWith(String(appid) + '_icon.');
    const iconOk = want ? sc.shortcut.icon === want : !ourIcon(sc.shortcut.icon);
    if (!iconOk) {
        if (await steam.isSteamRunning(opts)) iconPending = true;
        else if (target) steam.upsertShortcut(a, target, want, opts);
        else iconPending = true;
    }
    art.noteSteam(root, a.id, { appid, install: a.install, files: applied.copied.map(c => c.file), verified: applied.verified });
    return { ok: applied.verified, appid, applied, iconPending, inSteam: steam.artworkInSteam(a, appid) };
}

/* adds (or brings up to date) the shortcut, then the artwork; Steam closed */
async function add(root, ref, opts = {}) {
    if (await steam.isSteamRunning(opts)) return { ok: false, error: 'running' };
    const r = await resolve(root, ref, opts);
    if (r.error) return { ok: false, error: r.error };
    if (!r.target) return { ok: false, error: 'noexe' };
    /* the artwork first, chosen by itself for any slot still empty: the game
     * arrives in Steam with it (no key, no network: added without) */
    let auto = null;
    if (art.apiKey(root) && opts.autoArt !== false) {
        try { auto = await art.autoPick(root, false, opts.fetch); } catch (e) { auto = { ok: false, error: String(e.message || e) }; }
    }
    let res;
    try {
        res = steam.upsertShortcut(r.a, r.target, null, opts);
    } catch (e) {
        return { ok: false, error: 'vdf', detail: String(e.message || e) };
    }
    const ap = await apply(root, ref, opts);
    return { ok: true, appid: res.appid, added: res.added, removedDuplicates: res.removedDuplicates, backup: res.backup, auto, artwork: ap };
}

async function remove(root, ref, opts = {}) {
    if (await steam.isSteamRunning(opts)) return { ok: false, error: 'running' };
    const r = await resolve(root, ref, opts);
    if (r.error) return { ok: false, error: r.error };
    try {
        const res = steam.removeShortcut(r.a, r.target, opts);
        return Object.assign({ ok: true }, res);
    } catch (e) {
        return { ok: false, error: 'vdf', detail: String(e.message || e) };
    }
}

/* after a choice changes: copy it to Steam if the shortcut is there */
async function syncIfAdded(root, opts = {}) {
    const st = await status(root, opts);
    if (!st.account) return { synced: false };
    const acc = st.accounts.find(x => x.id === st.account.id && x.install === st.account.install);
    if (!acc || !acc.shortcut) return { synced: false };
    const r = await apply(root, st.account, opts);
    return Object.assign({ synced: true }, r);
}

/* every slot chosen again by itself, then copied to Steam if the game is there */
async function autoAll(root, opts = {}) {
    const r = await art.autoPick(root, true, opts.fetch);
    if (r.picked.length) r.sync = await syncIfAdded(root, opts);
    return r;
}

module.exports = { status, chooseAccount, apply, add, remove, syncIfAdded, autoAll };
