<script>
    /* PLAY: the game's language, which build when both are compiled, debug
     * mode, the intros, and the button. */
    import { app, tx, ARCH, LANGS, save, refreshRoot, doPlay } from '#lib/app.svelte.js';
    import { isWin } from '#lib/platform.js';
    import { hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';

    const when = $derived(app.builtAt ? new Date(app.builtAt).toLocaleString(app.ui === 'ES' ? 'es' : 'en') : '');
    const archIdx = $derived(Math.max(0, ARCH.indexOf(app.cfg.arch || (isWin ? 'x86' : 'x64'))));

    async function arch(e) {
        await save({ arch: ARCH[e.currentTarget.selectedIndex] });
        await refreshRoot();
    }
</script>

<Page>
    <div class="state {app.ready ? 'ok' : 'need'}">{app.ready ? tx('builtAt') + ' — ' + when : tx('notBuilt')}</div>
    <div class="field">
        <label class="lbl" for="lang">{tx('lang')}</label>
        <select id="lang" class="sel" value={app.cfg.language || 0}
                onchange={e => save({ language: e.currentTarget.selectedIndex })}>
            {#each LANGS as n, i}<option value={i}>{n || tx('auto')}</option>{/each}
        </select>
    </div>
    {#if app.both}
        <div class="field">
            <label class="lbl" for="playarch">{tx('playArchLbl')}</label>
            <select id="playarch" class="sel" value={archIdx} onchange={arch}>
                {#each tx('arches') as n, i}<option value={i}>{n}</option>{/each}
            </select>
        </div>
    {/if}
    <label class="chk"><input type="checkbox" checked={!!app.cfg.debug}
        onchange={e => save({ debug: e.currentTarget.checked })}><span>{tx('dbg')}</span></label>
    <label class="chk"><input type="checkbox" checked={!!app.cfg.skipIntro}
        onchange={e => save({ skipIntro: e.currentTarget.checked })}><span>{tx('skip')}</span></label>
    <button class="btn hot big" disabled={!app.ready || app.build.on} onpointerenter={hoverBlip} onclick={doPlay}>{tx('play')}</button>
</Page>
