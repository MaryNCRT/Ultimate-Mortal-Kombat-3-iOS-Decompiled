<script>
    /* The window has no frame: this bar drags it and holds its buttons, the
     * language first. On the menu the version sits at its left. */
    import { app, tx, setLang } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip } from '#lib/sound.js';

    function lang() {
        selectBlip();
        setLang(app.ui === 'ES' ? 'EN' : 'ES');
    }
</script>

<header class="titlebar">
    <div class="drag">
        {#if !app.full}<span class="ver">{tx('ttlSub')} · {app.version}</span>{/if}
    </div>
    <div class="btns">
        <button class="tb" title="Idioma / Language" onclick={lang}>{tx('ui')}</button>
        <button class="tb" title="—" onclick={() => api.minimize()}>&#8212;</button>
        <button class="tb" title="□" onclick={() => api.maximize()}>&#9633;</button>
        <button class="tb" title="F11" onclick={() => api.fullscreen()}>&#9974;</button>
        <button class="tb x" title="×" onclick={() => api.close()}>&#215;</button>
    </div>
</header>

<style>
    .titlebar { grid-area: tb; position: relative; display: flex; z-index: 10; }
    .drag {
        flex: 1; -webkit-app-region: drag;
        display: flex; align-items: center; padding-left: calc(var(--u) * 1.6);
    }
    .ver { color: var(--head); font-size: calc(var(--t) * 1.3); letter-spacing: .18em; text-transform: uppercase; text-shadow: 0 1px 0 #000; }
    .btns { display: flex; -webkit-app-region: no-drag; }
    .tb {
        min-width: calc(var(--u) * 5); height: 100%;
        background: transparent; border: 0; color: var(--dim);
        font-size: calc(var(--t) * 1.7); cursor: pointer; padding: 0 calc(var(--u) * 1.2);
        text-shadow: 0 1px 0 #000;
    }
    .tb:hover { color: #ffffff; background: rgba(120, 140, 170, .18); }
    .tb.x:hover { background: var(--blood); color: #fff; }
</style>
