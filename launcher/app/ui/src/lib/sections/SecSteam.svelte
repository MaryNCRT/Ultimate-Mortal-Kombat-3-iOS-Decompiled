<script>
    /* STEAM: the launcher as a non-Steam game, and its artwork.
     *   STEAM        the account, the shortcut (add / update / remove, only
     *                with Steam closed), the five pictures chosen and what
     *                Steam's grid folder has, COPY ARTWORK TO STEAM
     *   STEAMGRIDDB  this game's images on SteamGridDB (game 38752), page by
     *                page: download, choose
     *   DOWNLOADED   what is in data\steam-artwork, offline: choose, delete
     * lib/steamsync.js, lib/steam.js, lib/artwork.js in the main process. */
    import { onMount } from 'svelte';
    import { app, tx, STEAM_TABS } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';

    const SLOTS = [['grid', 'sGrid'], ['wide', 'sWide'], ['hero', 'sHero'], ['logo', 'sLogo'], ['icon', 'sIcon']];
    const KINDS = [['grid', 'kGrid'], ['hero', 'kHero'], ['logo', 'kLogo'], ['icon', 'kIcon']];

    let st = $state(null);            /* steamsync.status */
    let lib = $state({ items: [], selected: {}, steam: {} });
    let msg = $state(null);           /* { text, bad } */
    let busy = $state(false);
    let keyText = $state('');
    let keyEdit = $state(false);

    let kind = $state('grid');
    let shape = $state('all');
    let page = $state(0);
    let res = $state(null);           /* artwork.browse */
    let loading = $state(false);
    let confirmDel = $state(null);    /* key */

    const acc = $derived(st && st.account ? st.accounts.find(a => a.id === st.account.id && a.install === st.account.install) : null);
    const byKey = $derived(Object.fromEntries(lib.items.map(i => [i.key, i])));
    const chosenKeys = $derived(new Set(Object.values(lib.selected).filter(Boolean)));

    function say(text, bad = false) { msg = { text, bad }; }
    const err = r => tx('stErr_' + r.error) + (r.detail ? ` (${r.detail})` : '');

    async function refresh() {
        [st, lib] = await Promise.all([api.steam.status(), api.art.library()]);
    }
    onMount(async () => {
        await refresh();
        if (st && st.hasKey && app.steamTab === 'browse') load();
    });
    $effect(() => {
        if (app.steamTab === 'browse' && st && st.hasKey && !res && !loading) load();
    });

    async function run(fn, what = true) {
        if (busy) return;
        busy = what;
        try { await fn(); } finally { busy = false; await refresh(); }
    }

    function reportApply(r) {
        if (!r.ok) say(err(r), true);
        else say(r.iconPending ? tx('stIconPending') : tx('stApplied'), false);
    }
    const add = () => run(async () => {
        selectBlip();
        const r = await api.steam.add(st.account);
        if (!r.ok) return say(err(r), true);
        let t = tx('stAddedOk');
        if (r.auto && r.auto.picked && r.auto.picked.length) t += ' ' + tx('stAutoDone');
        else if (r.auto && r.auto.error) t += ' ' + err(r.auto);
        if (r.artwork && r.artwork.iconPending) t += ' ' + tx('stIconPending');
        say(t);
    }, 'add');
    const remove = () => run(async () => {
        selectBlip();
        const r = await api.steam.remove(st.account);
        say(r.ok ? tx('stRemovedOk') : err(r), !r.ok);
    }, 'remove');
    const auto = () => run(async () => {
        selectBlip();
        const r = await api.steam.auto();
        if (!r.ok && !r.picked.length) return say(err(r.failed && r.failed[0] ? r.failed[0] : r), true);
        if (r.sync && r.sync.synced) reportApply(r.sync);
        else say(tx('stAutoDone'));
    });
    const apply = () => run(async () => { selectBlip(); reportApply(await api.steam.apply(st.account)); });
    async function chooseAccount(v) {
        const [install, id] = JSON.parse(v);
        await api.steam.account({ install, id });
        await refresh();
    }

    async function saveKey() {
        selectBlip();
        await api.art.setKey(keyText);
        keyText = ''; keyEdit = false; res = null;
        await refresh();
        if (st.hasKey) { app.steamTab = 'browse'; load(); }
    }

    async function load() {
        loading = true;
        try {
            const r = await api.art.browse(kind, page, shape);
            if (!r.ok) { res = { items: [], more: false, page }; say(err(r), true); }
            else { res = r; msg = null; }
        } finally { loading = false; }
    }
    function setKind(k) { selectBlip(); kind = k; page = 0; shape = 'all'; load(); }
    function setShape(s) { selectBlip(); shape = s; page = 0; load(); }
    function turn(d) { selectBlip(); page = Math.max(0, page + d); load(); }

    /* a choice changed: the main process copied it to Steam when the game is there */
    function afterChoice(r) {
        if (!r.ok) return say(err(r), true);
        if (r.sync && r.sync.synced) reportApply(r.sync);
        else msg = null;
    }
    const select = item => run(async () => {
        selectBlip();
        afterChoice(await api.art.select($state.snapshot(item)));
        if (res) res.items = res.items.map(i => i.key === item.key ? Object.assign(i, { downloaded: true }) : i);
    });
    const download = item => run(async () => {
        selectBlip();
        const r = await api.art.download($state.snapshot(item));
        if (!r.ok) say(err(r), true);
        else if (res) res.items = res.items.map(i => i.key === item.key ? Object.assign(i, { downloaded: true }) : i);
    });
    const unselect = slot => run(async () => { selectBlip(); afterChoice(await api.art.unselect(slot)); });
    const del = (key, confirm) => run(async () => {
        selectBlip();
        const r = await api.art.remove(key, confirm);
        if (r.needConfirm) { confirmDel = key; return; }
        confirmDel = null;
        afterChoice(r);
    });

    const slotOfItem = i => i.kind !== 'grid' ? i.kind : (i.height > i.width ? 'grid' : 'wide');
    const isChosen = key => chosenKeys.has(key);
