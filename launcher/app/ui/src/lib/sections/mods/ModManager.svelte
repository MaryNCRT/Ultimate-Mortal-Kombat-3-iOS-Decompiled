<script>
    /* MOD MANAGER: the mods in mods\, highest priority first. Each one on or
     * off, moved up or down, its card (name, author, version, category,
     * description) and its files with what each replaces. Every change
     * writes mods\mods.ini at once; the game reads it when it starts. */
    import { onMount } from 'svelte';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { ms, refreshMods, reloadMods, moveMod, toggleMod } from '#lib/mods/store.svelte.js';
    import { toPng } from '#lib/mods/image.js';

    const CATS = ['textures', 'audio', 'music', 'models', 'mixed'];
    let making = $state(false);
    let draft = $state({ name: '', author: '', version: '1.0', category: 'textures', description: '' });
    let err = $state('');
    let trashArmed = $state(null);

    const shown = $derived(ms.list.map((m, i) => ({ m, i })).filter(({ m }) => ms.filter === 'all' || m.category === ms.filter));

    onMount(refreshMods);

    async function create() {
        const r = await api.modsCreate($state.snapshot(draft));
        if (!r || !r.ok) {
            err = r && r.error === 'exists' ? '!' : '?';
            return;
        }
        selectBlip();
        making = false;
        err = '';
        draft = { name: '', author: '', version: '1.0', category: draft.category, description: '' };
        await refreshMods();
        ms.open = r.folder;
    }

    async function info(m, k, v) {
        await api.modsInfo(m.folder, { [k]: v });
        await refreshMods();
    }

    async function add(m) {
        if (await api.modsAdd(m.folder)) await refreshMods();
    }

    /* the mod's picture in the list: any picture the player picks, made a
     * PNG no wider than 640 (a photo straight off a camera was too big to
     * show) */
    let coverErr = $state('');
    async function cover(m) {
        selectBlip();
        coverErr = '';
        const got = await api.pickFile('picture');
        if (!got) return;
        const png = await toPng(got.bytes, 640).catch(() => null);
        if (!png) { coverErr = tx('vwNotPicture'); return; }
        if (await api.modsCover(m.folder, png)) await refreshMods();
        else coverErr = tx('vwNotPicture');
    }

    async function trash(m) {
        if (trashArmed !== m.folder) {
            trashArmed = m.folder;
            return;
        }
        trashArmed = null;
        await api.modsTrash(m.folder);
        await refreshMods();
    }

    const kinds = m => Object.entries(m.kinds).filter(([k]) => k !== 'other').map(([k, n]) => (tx('cats')[k] || k) + ' ' + n).join(' · ');
</script>

