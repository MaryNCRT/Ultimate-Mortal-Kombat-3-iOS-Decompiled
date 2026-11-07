<script>
    /* PROFILE: profiles.ini, which the game's settings menu edits too. The
     * name, the id (one per installation, it never changes), the fighter in
     * his first or second outfit, the background's gradient (the gradient
     * picker: two colours and an angle) and a reset. 0.4.2 (Mary): the Player
     * Banner on top -- the gradient on steel, the fighter, name, state, id and
     * favourite -- stays in sight while the page scrolls, so every change shows
     * on it; under it IDENTITY (name; the fighter -- icon and favourite are
     * one, Mary -- chosen in a window of his own, FighterPicker; the outfit)
     * and LOOK (the background). Its
     * other pages: the account (ProfAccount) and the friends (ProfFriends). */
    import { app, tx, updateProf, resetProf, PROF_TABS } from '#lib/app.svelte.js';
    import SubNav from '#lib/components/SubNav.svelte';
    import ProfAccount from './ProfAccount.svelte';
    import ProfFriends from './ProfFriends.svelte';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import IconCanvas from '#lib/components/IconCanvas.svelte';
    import GradientPicker from '#lib/components/GradientPicker.svelte';
    import PlayerBanner from '#lib/components/PlayerBanner.svelte';
    import FighterPicker from '#lib/components/FighterPicker.svelte';
    import { FIGHTERS } from '#lib/fighters.js';

    const p = $derived(app.prof);
    let picking = $state(false);       /* the fighter's window open */
    let name = $state('');
    let typing = $state(false);
    /* the name follows the profile, except while it is being typed */
    $effect(() => {
        if (p && !typing) name = p.name;
    });

    /* Logged in, the name is the account's and no two players share one
     * (lib/account.js, rename): a name someone else has goes back to the
     * one the account keeps. */
    let nameMsg = $state('');
    async function nameDone() {
        if (!name.trim()) await updateProf({ name: tx('profDefault') });
        const r = await api.account.rename(app.prof.name);
        nameMsg = r.error === 'name_taken' ? tx('profNameTaken') : '';
        if (r.error === 'name_taken') await updateProf({ name: r.name || tx('profDefault') });
    }

    function pick(fields) {
        selectBlip();
        updateProf(fields);
    }

    /* The profile's gradient as the picker's CSS. An older one (light, dark,
     * or custom without an angle) is shown as the straight gradient nearest
     * to it; it changes only when the player picks. */
    const hex = c => '#' + c.toLowerCase();
    const gradCss = $derived.by(() => {
        if (!p) return '';
        if (p.shade === 'C' && p.angle >= 0)
            return p.color === p.color2 ? hex(p.color) : `linear-gradient(${p.angle}deg, ${hex(p.color)}, ${hex(p.color2)})`;
        return p.shade === 'L' ? `linear-gradient(180deg, ${hex(p.color2)}, ${hex(p.color)})`
                               : `linear-gradient(180deg, ${hex(p.color)}, ${hex(p.color2)})`;
    });

    /* the picker's CSS into the profile: two colours (one: both ends) and the
     * angle; none -- QUITAR -- is the default light gradient again */
    const NAMED = { 'to top': 0, 'to right': 90, 'to bottom': 180, 'to left': 270 };
    function full(h) {
        h = h.replace('#', '');
        if (h.length === 3 || h.length === 4) h = h.slice(0, 3).split('').map(c => c + c).join('');
        return h.slice(0, 6).toUpperCase();
    }
    function setGradient(css) {
        if (!css) {
            updateProf({ shade: 'L', color: '3A8EE6', angle: -1 });
            return;
        }
        const hexes = css.match(/#[0-9a-fA-F]{3,8}/g) || [];
        if (!hexes.length) return;
        const deg = /(\d+(?:\.\d+)?)deg/.exec(css), named = /(to (?:top|right|bottom|left))/.exec(css);
        const angle = deg ? Math.round(parseFloat(deg[1])) % 360 : named ? NAMED[named[1]] : 180;
        updateProf({ shade: 'C', color: full(hexes[0]), color2: full(hexes[1] || hexes[0]), angle });
    }
</script>

<Page>
    <SubNav tabs={PROF_TABS} current={app.profTab} pick={t => app.profTab = t} dot={app.frIn ? ['friends'] : []} />
    {#if app.profTab === 'account'}
        <ProfAccount />
    {:else if app.profTab === 'friends'}
        <ProfFriends />
    {:else if p}
        <div class="pin"><PlayerBanner grad={gradCss} /></div>

        <div class="cols">
            <section>
                <h3 class="first">{tx('profIdent')}</h3>
                <div class="fields one">
                <div class="field">
                    <label class="lbl" for="profName">{tx('profName')}</label>
                    <input id="profName" class="edt" maxlength="24" spellcheck="false" bind:value={name}
                           onfocus={() => typing = true}
                           onblur={() => typing = false}
                           oninput={() => updateProf({ name: name.replace(/[|\r\n\t]/g, ' ') })}
                           onchange={nameDone}>
                    {#if nameMsg}<span class="taken">{nameMsg}</span>{/if}
                </div>
                <div class="field">
                    <span class="lbl">{tx('profFavLbl')}</span>
                    <button class="fighter" onpointerenter={hoverBlip} onclick={() => { selectBlip(); picking = true; }}>
                        <IconCanvas size={64} icon={p.icon} color={p.color} shade={p.shade} color2={p.color2} angle={p.angle} alt={p.alt} />
                        <span class="fname">{FIGHTERS[app.ui === 'ES' ? 'ES' : 'EN'][p.icon]}</span>
                        <span class="chg">{tx('profPick')}</span>
                    </button>
                </div>
                </div>
                <span class="tabs sub shades">
                    {#each [[0, 'profOutfit1'], [1, 'profOutfit2']] as [a, key]}
                        <button class="tab" class:active={(p.alt ? 1 : 0) === a} onpointerenter={hoverBlip}
                                onclick={() => pick({ alt: a })}>{tx(key)}</button>
                    {/each}
                </span>
                <div class="row">
                    <button class="btn" onpointerenter={hoverBlip} onclick={resetProf}>{tx(app.profResetArmed ? 'profResetAsk' : 'profReset')}</button>
                </div>
            </section>
            <section>
                <h3 class="first">{tx('profLook')} — {tx('profColor')}</h3>
                <GradientPicker label={tx('gpLabel')} value={gradCss} onchange={setGradient} />
            </section>
        </div>
        {#if picking}<FighterPicker {p} pick={pick} close={() => picking = false} />{/if}
    {/if}
</Page>

<style>
    /* the banner stays in sight while the page scrolls */
    .pin {
        position: sticky; top: calc(var(--u) * -2.2); z-index: 4;
        margin: calc(var(--u) * -2.2) calc(var(--u) * -2.6) calc(var(--u) * 1);
        padding: calc(var(--u) * .4) calc(var(--u) * 2.6) calc(var(--u) * 1.2);
        background: linear-gradient(rgba(10, 13, 19, .97) 82%, rgba(10, 13, 19, 0));
    }
    /* IDENTITY on the left, the background's editor on the right: the
     * page's width used, nothing left empty (Mary) */
    .cols { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: calc(var(--u) * 2.6); align-items: start; }
    .cols > section { min-width: 0; }
    .cols :global(.bg-picker) { width: 100%; }
    .fields.one { grid-template-columns: minmax(0, 1fr); }
    /* the fighter: his icon and name on a steel plate; a click opens the window */
    .fighter {
        display: flex; align-items: center; gap: calc(var(--u) * 1); height: calc(var(--u) * 5);
        padding: calc(var(--u) * .4) calc(var(--u) * 1.2) calc(var(--u) * .4) calc(var(--u) * .4);
        background: var(--plate); border: 1px solid var(--line); border-left: calc(var(--u) * .4) solid var(--blood);
        border-radius: var(--r-sm); box-shadow: var(--bevel); cursor: pointer; color: var(--text); text-align: left;
    }
    .fighter :global(canvas) { width: calc(var(--u) * 4.2); height: calc(var(--u) * 4.2); border-radius: 10%; box-shadow: 0 0 0 1px #000; flex: none; }
    .fname { flex: 1; font-weight: 900; letter-spacing: .06em; text-transform: uppercase; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .chg { color: var(--head); font-size: calc(var(--u) * 1.2); font-weight: 800; letter-spacing: .12em; }
    .fighter:hover { border-color: var(--hot-rim); box-shadow: var(--bevel), var(--hover-glow); }
    .taken { color: var(--bad-text); font-weight: bold; }
    .shades { margin: calc(var(--u) * .8) 0 0; }
</style>
