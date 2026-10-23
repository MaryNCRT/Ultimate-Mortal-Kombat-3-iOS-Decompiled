<script>
    /* EXPORT: fighters, stages, other models or textures, one or many, into
     * the game folder's exports\, each in its own folder (mods/owners.js):
     * Characters\<fighter>\<FORMAT>, Stages\<stage>\<FORMAT>,
     * Models\<model>\<FORMAT>; a texture in its fighter's or stage's
     * Textures, or in Interface or Effects. A stage can carry its animation
     * (the scene's frames, issue #101). Models as FBX (skeleton, skin,
     * one action per animation), USD -- USDA text, USDC binary, or USDZ, the
     * USDC with its pictures in one file -- (the same, the animations end to
     * end) or OBJ (the mesh alone); their textures as PNG beside them. */
    import { onMount } from 'svelte';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip, doneChord, failChord } from '#lib/sound.js';
    import { ms, src, loadCatalog, fighter, VIEW_KINDS } from '#lib/mods/store.svelte.js';
    import { fighterDesc, stageDesc, meshsetDesc, exportFiles, pngBytes, textureFile } from '#lib/mods/assets.js';
    import { homeOf, textureOwners, textureHome, exportTexDir } from '#lib/mods/owners.js';

    let kind = $state(ms.kind);
    let chosen = $state(new Set());
    let format = $state('fbx');
    let rest = $state('tpose');
    let anims = $state('all');
    let outfits = $state(true);
    let stageAnim = $state(true);
    let busy = $state(false);
    let line = $state('');
    let done = $state('');

    const items = $derived(ms.cat ? ms.cat[kind] || [] : []);
    /* USD three ways: text, binary, and the binary packed with its pictures */
    const FORMATS = [['fbx', 'FBX'], ['usd', 'USDA'], ['usdc', 'USDC'], ['usdz', 'USDZ'], ['obj', 'OBJ']];

    onMount(async () => {
        await loadCatalog();
        /* what the viewer has open comes chosen */
        if (ms.pick && ms.kind === kind) chosen = new Set([ms.pick.id]);
    });

    function flip(id) {
        const s = new Set(chosen);
        s.has(id) ? s.delete(id) : s.add(id);
        chosen = s;
    }

    async function run() {
        if (!chosen.size || busy) return;
        selectBlip();
        busy = true;
        done = '';
        let last = '', n = 0;
        try {
            for (const it of items.filter(x => chosen.has(x.id))) {
                line = tx('exBusy') + ' ' + it.name + ' (' + ++n + '/' + chosen.size + ')';
                await new Promise(r => setTimeout(r, 0));
                let files, sub;
                if (kind === 'textures') {
                    const png = await pngBytes(src, it.id);
                    files = png ? { [it.name + '.png']: png } : {};
                    sub = exportTexDir(textureHome(await textureOwners(src, ms.cat), it.name));
                } else {
                    let desc;
                    if (kind === 'characters') {
                        const f = await fighter(it);
                        const clips = format === 'obj' || anims === 'none' ? []
                            : anims === 'all' ? 'all'
                            : ms.pick && ms.pick.id === it.id && ms.clip >= 0 ? [ms.clip] : [];
                        desc = fighterDesc(f, { rest, clips, variant: ms.pick && ms.pick.id === it.id ? ms.outfit : '' });
                        files = await exportFiles(src, desc, format);
                        /* the fighter's other outfits, to paint from */
                        if (outfits)
                            for (const v of f.variants) {
                                const file = textureFile(src.files, v);
                                if (file && !files[v + '.png']) {
                                    const png = await pngBytes(src, file);
                                    if (png) files[v + '.png'] = png;
                                }
                            }
                    } else {
                        desc = kind === 'stages' ? await stageDesc(src, it, stageAnim && format !== 'obj')
                            : await meshsetDesc(src, it.id);
                        files = await exportFiles(src, desc, format);
                    }
                    sub = homeOf(kind, it) + '/' + format.toUpperCase();
                }
                last = await api.exportSave(sub, files);
            }
            doneChord();
            /* one: its folder; many: the exports folder */
            done = tx('exDone') + ' ' + (chosen.size === 1 ? last : last.replace(/([\\/]exports)[\\/].*$/, '$1'));
        } catch (e) {
            failChord();
            done = String(e && e.message || e);
        } finally {
            busy = false;
            line = '';
        }
    }
</script>