</script>

{#snippet thumb(src, alt, cls = '')}
    <div class="pic {cls}"><img {src} {alt} loading="lazy" referrerpolicy="no-referrer" /></div>
{/snippet}

<Page>
    <SubNav tabs={STEAM_TABS} current={app.steamTab} pick={t => app.steamTab = t} />

    {#if msg}<div class="msg" class:bad={msg.bad}>{msg.text}</div>{/if}

    {#if !st}
        <div class="none">{tx('stLoading')}</div>
    {:else if app.steamTab === 'steam'}
        {#if !st.accounts.length}
            <div class="none">{tx('stNoSteam')}</div>
        {:else}
            <div class="row">
                <span class="lbl">{tx('stAccount')}</span>
                <select class="sel" disabled={busy} value={JSON.stringify([st.account.install, st.account.id])}
                        onchange={e => chooseAccount(e.currentTarget.value)}>
                    {#each st.accounts as a, i}
                        <option value={JSON.stringify([a.install, a.id])}>{a.persona || `${tx('stAccountN')} ${i + 1}`}{a.kind === 'flatpak' || a.kind === 'snap' ? ` (${a.kind})` : ''}</option>
                    {/each}
                </select>
            </div>
            <div class="note" class:warn={st.running}>{st.running ? tx('stRunning') : tx('stClosed')}</div>
            {#if !st.target}<div class="note warn">{tx('stNoExe')}</div>{/if}

            <div class="card">
                <div class="who">
                    <span class="name">{st.name}</span>
                    <span class="meta">
                        {acc && acc.shortcut ? tx('stIn') : tx('stOut')}
                    </span>
                    {#if acc && acc.error}<span class="meta bad">{acc.error}</span>{/if}
                    {#if acc && acc.shortcut && acc.shortcut.stale}<span class="meta bad">{tx('stStale')}</span>{/if}
                    {#if acc && acc.shortcut && acc.shortcut.copies > 1}<span class="meta bad">{tx('stCopies')}</span>{/if}
                </div>
                <div class="tools">
                    <button class="btn hot" disabled={busy || !st.target} onpointerenter={hoverBlip} onclick={add}>
                        {busy === 'add' ? (st.running ? tx('stClosing') : tx('stWorking')) : acc && acc.shortcut ? tx('stUpdate') : tx('stAddSteam')}</button>
                    <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={auto}>{tx('stAuto')}</button>
                    <button class="btn" disabled={busy || !(acc && acc.shortcut)} onpointerenter={hoverBlip} onclick={apply}>{tx('stApply')}</button>
                    <button class="btn danger" disabled={busy || !(acc && acc.shortcut)} onpointerenter={hoverBlip} onclick={remove}>{tx('stRemove')}</button>
                    <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={() => { selectBlip(); refresh(); }}>{tx('stRecheck')}</button>
                </div>
            </div>

            <div class="slots">
                {#each SLOTS as [slot, label]}
                    {@const key = lib.selected[slot]}
                    {@const it = key ? byKey[key] : null}
                    <div class="slot s-{slot}">
                        <span class="lbl">{tx(label)}</span>
                        {#if it}{@render thumb(it.src, tx(label))}{:else}<div class="pic empty">{tx('stNone')}</div>{/if}
                        <span class="meta">{tx('stInSteam')}: {acc && acc.inSteam && acc.inSteam[slot] ? tx('stYes') : '—'}</span>
                    </div>
                {/each}
            </div>
            {#if acc && acc.kind === 'flatpak' && st.target}
                <div class="note">{tx('stFlatpak')}<code>flatpak override --user --filesystem="{st.target.startDir}" com.valvesoftware.Steam</code></div>
            {/if}
        {/if}
        <p class="how">{tx('stHow')}</p>

    {:else if app.steamTab === 'browse'}
        {#if !st.hasKey || keyEdit}
            <div class="keybox">
                <span class="lbl">{tx('stApiKey')}</span>
                <div class="row">
                    <input class="edt" type="password" autocomplete="off" spellcheck="false" bind:value={keyText}
                           onkeydown={e => e.key === 'Enter' && keyText.trim() && saveKey()} />
                    <button class="btn hot" disabled={!keyText.trim()} onpointerenter={hoverBlip} onclick={saveKey}>{tx('stKeySave')}</button>
                    <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); api.art.site('key'); }}>{tx('stKeyGet')}</button>
                </div>
                <span class="meta">{tx('stKeyHow')}</span>
            </div>
        {:else}
            <div class="row">
                {#each KINDS as [k, label]}
                    <button class="btn" class:hot={kind === k} onpointerenter={hoverBlip} onclick={() => setKind(k)}>{tx(label)}</button>
                {/each}
                <span class="grow"></span>
                <span class="meta">{st.keySource === 'project' ? tx('stKeyProject') : tx('stKeyHave')}</span>
                <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); keyEdit = true; }}>{st.keySource === 'project' ? tx('stKeyOwn') : tx('stKeyChange')}</button>
                <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); api.art.site('game'); }}>{tx('stSite')}</button>
            </div>
            {#if kind === 'grid'}
                <div class="row">
                    {#each [['all', 'shAll'], ['portrait', 'shPortrait'], ['wide', 'shWide']] as [s, label]}
                        <button class="btn small" class:hot={shape === s} onpointerenter={hoverBlip} onclick={() => setShape(s)}>{tx(label)}</button>
                    {/each}
                </div>
            {/if}
            {#if loading && !res}<div class="none">{tx('stLoading')}</div>{/if}
            {#if res}
                <div class="gallery k-{kind}" class:dim={loading}>
                    {#each res.items as it (it.key)}
                        <div class="art" class:chosen={isChosen(it.key)}>
                            {@render thumb(it.thumb, `${it.kind} ${it.id}`)}
                            <span class="meta">{it.width}×{it.height} · {it.mime.replace('image/', '')}{it.style ? ' · ' + it.style : ''}{it.author ? ' · ' + it.author : ''}</span>
                            <div class="tools">
                                <button class="btn hot small" disabled={busy || isChosen(it.key)} onpointerenter={hoverBlip} onclick={() => select(it)}>
                                    {isChosen(it.key) ? tx('stChosen') + ' · ' + tx(SLOTS.find(s => s[0] === slotOfItem(it))[1]) : tx('stSelect')}</button>
                                <button class="btn small" disabled={busy || it.downloaded} onpointerenter={hoverBlip} onclick={() => download(it)}>
                                    {it.downloaded ? tx('stDownloaded') : tx('stDownload')}</button>
                            </div>
                        </div>
                    {:else}
                        <div class="none">—</div>
                    {/each}
                </div>
                <div class="row pager">
                    <button class="btn" disabled={loading || page === 0} onpointerenter={hoverBlip} onclick={() => turn(-1)}>{tx('stPrev')}</button>
                    <span class="meta">{tx('stPage')} {page + 1}{res.total ? ` / ${Math.max(1, Math.ceil(res.total / res.limit))}` : ''}</span>
                    <button class="btn" disabled={loading || !res.more} onpointerenter={hoverBlip} onclick={() => turn(1)}>{tx('stNext')}</button>
                </div>
            {/if}
        {/if}

    {:else}
        <div class="row">
            <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); api.art.open(); }}>{tx('stOpen')}</button>
        </div>
        {#if !lib.items.length}<div class="none">{tx('stNoneHere')}</div>{/if}
        {#each KINDS as [k, label]}
            {@const mine = lib.items.filter(i => i.kind === k)}
            {#if mine.length}
                <h3>{tx(label)}</h3>
                <div class="gallery k-{k}">
                    {#each mine as it (it.key)}
                        <div class="art" class:chosen={isChosen(it.key)}>
                            {@render thumb(it.src, it.file)}
                            <span class="meta">{it.width}×{it.height} · {it.file.split('/').pop()}</span>
                            {#if confirmDel === it.key}
                                <span class="meta bad">{tx('stDelConfirm')}</span>
                                <div class="tools">
                                    <button class="btn danger small" disabled={busy} onclick={() => del(it.key, true)}>{tx('stYesDelete')}</button>
                                    <button class="btn small" onclick={() => { confirmDel = null; }}>{tx('stCancel')}</button>
                                </div>
                            {:else}
                                <div class="tools">
                                    {#if isChosen(it.key)}
                                        <button class="btn small" disabled={busy} onpointerenter={hoverBlip} onclick={() => unselect(it.slot)}>{tx('stUnselect')}</button>
                                    {:else}
                                        <button class="btn hot small" disabled={busy} onpointerenter={hoverBlip} onclick={() => select(it)}>{tx('stSelect')}</button>
                                    {/if}
                                    <button class="btn danger small" disabled={busy} onpointerenter={hoverBlip} onclick={() => del(it.key, false)}>{tx('stDelete')}</button>
                                </div>
                            {/if}
                        </div>
                    {/each}
                </div>
            {/if}
        {/each}
    {/if}
</Page>

<style>
    .row { display: flex; align-items: center; flex-wrap: wrap; gap: calc(var(--u) * .7); margin-bottom: calc(var(--u) * .9); }
    .grow { flex: 1; }
    .lbl { color: #e6bb63; font-family: var(--font-head); font-weight: 900; letter-spacing: .06em; font-size: calc(var(--u) * 1.3); }
    .meta { color: var(--dim); font-size: calc(var(--u) * 1.15); letter-spacing: .03em; overflow-wrap: anywhere; }
    .meta.bad, .msg.bad { color: var(--bad-text, #ff8c7a); }
    .sel { flex: 1; min-width: 0; }
    .edt { flex: 1; min-width: calc(var(--u) * 20); }
    .btn.small { height: auto; font-size: calc(var(--t) * 1.15); padding: calc(var(--u) * .35) calc(var(--u) * .8); }
    .msg, .note, .none, .how {
        font-size: calc(var(--u) * 1.25); padding: calc(var(--u) * .7) calc(var(--u) * 1.2); margin-bottom: calc(var(--u) * .8);
        background: var(--sunk); border-left: calc(var(--u) * .35) solid #2e3846; color: var(--dim);
    }
    .msg { border-left-color: #e6bb63; color: #fff; }
    .note.warn { border-left-color: var(--blood); color: #fff; }
    .how { border-left-color: #e6bb63; margin-top: calc(var(--u) * 1.4); }
    code { display: block; margin-top: calc(var(--u) * .4); color: #fff; user-select: all; overflow-wrap: anywhere; }
    .card {
        display: flex; align-items: center; flex-wrap: wrap; gap: calc(var(--u) * .8);
        margin-bottom: calc(var(--u) * 1); padding: calc(var(--u) * .8) calc(var(--u) * 1);
        background: var(--plate); border: 1px solid #2e3846; border-left: calc(var(--u) * .35) solid var(--blood);
        box-shadow: var(--bevel);
    }
    .who { flex: 1 1 calc(var(--u) * 30); min-width: 0; display: flex; flex-direction: column; }
    .name { color: #fff; font-family: var(--font-head); font-weight: 900; letter-spacing: .06em; font-size: calc(var(--u) * 1.6); text-shadow: 0 1px 0 #000; }
    .tools { display: flex; flex-wrap: wrap; gap: calc(var(--u) * .5); }
    .slots { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: calc(var(--u) * .8); }
    .slot { display: flex; flex-direction: column; gap: calc(var(--u) * .4); min-width: 0; }
    .pic {
        display: flex; align-items: center; justify-content: center; overflow: hidden;
        height: calc(var(--u) * 13); background: #07090c repeating-conic-gradient(#0c1016 0 25%, #07090c 0 50%) 0 0 / calc(var(--u) * 1.6) calc(var(--u) * 1.6);
        border: 1px solid #2e3846;
    }
    .pic img { max-width: 100%; max-height: 100%; object-fit: contain; display: block; }
    .pic.empty { color: var(--dim); font-size: calc(var(--u) * 1.1); }
    .gallery { display: grid; gap: calc(var(--u) * .9); grid-template-columns: repeat(auto-fill, minmax(calc(var(--u) * 15), 1fr)); margin-bottom: calc(var(--u) * 1); }
    .gallery.k-hero { grid-template-columns: repeat(auto-fill, minmax(calc(var(--u) * 30), 1fr)); }
    .gallery.dim { opacity: .5; }
    .art {
        display: flex; flex-direction: column; gap: calc(var(--u) * .4); min-width: 0;
        padding: calc(var(--u) * .5); background: var(--plate); border: 1px solid #2e3846;
    }
    .art.chosen { border-color: #e6bb63; box-shadow: 0 0 calc(var(--u) * .8) rgba(230, 187, 99, .4); }
    .k-hero .pic { height: calc(var(--u) * 10); }
    .k-icon .pic, .k-logo .pic { height: calc(var(--u) * 9); }
    .pager { justify-content: center; }
    h3 { margin: calc(var(--u) * 1) 0 calc(var(--u) * .6); }
</style>
