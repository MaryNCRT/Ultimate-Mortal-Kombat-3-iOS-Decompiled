<script>
    import ModTarget from './ModTarget.svelte';
    /* TEXTURES: every texture of the game by where it belongs -- a fighter,
     * a stage, the interface and menus, the effects (mods/owners.js) -- each
     * replaced by a picture into a mod, in that place's folder of the mod
     * (Stages\The Pit III\PIT1.png), or exported as PNG to the same place in
     * exports\ -- one, all of a fighter's or stage's, or every fighter's
     * or stage's (EXPORT ALL). The game takes a mod's file by name from any of its folders
     * (runtime/mods.c).
     *
     * Each texture is shown as its picture, in groups (a fighter's outfits,
     * portraits, babality ...; the menus' by their names: mods/derived.js),
     * and the one picked large on the right with its size. */
    import { onMount } from 'svelte';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { ms, src, loadCatalog, refreshMods, forgetMod } from '#lib/mods/store.svelte.js';
    import { pngBytes } from '#lib/mods/assets.js';
    import { putTexture } from '#lib/mods/image.js';
    import { textureOwners, exportTexDir } from '#lib/mods/owners.js';
    import { textureGroups } from '#lib/mods/derived.js';
    import { thumb, forgetThumbs, lazy } from '#lib/mods/thumbs.js';

    const GROUP_KEYS = [['characters', 'txChars'], ['stages', 'txStages'], ['interface', 'txUi'], ['effects', 'txFx']];

    let owners = $state(null);
    let note = $state('');
    let busy = $state(false);
    let pics = $state({});                  /* texture file -> { url, w, h } */
    let sel = $state(null);                 /* the texture shown large */
    let big = $state(null);                 /* its large picture */

    onMount(async () => {
        await loadCatalog();
        await refreshMods();
        if (!ms.edMod) ms.edMod = (ms.layer && ms.layer !== '*' ? ms.layer : '') || (ms.list[0] && ms.list[0].folder) || '';
        owners = await textureOwners(src, ms.cat);
    });

    /* the fighters or stages that own textures, in the catalogue's order */
    const items = $derived.by(() => {
        if (!owners || !ms.cat) return [];
        const names = new Set([...owners.values()].filter(o => o.group === ms.txGroup).map(o => o.name));
        const from = ms.txGroup === 'characters' ? ms.cat.characters : ms.txGroup === 'stages' ? ms.cat.stages : [];
        return from.filter(x => names.has(x.name)).map(x => x.name);
    });
    $effect(() => { if (items.length && !items.includes(ms.txItem)) ms.txItem = items[0]; });

    /* the textures shown: the group's, or the chosen fighter's or stage's;
     * the smaller copies only when AUTO does not write them */
    const shown = $derived.by(() => {
        if (!owners || !ms.cat) return [];
        const one = ms.txGroup === 'characters' || ms.txGroup === 'stages';
        return ms.cat.textures.filter(t => {
            const o = owners.get(t.name.toUpperCase());
            if (!o || o.group !== ms.txGroup || (one && o.name !== ms.txItem)) return false;
            return !ms.autoSmall || !/_LOW\d*$|_LITE\d*$/i.test(t.name);
        });
    });

    const groups = $derived(textureGroups(shown, ms.txGroup));
    const gName = g => { const t = tx('dv'); return ((t && t[g]) || g).toUpperCase(); };

    /* a texture's picture: small in the grid, large when picked; from the
     * chosen mod when it has one */
    const layerOf = () => (ms.edMod && ms.list.some(m => m.folder === ms.edMod) ? ms.edMod : '');
    function pic(t) {
        const k = layerOf() + '|' + t.id;
        if (pics[k] !== undefined) return;
        pics[k] = null;
        thumb(reader(), t.id, layerOf()).then(r => { pics[k] = r; });
    }
    const reader = () => { const l = layerOf(); return { read: rel => api.resRead(rel, l) }; };
    async function pick(t) {
        selectBlip();
        sel = t;
        big = null;
        const r = await thumb(reader(), t.id, layerOf(), 1024);
        if (sel === t) big = r;
    }

    /* the files the chosen mod already has, by name */
    const inMod = $derived.by(() => {
        const m = ms.list.find(x => x.folder === ms.edMod);
        return new Set(m ? m.files.map(f => f.file.replace(/^.*\//, '').replace(/\.\w+$/, '').toUpperCase()) : []);
    });

    const home = t => (owners.get(t.name.toUpperCase()) || { home: 'Interface' }).home;

    async function ensureMod() {
        const name = (ms.edMod || '').trim();
        if (!name) { note = tx('edNoMod'); return false; }
        if (!ms.list.some(m => m.folder === name)) {
            const r = await api.modsCreate({ name, category: 'textures' });
            if (!r || !r.ok) { note = tx('edNoMod'); return false; }
            await refreshMods();
        }
        return true;
    }

    async function replace(t) {
        selectBlip();
        if (busy || !(await ensureMod())) return;
        const got = await api.pickFile('picture');
        if (!got) return;
        busy = true;
        try {
            const files = await putTexture(api, src, ms.edMod, t.name, got.bytes, ms.autoSmall, home(t), ms.autoIce);
            if (!files) { note = tx('vwNotPicture'); return; }
            forgetMod();
            forgetThumbs();
            pics = {};
            if (sel === t) pick(t);
            note = tx('vwSaved') + ms.edMod + '\\' + files.join(', ').replace(/\//g, '\\');
            await refreshMods();
        } finally {
            busy = false;
        }
    }

    async function exportOne(t) {
        selectBlip();
        const png = await pngBytes(src, t.id);
        if (png) note = tx('exDone') + ' ' + await api.exportSave(exportTexDir(home(t)), { [t.name + '.png']: png });
    }

    /* EXPORT ALL: every texture of the fighter or stage shown (or of the
     * whole group: every fighter, every stage), smaller copies included,
     * each to its place in exports\, a few at a time */
    async function exportMany(whole) {
        selectBlip();
        if (busy || !owners || !ms.cat) return;
        const list = ms.cat.textures.filter(t => {
            const o = owners.get(t.name.toUpperCase());
            return o && o.group === ms.txGroup && (whole || !items.length || o.name === ms.txItem);
        });
        busy = true;
        let done = 0, where = '';
        try {
            for (let i = 0; i < list.length; i += 16) {
                const by = {};
                for (const t of list.slice(i, i + 16)) {
                    const png = await pngBytes(src, t.id);
                    if (!png) continue;
                    const d = exportTexDir(home(t));
                    (by[d] = by[d] || {})[t.name + '.png'] = png;
                    done++;
                }
                for (const d in by) where = await api.exportSave(d, by[d]);
                note = tx('exBusy') + ' ' + Math.min(i + 16, list.length) + ' / ' + list.length;
            }
            note = tx('exDone') + ' ' + (whole || !items.length ? where.replace(/[\\/][^\\/]+[\\/]Textures$/, '') : where) + '  (' + done + ' ' + tx('txCount') + ')';
        } finally {
            busy = false;
        }
    }

    function see(t) {
        selectBlip();
        ms.kind = 'textures';
        ms.pick = t;
        if (ms.edMod && ms.list.some(m => m.folder === ms.edMod)) ms.layer = ms.edMod;
        ms.tab = 'view';
    }
</script>

<div class="ed">
    <aside class="side">
        <label class="field"><span class="lbl">{tx('edMod')}</span>
            <ModTarget bind:value={ms.edMod} /></label>
        <span class="tabs sub groups">
            {#each GROUP_KEYS as [g, key]}
                <button class="tab" class:active={ms.txGroup === g} onpointerenter={hoverBlip}
                        onclick={() => { selectBlip(); ms.txGroup = g; }}>{tx(key)}</button>
            {/each}
        </span>
        {#if items.length}
            <ul class="list">
                {#each items as name}
                    <li><button class:on={ms.txItem === name} onpointerenter={hoverBlip}
                        onclick={() => { selectBlip(); ms.txItem = name; }}>{name}</button></li>
                {/each}
            </ul>
        {/if}
    </aside>

    <section class="main">
        <div class="top">
            <h3>{items.length ? ms.txItem : tx(GROUP_KEYS.find(g => g[0] === ms.txGroup)[1])}</h3>
            <span class="dim">{shown.length} {tx('txCount')}</span>
            <span class="grow"></span>
            <button class="btn" disabled={busy || !owners} onpointerenter={hoverBlip} onclick={() => exportMany(false)}>{tx(items.length ? 'txExportOne' : 'txExportAll')}</button>
            {#if items.length}<button class="btn" disabled={busy || !owners} onpointerenter={hoverBlip} onclick={() => exportMany(true)}>{tx(ms.txGroup === 'stages' ? 'txExportStages' : 'txExportChars')}</button>{/if}
            <label class="chk"><input type="checkbox" bind:checked={ms.autoSmall}><span>{tx('vwAutoSmall')}</span></label>
            <label class="chk"><input type="checkbox" bind:checked={ms.autoIce}><span>{tx('vwAutoIce')}</span></label>
        </div>
        <p class="note">{tx('txNote')}</p>
        {#if note}<p class="note ok">{note}</p>{/if}
        {#if !owners}
            <p class="note">{tx('exBusy')}</p>
        {:else}
            <div class="cols">
                <div class="scroll">
                    {#each groups as [g, list] (g)}
                        <h4>{gName(g)} <small class="dim">{list.length}</small></h4>
                        <div class="cards">
                            {#each list as t (t.id)}
                                {@const p = pics[layerOf() + '|' + t.id]}
                                <button class="card" class:on={sel === t} class:mod={inMod.has(t.name.toUpperCase())}
                                        use:lazy={() => pic(t)} onpointerenter={hoverBlip} onclick={() => pick(t)} title={t.name}>
                                    <span class="pic">{#if p}<img src={p.url} alt="">{/if}</span>
                                    <span class="nm">{t.name}</span>
                                    <span class="sz">{p ? p.w + '×' + p.h : ''}{#if inMod.has(t.name.toUpperCase())} · <b class="ok">{tx('txInMod')}</b>{/if}</span>
                                </button>
                            {/each}
                        </div>
                    {/each}
                </div>
                <aside class="look">
                    {#if sel}
                        <div class="big">{#if big}<img src={big.url} alt="">{/if}</div>
                        <span class="lbl">{sel.name}</span>
                        <span class="dim">{big ? big.w + ' × ' + big.h : ''}</span>
                        <button class="btn hot" disabled={busy} onclick={() => replace(sel)}>{tx('edReplacePng')}</button>
                        <button class="btn" onclick={() => exportOne(sel)}>{tx('edExportPng')}</button>
                        <button class="btn" onclick={() => see(sel)}>{tx('edSee')}</button>
                    {:else}
                        <p class="note">{tx('txPick')}</p>
                    {/if}
                </aside>
            </div>
        {/if}
    </section>
</div>

<style>
    .ed { display: grid; grid-template-columns: calc(var(--u) * 24) minmax(0, 1fr); gap: calc(var(--u) * 1.4); flex: 1; min-height: calc(var(--u) * 40); }
    .side { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; }
    .field { display: flex; flex-direction: column; gap: calc(var(--u) * .3); }
    .groups { display: flex; flex-wrap: wrap; }
    .list { list-style: none; margin: 0; padding: 0; overflow-y: auto; flex: 1; min-height: 0; background: var(--well); border-radius: var(--r-sm); }
    .list button {
        width: 100%; text-align: left; background: none; border: 0; cursor: pointer;
        padding: calc(var(--u) * .4) calc(var(--u) * .9); font-size: calc(var(--t) * 1.4); color: var(--lav);
    }
    .list button:hover { color: #fff; background: rgba(150, 140, 255, .12); }
    .list button.on { color: #fff; background: rgba(140, 125, 255, .3); }
    .main { display: flex; flex-direction: column; min-height: 0; gap: calc(var(--u) * .5); }
    .top { display: flex; align-items: center; gap: calc(var(--u) * 1); flex-wrap: wrap; }
    .top h3 { margin: 0; }
    .grow { flex: 1; }
    .dim { color: var(--dim); }
    .scroll { overflow-y: auto; flex: 1; min-height: 0; padding-right: calc(var(--u) * .4); }
    .cols { flex: 1; min-height: 0; display: grid; grid-template-columns: minmax(0, 1fr) calc(var(--u) * 26); gap: calc(var(--u) * 1.2); }
    h4 { margin: calc(var(--u) * .8) 0 calc(var(--u) * .4); letter-spacing: .08em; color: #fff; font-size: calc(var(--t) * 1.3); }
    .cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(calc(var(--u) * 11), 1fr)); gap: calc(var(--u) * .6); }
    .card { display: flex; flex-direction: column; gap: calc(var(--u) * .25); background: var(--well); border: 1px solid transparent; border-radius: var(--r-sm);
        padding: calc(var(--u) * .4); cursor: pointer; color: var(--lav); text-align: left; min-width: 0; }
    .card:hover { border-color: rgba(150, 140, 255, .5); color: #fff; }
    .card.on { border-color: var(--blue); background: rgba(140, 125, 255, .25); color: #fff; }
    .card.mod { box-shadow: inset 0 0 0 1px rgba(120, 220, 160, .45); }
    .card .pic { aspect-ratio: 1; display: grid; place-items: center; border-radius: 3px;
        background: repeating-conic-gradient(#1b1840 0 25%, #14112f 0 50%) 0 0 / 12px 12px; overflow: hidden; }
    .card .pic img { max-width: 100%; max-height: 100%; image-rendering: auto; }
    .card .nm { font-size: calc(var(--t) * 1.1); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .card .sz { font-size: calc(var(--t) * 1); color: var(--dim); }
    .look { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; overflow-y: auto; }
    .look .btn { width: 100%; }
    .big { aspect-ratio: 1; display: grid; place-items: center; border-radius: var(--r-sm); border: 1px solid var(--line);
        background: repeating-conic-gradient(#1b1840 0 25%, #14112f 0 50%) 0 0 / 16px 16px; overflow: hidden; }
    .big img { max-width: 100%; max-height: 100%; }
    .ok { color: var(--ok-text); word-break: break-all; }
</style>
