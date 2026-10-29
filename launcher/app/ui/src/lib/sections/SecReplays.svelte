<script>
    /* REPLAYS: the last five fights the game kept and the ones kept for good
     * (lib/replays.js). WATCH opens the game on that fight, with its player;
     * a recent one can be kept for good; any can be exported for someone
     * else, and theirs imported. */
    import { onMount } from 'svelte';
    import { app, tx, save, setStatus } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';

    let lists = $state({ recent: [], saved: [] });

    async function refresh() {
        lists = await api.replaysList();
    }
    onMount(refresh);

    const clock = s => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;
    const pretty = n => (n || '').replace(/-/g, ' ').toUpperCase();
    const title = r => r.n1 ? `${pretty(r.n1)}  VS  ${pretty(r.n2)}` : r.name.replace(/\.umkr$/i, '').toUpperCase();

    async function watch(r) {
        selectBlip();
        const res = await api.replaysPlay(r.folder, r.name);
        if (!res.ok)
            setStatus('need', tx('rpPlayFail'));
        else
            setStatus('playing', tx('statusPlaying'));
    }
    async function keep(r) {
        selectBlip();
        await api.replaysKeep(r.name);
        await refresh();
    }
    async function del(r) {
        selectBlip();
        await api.replaysDelete(r.folder, r.name);
        await refresh();
    }
    async function exp(r) {
        selectBlip();
        await api.replaysExport(r.folder, r.name);
    }
    async function imp() {
        selectBlip();
        if (await api.replaysImport())
            await refresh();
    }
</script>

{#snippet row(r)}
    <div class="rp">
        <div class="who">
            <span class="name">{title(r)}</span>
            <span class="meta">{r.when || new Date(r.mtime).toLocaleString(app.ui === 'ES' ? 'es' : 'en')} — {clock(r.seconds)}</span>
        </div>
        <button class="btn hot" disabled={!app.ready} onpointerenter={hoverBlip} onclick={() => watch(r)}>{tx('rpWatch')}</button>
        {#if r.folder === 'recent'}
            <button class="btn" onpointerenter={hoverBlip} onclick={() => keep(r)}>{tx('rpKeep')}</button>
        {/if}
        <button class="btn" onpointerenter={hoverBlip} onclick={() => exp(r)}>{tx('rpExport')}</button>
        <button class="btn danger" onpointerenter={hoverBlip} onclick={() => del(r)}>{tx('rpDelete')}</button>
    </div>
{/snippet}

<Page>
    <label class="chk"><input type="checkbox" checked={app.cfg.replays !== false}
        onchange={e => save({ replays: e.currentTarget.checked })}><span>{tx('rpKeepOn')}</span></label>

    <h3>{tx('rpRecent')}</h3>
    {#each lists.recent as r (r.name)}{@render row(r)}{:else}<div class="none">{tx('rpNoneRecent')}</div>{/each}

    <h3>{tx('rpSaved')}</h3>
    <div class="tools">
        <button class="btn" onpointerenter={hoverBlip} onclick={imp}>{tx('rpImport')}</button>
        <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); api.replaysOpen(); }}>{tx('rpOpen')}</button>
    </div>
    {#each lists.saved as r (r.name)}{@render row(r)}{:else}<div class="none">{tx('rpNone')}</div>{/each}

    <p class="how">{tx('rpHow')}</p>
</Page>

<style>
    .rp {
        display: flex; align-items: center; gap: calc(var(--u) * .8);
        padding: calc(var(--u) * .6) 0; border-bottom: 1px solid var(--line);
    }
    .who { flex: 1; min-width: 0; display: flex; flex-direction: column; }
    .name { color: #fff; font-size: calc(var(--t) * 1.6); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .meta { color: var(--dim); font-size: calc(var(--t) * 1.3); }
    .tools { display: flex; gap: calc(var(--u) * .8); margin-bottom: calc(var(--u) * .6); }
    .none { color: var(--dim); font-size: calc(var(--t) * 1.4); padding: calc(var(--u) * .4) 0; }
    .how { color: var(--dim); font-size: calc(var(--t) * 1.3); margin-top: calc(var(--u) * 1.4); }
</style>
