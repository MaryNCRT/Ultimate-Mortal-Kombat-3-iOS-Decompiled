<script>
    /* COMPILE YOUR .IPA: the player's .ipa, which build, the compile with
     * its bar and log, and the updates from GitHub with the latest notes. */
    import { app, tx, ARCH, save, refreshRoot, startBuild, browseIpa, checkUpdate, installUpdate } from '#lib/app.svelte.js';
    import { api, isWin } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import Bar from '#lib/components/Bar.svelte';
    import Notes from '#lib/components/Notes.svelte';

    let log = $state();
    const archIdx = $derived(Math.max(0, ARCH.indexOf(app.cfg.arch || (isWin ? 'x86' : 'x64'))));
    const when = $derived(app.builtAt ? new Date(app.builtAt).toLocaleString(app.ui === 'ES' ? 'es' : 'en') : '');

    /* the log follows its last line */
    $effect(() => {
        app.build.log;
        if (log) log.scrollTop = log.scrollHeight;
    });

    async function arch(e) {
        await save({ arch: ARCH[e.currentTarget.selectedIndex] });
        await refreshRoot();          /* compiled or not: that exe's state */
    }
</script>

<Page>
    <div class="state {app.ready ? 'ok' : 'need'}">{app.ready ? tx('builtAt') + ' — ' + when : tx('notBuilt')}</div>
    <label class="lbl" for="ipa">{tx('ipaLbl')}</label>
    <div class="row">
        <input id="ipa" class="edt" readonly spellcheck="false" placeholder="UMK3.ipa" value={app.cfg.ipa || ''}>
        <button class="btn" disabled={app.build.on} onpointerenter={hoverBlip} onclick={browseIpa}>{tx('browse')}</button>
    </div>
    <label class="lbl" for="arch">{tx('archLbl')}</label>
    <div class="row">
        <select id="arch" class="sel" disabled={app.build.on} value={archIdx} onchange={arch}>
            {#each tx('arches') as n, i}<option value={i}>{n}</option>{/each}
        </select>
    </div>
    <div class="row">
        <button class="btn hot" disabled={app.build.on || app.upd.busy} onpointerenter={hoverBlip}
                onclick={startBuild}>{tx(app.ready ? 'rebuild' : 'build')}</button>
        {#if app.build.on}
            <button class="btn danger" onpointerenter={hoverBlip} onclick={() => api.cancelBuild()}>{tx('cancel')}</button>
        {/if}
        {#if app.build.show}
            <div class="progress">
                <Bar pct={app.build.pct} fail={app.build.fail} />
                <span class="plbl">{app.build.label}</span>
            </div>
        {/if}
    </div>
    {#if app.build.console}<pre class="console" bind:this={log}>{app.build.log}</pre>{/if}

    <h3>{tx('gUpdates')}</h3>
    <div class="updrow">
        <div class="state {app.upd.cls}">{app.upd.text}</div>
        <button class="btn" disabled={app.upd.checking || app.upd.busy} onpointerenter={hoverBlip}
                onclick={() => { selectBlip(); checkUpdate(); }}>{tx('updCheck')}</button>
        {#if app.upd.canGet}
            <button class="btn hot" disabled={app.upd.busy} onpointerenter={hoverBlip} onclick={installUpdate}>{tx('updGet')}</button>
        {/if}
    </div>
    {#if app.upd.bar}
        <div class="progress upd">
            <Bar pct={app.upd.pct} />
            <span class="plbl">{app.upd.label}</span>
        </div>
    {/if}
    <span class="lbl notes-lbl">{tx('notesLbl')}{app.upd.notesLbl ? ' — ' + app.upd.notesLbl : ''}</span>
    <Notes md={app.upd.notes} />
</Page>

<style>
    .progress { flex: 1; display: flex; gap: calc(var(--u) * 1); align-items: center; }
    .progress.upd { flex: none; margin-top: calc(var(--u) * .8); }
    .plbl { color: var(--dim); font-size: calc(var(--u) * 1.5); }
    .upd .plbl { font-size: calc(var(--u) * 1.4); }
    .console {
        margin: calc(var(--u) * 1) 0 0; padding: calc(var(--u) * 1);
        flex: 1 1 auto; min-height: calc(var(--u) * 8); overflow: auto;
        background: rgba(0, 0, 0, .7); border: 1px solid var(--line);
        font-family: var(--font-mono); font-size: calc(var(--u) * 1.3); color: #9fc0e0;
        user-select: text; white-space: pre-wrap;
    }
    .updrow { display: flex; gap: calc(var(--u) * 1); align-items: center; margin-top: calc(var(--u) * 1.4); }
    .updrow .state { flex: 1; margin: 0; }
    .notes-lbl { margin-top: calc(var(--u) * 1.2); }
</style>