<div class="bar">
    <button class="btn hot" onpointerenter={hoverBlip} onclick={() => { making = !making; err = ''; }}>{tx('mgNew')}</button>
    <button class="btn" onpointerenter={hoverBlip} onclick={() => api.modsOpen('')}>{tx('mgOpen')}</button>
    <button class="btn" onpointerenter={hoverBlip} onclick={reloadMods}>{tx('mgRefresh')}</button>
    <span class="grow"></span>
    <label class="lbl" for="mfilter">{tx('mgFilter')}</label>
    <select id="mfilter" class="sel narrow" bind:value={ms.filter}>
        <option value="all">{tx('mgAll')}</option>
        {#each CATS as c}<option value={c}>{tx('cats')[c]}</option>{/each}
    </select>
</div>

{#if making}
    <div class="card new">
        <div class="fields">
            <div class="field"><label class="lbl" for="nname">{tx('mgName')}</label>
                <input id="nname" class="edt" maxlength="60" spellcheck="false" bind:value={draft.name} class:bad={err}></div>
            <div class="field"><label class="lbl" for="ncat">{tx('mgCat')}</label>
                <select id="ncat" class="sel" bind:value={draft.category}>
                    {#each CATS as c}<option value={c}>{tx('cats')[c]}</option>{/each}
                </select></div>
            <div class="field"><label class="lbl" for="nauth">{tx('mgAuthor')}</label>
                <input id="nauth" class="edt" maxlength="60" spellcheck="false" bind:value={draft.author}></div>
            <div class="field"><label class="lbl" for="nver">{tx('mgVersion')}</label>
                <input id="nver" class="edt" maxlength="20" spellcheck="false" bind:value={draft.version}></div>
            <div class="field wide"><label class="lbl" for="ndesc">{tx('mgDesc')}</label>
                <input id="ndesc" class="edt" maxlength="300" bind:value={draft.description}></div>
        </div>
        <div class="row end"><button class="btn hot" disabled={!draft.name.trim()} onclick={create}>{tx('mgCreate')}</button></div>
    </div>
{/if}

{#if !ms.list.length}
    <p class="empty">{tx('mgNone')}</p>
{/if}

<ol class="mods">
    {#each shown as { m, i } (m.folder)}
        <li class="card" class:off={!m.on} class:open={ms.open === m.folder}>
            <div class="head">
                <span class="prio" title={tx('mgPrio')}>{i + 1}</span>
                <span class="pic">{#if m.preview}<img src={m.preview} alt="">{:else}<i class="cat {m.category}"></i>{/if}</span>
                <button class="name" onclick={() => { selectBlip(); ms.open = ms.open === m.folder ? null : m.folder; }}>
                    <b>{m.name}</b>
                    <small>{[m.version && 'v' + m.version, m.author].filter(Boolean).join(' · ')}</small>
                </button>
                <span class="tag {m.category}">{tx('cats')[m.category]}</span>
                <span class="count">{m.count} {tx('mgFiles')}{#if m.unknown}<em class="warn" title={tx('mgUnknown')}> ⚠ {m.unknown}</em>{/if}{#if m.lost}<em class="lost" title={tx('mgLost')}> ⇣ {m.lost}</em>{/if}</span>
                <button class="arrow" title={tx('mgUp')} disabled={i === 0} onclick={() => { selectBlip(); moveMod(i, -1); }}>▲</button>
                <button class="arrow" title={tx('mgDown')} disabled={i === ms.list.length - 1} onclick={() => { selectBlip(); moveMod(i, 1); }}>▼</button>
                <button class="switch" class:on={m.on} onclick={() => { selectBlip(); toggleMod(i); }}>{m.on ? tx('mgOn') : tx('mgOff')}</button>
            </div>
            {#if ms.open === m.folder}
                <div class="body">
                    <div class="cover">
                        {#if m.preview}<img src={m.preview} alt="">{:else}<i class="cat {m.category}"></i>{/if}
                        <button class="btn" onpointerenter={hoverBlip} onclick={() => cover(m)}>{tx('mgCover')}</button>
                        {#if coverErr}<span class="note bad">{coverErr}</span>{/if}
                    </div>
                    <div class="fields">
                        <div class="field"><span class="lbl">{tx('mgName')}</span>
                            <input class="edt" maxlength="60" value={m.name} onchange={e => info(m, 'name', e.currentTarget.value)}></div>
                        <div class="field"><span class="lbl">{tx('mgCat')}</span>
                            <select class="sel" value={m.category} onchange={e => info(m, 'category', e.currentTarget.value)}>
                                {#each CATS as c}<option value={c}>{tx('cats')[c]}</option>{/each}
                            </select></div>
                        <div class="field"><span class="lbl">{tx('mgAuthor')}</span>
                            <input class="edt" maxlength="60" value={m.author} onchange={e => info(m, 'author', e.currentTarget.value)}></div>
                        <div class="field"><span class="lbl">{tx('mgVersion')}</span>
                            <input class="edt" maxlength="20" value={m.version} onchange={e => info(m, 'version', e.currentTarget.value)}></div>
                        <div class="field wide"><span class="lbl">{tx('mgDesc')}</span>
                            <input class="edt" maxlength="300" value={m.description} onchange={e => info(m, 'description', e.currentTarget.value)}></div>
                    </div>
                    {#if kinds(m)}<p class="note">{kinds(m)}</p>{/if}
                    <ul class="files">
                        {#each m.files.slice(0, 200) as f}
                            <li class:miss={!f.replaces}><span>{f.file}</span><span class="to">{f.replaces ? '→ ' + f.replaces : tx('mgNoMatch')}</span></li>
                        {/each}
                    </ul>
                    <div class="row end">
                        <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); ms.layer = m.folder; ms.tab = 'view'; }}>{tx('mgSeeIt')}</button>
                        <button class="btn" onpointerenter={hoverBlip} onclick={() => add(m)}>{tx('mgAdd')}</button>
                        <button class="btn" onpointerenter={hoverBlip} onclick={() => api.modsOpen(m.folder)}>{tx('mgOpen')}</button>
                        <button class="btn danger" onpointerenter={hoverBlip} onclick={() => trash(m)}>{trashArmed === m.folder ? tx('mgTrashAsk') : tx('mgTrash')}</button>
                    </div>
                </div>
            {/if}
        </li>
    {/each}
</ol>

<p class="note">{tx('mgHow')}</p>

<style>
    .bar { display: flex; align-items: center; gap: calc(var(--u) * 1); margin-bottom: calc(var(--u) * 1.2); }
    .grow { flex: 1; }
    .narrow { flex: 0 0 calc(var(--u) * 18); }
    .empty { color: var(--dim); text-align: center; margin: calc(var(--u) * 3) 0; }
    .mods { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: calc(var(--u) * .6); }
    .card {
        background: var(--panel); border: 1px solid var(--line); border-radius: var(--r-sm);
        padding: calc(var(--u) * .6) calc(var(--u) * 1);
    }
    .card.new { margin-bottom: calc(var(--u) * 1.2); padding: calc(var(--u) * 1.2); }
    .card.off { opacity: .55; }
    .card.open { border-color: var(--blue); }
    .head { display: flex; align-items: center; gap: calc(var(--u) * 1); }
    .prio { width: calc(var(--u) * 2.6); text-align: center; color: var(--dim); font-family: var(--font-mono); }
    .pic { width: calc(var(--u) * 4.4); height: calc(var(--u) * 4.4); flex: none; display: grid; place-items: center; }
    .pic img { max-width: 100%; max-height: 100%; border-radius: 4px; }
    .cover { float: right; display: flex; flex-direction: column; align-items: center; gap: calc(var(--u) * .5); margin: 0 0 calc(var(--u) * .6) calc(var(--u) * 1.2); }
    .cover img { width: calc(var(--u) * 16); height: calc(var(--u) * 10); object-fit: cover; border-radius: var(--r-sm); border: 1px solid var(--line); }
    .cover i { width: calc(var(--u) * 16); height: calc(var(--u) * 10); display: block; }
    .cat { width: 70%; height: 70%; border-radius: 50%; background: var(--grad-hot); box-shadow: 0 0 calc(var(--u) * .6) rgba(140, 130, 255, .6); }
    .cat.audio, .tag.audio { background: #1f6f6a; }
    .cat.music, .tag.music { background: #8a3c8f; }
    .cat.models, .tag.models { background: #9a6a1c; }
    .cat.mixed, .tag.mixed { background: #4b4b6b; }
    .name {
        flex: 1; min-width: 0; text-align: left; background: none; border: 0; cursor: pointer; padding: 0;
        display: flex; flex-direction: column;
    }
    .name b { font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .name small { color: var(--dim); font-size: calc(var(--t) * 1.3); }
    .tag {
        font-size: calc(var(--t) * 1.2); letter-spacing: .08em; padding: calc(var(--u) * .25) calc(var(--u) * .8);
        border-radius: calc(var(--u) * 1); background: #4a3fae;
    }
    .count { color: var(--dim); font-size: calc(var(--t) * 1.4); white-space: nowrap; }
    .warn { color: var(--gold1); font-style: normal; }
    .lost { color: #ff9aa0; font-style: normal; }
    .arrow {
        width: calc(var(--u) * 3); height: calc(var(--u) * 3); padding: 0; cursor: pointer;
        background: var(--grad-btn); border: 1px solid var(--line); border-radius: var(--r-sm);
    }
    .arrow:disabled { opacity: .3; cursor: default; }
    .switch {
        min-width: calc(var(--u) * 12); height: calc(var(--u) * 3); cursor: pointer; letter-spacing: .08em;
        font-size: calc(var(--t) * 1.3); border-radius: calc(var(--u) * 1.5);
        background: var(--well); border: 1px solid var(--line); color: var(--dim);
    }
    .switch.on { background: var(--grad-hot); border-color: var(--hot-rim); color: #fff; }
    .body { padding: calc(var(--u) * 1) 0 calc(var(--u) * .4) calc(var(--u) * 3.6); }
    .files {
        list-style: none; margin: calc(var(--u) * 1) 0; padding: calc(var(--u) * .6); max-height: calc(var(--u) * 18);
        overflow-y: auto; background: var(--well); border-radius: var(--r-sm);
        font-family: var(--font-mono); font-size: calc(var(--t) * 1.3);
    }
    .files li { display: flex; justify-content: space-between; gap: calc(var(--u) * 2); padding: calc(var(--u) * .15) 0; }
    .files .to { color: var(--dim); }
    .files .miss .to { color: var(--gold1); }
    .bad { border-color: #ff7f86; }
</style>
