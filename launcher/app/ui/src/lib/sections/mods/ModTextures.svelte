<script>
    /* TEXTURES: every texture of the game by where it belongs -- a fighter,
     * a stage, the interface and menus, the effects (mods/owners.js) -- each
     * replaced by a picture into a mod, in that place's folder of the mod
     * (Stages\The Pit III\PIT1.png), or exported as PNG to the same place in
     * exports\. The game takes a mod's file by name from any of its folders
     * (runtime/mods.c). */
    import { onMount } from 'svelte';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { ms, src, loadCatalog, refreshMods, forgetMod } from '#lib/mods/store.svelte.js';
    import { pngBytes } from '#lib/mods/assets.js';
    import { putTexture } from '#lib/mods/image.js';
    import { textureOwners, exportTexDir } from '#lib/mods/owners.js';

    const GROUP_KEYS = [['characters', 'txChars'], ['stages', 'txStages'], ['interface', 'txUi'], ['effects', 'txFx']];

    let owners = $state(null);
    let note = $state('');
    let busy = $state(false);

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
            const files = await putTexture(api, src, ms.edMod, t.name, got.bytes, ms.autoSmall, home(t));
            if (!files) { note = tx('vwNotPicture'); return; }
            forgetMod();
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
            <input class="edt" list="txmods" spellcheck="false" bind:value={ms.edMod}>
            <datalist id="txmods">{#each ms.list as m}<option value={m.folder}></option>{/each}</datalist></label>
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
            <label class="chk"><input type="checkbox" bind:checked={ms.autoSmall}><span>{tx('vwAutoSmall')}</span></label>
        </div>
        <p class="note">{tx('txNote')}</p>
        {#if note}<p class="note ok">{note}</p>{/if}
        {#if !owners}
            <p class="note">{tx('exBusy')}</p>
        {:else}
            <div class="scroll">
                <table class="grid">
                    <tbody>
                        {#each shown as t (t.id)}
                            <tr class:mod={inMod.has(t.name.toUpperCase())}>
                                <td>{t.name}{#if inMod.has(t.name.toUpperCase())} <small class="ok">· {tx('txInMod')}</small>{/if}</td>
                                <td><button class="btn" onclick={() => see(t)}>{tx('edSee')}</button></td>
                                <td><button class="btn" disabled={busy} onclick={() => replace(t)}>{tx('edReplacePng')}</button></td>
                                <td><button class="btn" onclick={() => exportOne(t)}>{tx('edExportPng')}</button></td>
                            </tr>
                        {/each}
                    </tbody>
                </table>
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
    .grid { width: 100%; border-collapse: collapse; font-size: calc(var(--t) * 1.3); }
    .grid td { padding: calc(var(--u) * .15) calc(var(--u) * .4); border-top: 1px solid var(--line); }
    .grid tr.mod td { background: rgba(120, 220, 160, .08); }
    .ok { color: var(--ok-text); word-break: break-all; }
</style>