<div class="exp">
    <div class="pick">
        <div class="kinds">
            {#each VIEW_KINDS as [k, key]}
                <button class="tab" class:active={kind === k} onpointerenter={hoverBlip}
                        onclick={() => { selectBlip(); kind = k; chosen = new Set(); }}>{tx(key)}</button>
            {/each}
        </div>
        <div class="row">
            <span class="lbl">{tx('exSel')}: {chosen.size}</span>
            <span class="grow"></span>
            <button class="btn" onclick={() => chosen = new Set(items.map(x => x.id))}>{tx('exSelAll')}</button>
            <button class="btn" onclick={() => chosen = new Set()}>{tx('exSelNone')}</button>
        </div>
        <ul class="list">
            {#each items as it (it.id)}
                <li><label class="chk"><input type="checkbox" checked={chosen.has(it.id)} onchange={() => flip(it.id)}><span>{it.name}</span></label></li>
            {/each}
        </ul>
    </div>

    <div class="opts">
        {#if kind !== 'textures'}
            <h3 class="first">{tx('exFormat')}</h3>
            <span class="tabs sub">
                {#each FORMATS as [f, name]}
                    <button class="tab" class:active={format === f} onclick={() => { selectBlip(); format = f; }}>{name}</button>
                {/each}
            </span>
            {#if kind === 'characters'}
                <h3>{tx('exRest')}</h3>
                <span class="tabs sub">
                    <button class="tab" class:active={rest === 'tpose'} onclick={() => rest = 'tpose'}>{tx('vwTpose')}</button>
                    <button class="tab" class:active={rest === 'bind'} onclick={() => rest = 'bind'}>{tx('vwBind')}</button>
                </span>
                <p class="note">{tx('exTposeNote')}</p>
                {#if format !== 'obj'}
                    <h3>{tx('exAnims')}</h3>
                    <select class="sel" bind:value={anims}>
                        <option value="all">{tx('exAll')}</option>
                        <option value="one">{tx('exOne')}</option>
                        <option value="none">{tx('exNone')}</option>
                    </select>
                {/if}
                <label class="chk more"><input type="checkbox" bind:checked={outfits}><span>{tx('exOutfits')}</span></label>
            {/if}
            {#if kind === 'stages' && format !== 'obj'}
                <h3>{tx('exAnims')}</h3>
                <label class="chk"><input type="checkbox" bind:checked={stageAnim}><span>{tx('exStageAnim')}</span></label>
                <p class="note">{tx('exStageAnimNote')}</p>
            {/if}
            <p class="note">{tx('exTexNote')}</p>
            {#if format.startsWith('usd')}<p class="note">{tx('exUsdNote')}</p>{/if}
        {:else}
            <h3 class="first">{tx('exFormat')}</h3>
            <p>{tx('exTexOnly')}</p>
        {/if}
        <div class="go">
            <button class="btn hot big" disabled={!chosen.size || busy} onpointerenter={hoverBlip} onclick={run}>{busy ? tx('exBusy') : tx('exGo')}</button>
            <button class="btn" onclick={() => api.exportOpen('')}>{tx('exOpen')}</button>
        </div>
        {#if line}<p class="note">{line}</p>{/if}
        {#if done}<p class="note ok">{done}</p>{/if}
    </div>
</div>

<style>
    .exp { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: calc(var(--u) * 2.4); flex: 1; min-height: calc(var(--u) * 40); }
    .pick { display: flex; flex-direction: column; min-height: 0; }
    .kinds { display: grid; grid-template-columns: repeat(4, 1fr); gap: calc(var(--u) * .4); }
    .kinds .tab { font-size: calc(var(--t) * 1.25); padding: calc(var(--u) * .5) calc(var(--u) * .4); }
    .grow { flex: 1; }
    .list { list-style: none; margin: 0; padding: calc(var(--u) * .4) calc(var(--u) * .8); overflow-y: auto; flex: 1; min-height: 0; background: var(--well); border-radius: var(--r-sm); }
    .list .chk { font-size: calc(var(--t) * 1.45); padding: calc(var(--u) * .2) 0; }
    .list .chk input { width: calc(var(--u) * 1.6); height: calc(var(--u) * 1.6); }
    .opts { display: flex; flex-direction: column; }
    .more { margin-top: calc(var(--u) * 1.2); }
    .go { display: flex; gap: calc(var(--u) * 1); align-items: center; margin-top: calc(var(--u) * 2); }
    .ok { color: var(--ok-text); word-break: break-all; }
</style>
