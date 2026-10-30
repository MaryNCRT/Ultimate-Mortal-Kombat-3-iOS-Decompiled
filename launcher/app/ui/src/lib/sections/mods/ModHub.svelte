<script>
    /* MOD HUB: the mods published in the hub repository (lib/hub.js), each
     * with its picture, what it is, the version in the hub and the one in
     * mods\. DOWNLOAD puts it in mods\ (on, at the top of the MOD MANAGER),
     * UPDATE replaces it with the newer one. */
    import { onMount } from 'svelte';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { refreshMods } from '#lib/mods/store.svelte.js';

    let list = $state([]);
    let loading = $state(false);
    let fail = $state('');
    let busy = $state(null);            /* the id being downloaded */
    let prog = $state({ got: 0, total: 0 });
    let msg = $state({});               /* id -> [ok, text] */

    const mb = n => n ? (n / 1048576 >= 10 ? Math.round(n / 1048576) : (n / 1048576).toFixed(1)) + ' MB' : '';

    async function load() {
        loading = true;
        fail = '';
        const r = await api.hubList();
        loading = false;
        if (!r) return;
        list = r.mods || [];
        if (!r.ok) fail = r.error || '?';
    }

    onMount(() => {
        api.onHubProgress(d => { if (d.id === busy) prog = { got: d.got, total: d.total }; });
        load();
    });

    async function get(m) {
        if (busy) return;
        selectBlip();
        busy = m.id;
        prog = { got: 0, total: m.size };
        msg[m.id] = null;
        const r = await api.hubInstall(m.id);
        busy = null;
        if (r && r.ok) {
            list = r.mods;
            msg[m.id] = [true, tx('hbDone') + r.folder];
            await refreshMods();
        } else {
            msg[m.id] = [false, tx('hbErr') + (r && r.error ? ': ' + r.error : '')];
        }
    }
</script>

<div class="bar">
    <button class="btn" onpointerenter={hoverBlip} disabled={loading || !!busy} onclick={load}>{tx('hbRefresh')}</button>
    <span class="grow"></span>
    {#if loading}<span class="note">{tx('hbLoading')}</span>{/if}
    {#if fail}<span class="note bad">{tx('hbFail')} ({fail})</span>{/if}
</div>

{#if !loading && !fail && !list.length}
    <p class="empty">{tx('hbNone')}</p>
{/if}

<ul class="mods">
    {#each list as m (m.id)}
        <li class="card">
            <span class="pic">{#if m.preview}<img src={m.preview} alt="">{:else}<i class="cat {m.category}"></i>{/if}</span>
            <div class="what">
                <b>{m.name}</b>
                <small>{[m.author, tx('cats')[m.category], mb(m.size)].filter(Boolean).join(' · ')}</small>
                {#if m.description}<p>{m.description}</p>{/if}
                <small class="ver">
                    {tx('hbHub')} v{m.version}
                    {#if m.installed !== null} · {tx('hbYours')} {m.installed ? 'v' + m.installed : '?'}{/if}
                </small>
                {#if busy === m.id}
                    <div class="prog"><i style="width: {prog.total ? Math.min(100, prog.got / prog.total * 100) : 0}%"></i></div>
                    <small>{tx('hbDl')} {mb(prog.got)}{prog.total ? ' / ' + mb(prog.total) : ''}</small>
                {:else if msg[m.id]}
                    <small class:bad={!msg[m.id][0]} class:good={msg[m.id][0]}>{msg[m.id][1]}</small>
                {/if}
            </div>
            <div class="act">
                {#if m.installed === null}
                    <button class="btn hot" onpointerenter={hoverBlip} disabled={!!busy} onclick={() => get(m)}>{tx('hbGet')}</button>
                {:else if m.update}
                    <button class="btn hot" onpointerenter={hoverBlip} disabled={!!busy} onclick={() => get(m)}>{tx('hbUpdate')}</button>
                {:else}
                    <span class="have">{tx('hbHave')}</span>
                    <button class="btn" onpointerenter={hoverBlip} disabled={!!busy} onclick={() => get(m)}>{tx('hbAgain')}</button>
                {/if}
            </div>
        </li>
    {/each}
</ul>

<p class="note">{tx('hbHow')}</p>

<style>
    .bar { display: flex; align-items: center; gap: calc(var(--u) * 1); margin-bottom: calc(var(--u) * 1.2); }
    .grow { flex: 1; }
    .empty { color: var(--dim); text-align: center; margin: calc(var(--u) * 3) 0; }
    .mods { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: calc(var(--u) * .6); }
    .card {
        display: flex; align-items: center; gap: calc(var(--u) * 1.2);
        background: var(--panel); border: 1px solid var(--line); border-radius: var(--r-sm);
        padding: calc(var(--u) * .8) calc(var(--u) * 1);
    }
    .pic { width: calc(var(--u) * 16); height: calc(var(--u) * 10); flex: none; display: grid; place-items: center; }
    .pic img { width: 100%; height: 100%; object-fit: cover; border-radius: var(--r-sm); border: 1px solid var(--line); }
    .cat { width: 50%; height: 80%; border-radius: 50%; background: var(--grad-hot); box-shadow: 0 0 calc(var(--u) * .6) rgba(140, 130, 255, .6); }
    .cat.audio { background: #1f6f6a; }
    .cat.music { background: #8a3c8f; }
    .cat.models { background: #9a6a1c; }
    .cat.mixed { background: #4b4b6b; }
    .what { flex: 1; min-width: 0; display: flex; flex-direction: column; gap: calc(var(--u) * .2); }
    .what b { font-weight: 600; }
    .what small { color: var(--dim); font-size: calc(var(--t) * 1.3); }
    .what p { margin: calc(var(--u) * .2) 0; font-size: calc(var(--t) * 1.4); }
    .what .ver { color: var(--text, inherit); }
    .act { display: flex; flex-direction: column; align-items: flex-end; gap: calc(var(--u) * .5); }
    .have { color: var(--dim); letter-spacing: .08em; font-size: calc(var(--t) * 1.3); }
    .prog { height: calc(var(--u) * .6); background: var(--well); border-radius: calc(var(--u) * .3); overflow: hidden; margin-top: calc(var(--u) * .3); }
    .prog i { display: block; height: 100%; background: var(--grad-hot); }
    small.bad, .note.bad { color: #ff9aa0; }
    small.good { color: #8fe3a0; }
</style>
