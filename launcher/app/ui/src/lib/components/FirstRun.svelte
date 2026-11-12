<script>
    /* The first start's windows (0.4.4, Mary), so a player who does not
     * know the options finds them:
     *   1. HOW TO PLAY, while umk3.ini has no preset=: ORIGINAL (the
     *      iPhone's game) or ENHANCED (the screen's largest resolution, no
     *      touch controls, ENHANCED LIGHTING); a folder that already had
     *      settings may keep them (lib/ini.js applyPreset).
     *   2. YOUR ACCOUNT, once (account_asked=), to a player with no account
     *      while the server answers: make one or log in (ProfAccount in its
     *      compact form, the recovery code included), or play without.
     *   3. WHAT'S NEW, once per launcher version (lib/whatsnew.json), and
     *      again from UPDATES' button (app.whatsNew = true). */
    import { onMount, onDestroy } from 'svelte';
    import { app, tx, save } from '#lib/app.svelte.js';
    import ProfAccount from '#lib/sections/ProfAccount.svelte';
    import { api } from '#lib/platform.js';
    import { portal } from '#lib/portal.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';

    let step = $state('');              /* '', 'preset', 'login', 'news' */
    let acc = $state(null);             /* account:status while YOUR ACCOUNT is up */
    let news = $state(null);            /* whatsnew:get's answer */
    let busy = $state(false);

    const lines = e => e[app.ui === 'ES' ? 'ES' : 'EN'] || e.EN || [];

    /* the next window after `from` */
    async function next(from) {
        if (from === '' && app.cfg && !app.cfg.preset) { step = 'preset'; return; }
        if ((from === '' || from === 'preset') && app.cfg && !app.cfg.accountAsked) {
            const st = await api.account?.status().catch(() => null);
            if (st && st.server && !st.user) { acc = st; step = 'login'; return; }
        }
        step = news?.show ? 'news' : '';
    }

    onMount(async () => {
        news = await api.whatsNew?.();
        await next('');
    });

    async function closeLogin() {
        selectBlip();
        await save({ accountAsked: true });
        await next('login');
    }

    /* UPDATES' WHAT'S NEW button */
    $effect(() => {
        if (app.whatsNew && step === '' && news?.entries?.length) {
            step = 'news';
            app.whatsNew = false;
        }
    });

    async function choose(name) {
        if (busy) return;
        busy = true;
        selectBlip();
        const cfg = await api.applyPreset(name);
        if (cfg) app.cfg = cfg;
        busy = false;
        await next('preset');
    }

    async function closeNews() {
        selectBlip();
        step = '';
        if (news?.show) {
            news.show = false;
            await api.whatsNewSeen();
        }
    }

    let counted = false;
    $effect(() => {
        const on = step !== '';
        if (on && !counted) { app.overlay++; counted = true; }
        else if (!on && counted) { app.overlay--; counted = false; }
    });
    onDestroy(() => { if (counted) app.overlay--; });

    function onKey(e) {
        if (step !== 'news' || e.key !== 'Escape') return;
        e.stopPropagation();
        e.preventDefault();
        closeNews();
    }
    onMount(() => window.addEventListener('keydown', onKey, true));
    onDestroy(() => window.removeEventListener('keydown', onKey, true));
</script>

{#if step === 'preset'}
    <div class="veil" role="presentation" use:portal>
        <div class="win" role="dialog" aria-modal="true" aria-label={tx('frTitle')}>
            <i class="orn tl"></i><i class="orn tr"></i><i class="orn bl"></i><i class="orn br"></i>
            <h2>{tx('frTitle')}</h2>
            <p class="sub">{tx('frSub')}</p>
            <div class="cards">
                <button class="card" disabled={busy} onpointerenter={hoverBlip} onclick={() => choose('original')}>
                    <b>{tx('frOrig')}</b>
                    <span>{tx('frOrigD')}</span>
                </button>
                <button class="card hot" disabled={busy} onpointerenter={hoverBlip} onclick={() => choose('enhanced')}>
                    <em>{tx('frRec')}</em>
                    <b>{tx('frEnh')}</b>
                    <span>{tx('frEnhD')}</span>
                </button>
            </div>
            {#if news && !news.fresh}
                <div class="foot">
                    <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={() => choose('kept')}>{tx('frKeep')}</button>
                </div>
            {/if}
        </div>
    </div>
{:else if step === 'login'}
    <div class="veil" role="presentation" use:portal>
        <div class="win acc" role="dialog" aria-modal="true" aria-label={tx('loTitle')}>
            <i class="orn tl"></i><i class="orn tr"></i><i class="orn bl"></i><i class="orn br"></i>
            <h2>{tx('loTitle')}</h2>
            <p class="sub">{tx('loSub')}</p>
            <div class="form"><ProfAccount compact onstatus={s => { acc = s; }} /></div>
            <div class="foot end">
                <button class="btn" class:hot={acc && acc.user} onpointerenter={hoverBlip}
                        onclick={closeLogin}>{tx(acc && acc.user ? 'loDone' : 'loGuest')}</button>
            </div>
        </div>
    </div>
{:else if step === 'news'}
    <div class="veil" role="presentation" use:portal onclick={e => { if (e.target === e.currentTarget) closeNews(); }}>
        <div class="win" role="dialog" aria-modal="true" aria-label={tx('wnTitle')}>
            <i class="orn tl"></i><i class="orn tr"></i><i class="orn bl"></i><i class="orn br"></i>
            <h2>{tx('wnTitle')}</h2>
            <div class="list">
                {#each news.entries as e, i}
                    <section class:cur={e.version === news.current}>
                        <h3>{tx('wnVer')} {e.version}{e.date ? ' · ' + e.date : ''}</h3>
                        <ul>{#each lines(e) as l}<li>{l}</li>{/each}</ul>
                    </section>
                {/each}
            </div>
            <div class="foot end">
                <button class="btn hot" onpointerenter={hoverBlip} onclick={closeNews}>{tx('wnOk')}</button>
            </div>
        </div>
    </div>
{/if}

<style>
    .veil {
        position: fixed; inset: 0; z-index: 60;
        display: flex; align-items: center; justify-content: center;
        background: radial-gradient(ellipse at 50% 45%, rgba(4, 6, 10, .72), rgba(0, 0, 0, .9));
        backdrop-filter: blur(3px) saturate(.6);
        animation: fade .18s ease-out;
    }
    @keyframes fade { from { opacity: 0; } }
    .win {
        position: relative; width: min(92vw, calc(var(--u) * 84)); max-height: 88vh;
        display: flex; flex-direction: column;
        padding: calc(var(--u) * 2) calc(var(--u) * 2.4);
        background: var(--veil); box-shadow: var(--glow-in), 0 calc(var(--u) * 1) calc(var(--u) * 4) #000;
        border-left: calc(var(--u) * .45) solid #2e3846; border-right: calc(var(--u) * .45) solid #2e3846;
        border-top: calc(var(--u) * .6) solid #e6b75c; border-bottom: calc(var(--u) * .45) solid #8e150c;
        animation: rise .22s ease-out;
    }
    @keyframes rise { from { transform: translateY(calc(var(--u) * 1.5)) scale(.98); opacity: 0; } }
    .orn { position: absolute; width: calc(var(--u) * 2.4); height: calc(var(--u) * 2.4); background-size: contain; background-repeat: no-repeat; pointer-events: none; }
    .orn.tl { left: calc(var(--u) * -.9); top: calc(var(--u) * -1); background-image: var(--orn-tl); }
    .orn.tr { right: calc(var(--u) * -.9); top: calc(var(--u) * -1); background-image: var(--orn-tr); }
    .orn.bl { left: calc(var(--u) * -.9); bottom: calc(var(--u) * -.9); background-image: var(--orn-bl); }
    .orn.br { right: calc(var(--u) * -.9); bottom: calc(var(--u) * -.9); background-image: var(--orn-br); }
    h2 { font-size: calc(var(--u) * 2.2); text-align: center; }
    .sub { text-align: center; color: var(--dim); font-size: calc(var(--u) * 1.15); margin: calc(var(--u) * .4) 0 calc(var(--u) * 1.4); }
    .cards { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: calc(var(--u) * 1.4); }
    .card {
        position: relative; display: flex; flex-direction: column; gap: calc(var(--u) * .7); text-align: left;
        padding: calc(var(--u) * 1.6) calc(var(--u) * 1.6) calc(var(--u) * 1.8);
        background: var(--plate); border: 1px solid #2e3846; border-radius: 3px; cursor: pointer;
        box-shadow: var(--bevel); transition: border-color .12s, transform .12s, box-shadow .12s;
    }
    .card b { font-size: calc(var(--u) * 1.9); letter-spacing: .06em; color: var(--text); text-shadow: 0 1px 0 #000; }
    .card span { font-size: calc(var(--u) * 1.1); line-height: 1.4; color: var(--dim); }
    .card em {
        position: absolute; top: calc(var(--u) * -.8); right: calc(var(--u) * 1); font-style: normal;
        font-size: calc(var(--u) * .9); font-weight: 800; letter-spacing: .08em;
        padding: calc(var(--u) * .2) calc(var(--u) * .7); background: #e6bb63; color: #1a1206; border-radius: 2px;
    }
    .card:hover { border-color: #e6bb63; transform: translateY(-2px); box-shadow: var(--bevel), var(--hover-glow); }
    .card.hot {
        border-color: #e6bb63;
        background: linear-gradient(rgba(215, 228, 255, .14), rgba(255, 255, 255, 0) 50%), var(--tex-grain), linear-gradient(#7d1913, #2c0503);
    }
    .card.hot span { color: var(--text); }
    .win.acc { width: min(92vw, calc(var(--u) * 64)); }
    .form { overflow-y: auto; min-height: 0; padding-right: calc(var(--u) * .4); }
    .list { overflow-y: auto; padding-right: calc(var(--u) * .6); }
    section { padding: calc(var(--u) * .8) 0; border-bottom: 1px solid #2e3846; }
    section:last-child { border-bottom: 0; }
    h3 { font-size: calc(var(--u) * 1.25); color: var(--dim); letter-spacing: .06em; }
    section.cur h3 { color: #e6bb63; }
    ul { margin: calc(var(--u) * .5) 0 0; padding-left: calc(var(--u) * 1.6); }
    li { font-size: calc(var(--u) * 1.15); line-height: 1.45; color: var(--text); margin-bottom: calc(var(--u) * .35); }
    section:not(.cur) li { color: var(--dim); }
    .foot { display: flex; justify-content: center; margin-top: calc(var(--u) * 1.4); }
    .foot.end { justify-content: flex-end; }
</style>
