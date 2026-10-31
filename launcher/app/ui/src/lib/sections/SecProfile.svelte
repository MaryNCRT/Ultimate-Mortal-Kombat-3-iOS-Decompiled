<script>
    /* PROFILE: profiles.ini, which the game's settings menu edits too. The
     * name, the id (one per installation, it never changes), the fighter in
     * his first or second outfit, the background's gradient (the gradient
     * picker: two colours and an angle) and a reset. The icon stays in sight
     * while the page scrolls to the colours, so every change shows. Its
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

    const p = $derived(app.prof);
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
        <div class="prof">
            <IconCanvas class="big" size={192} icon={p.icon} color={p.color} shade={p.shade} color2={p.color2}
                        angle={p.angle} alt={p.alt} />
            <div class="fields">
                <div class="field wide">
                    <label class="lbl" for="profName">{tx('profName')}</label>
                    <input id="profName" class="edt" maxlength="24" spellcheck="false" bind:value={name}
                           onfocus={() => typing = true}
                           onblur={() => typing = false}
                           oninput={() => updateProf({ name: name.replace(/[|\r\n\t]/g, ' ') })}
                           onchange={nameDone}>
                    {#if nameMsg}<span class="taken">{nameMsg}</span>{/if}
                </div>
                <div class="field wide">
                    <span class="lbl">{tx('profId')}</span>
                    <div class="idcode">{p.id}</div>
                </div>
            </div>
        </div>

        <h3>{tx('profIconLbl')}</h3>
        <div class="icongrid">
            {#each { length: 24 } as _, i}
                <IconCanvas class={i === p.icon ? 'on' : ''} size={64} icon={i} color={p.color} shade={p.shade}
                            color2={p.color2} angle={p.angle} alt={p.alt} onclick={() => pick({ icon: i })} />
            {/each}
        </div>
        <span class="tabs sub shades">
            {#each [[0, 'profOutfit1'], [1, 'profOutfit2']] as [a, key]}
                <button class="tab" class:active={(p.alt ? 1 : 0) === a} onpointerenter={hoverBlip}
                        onclick={() => pick({ alt: a })}>{tx(key)}</button>
            {/each}
        </span>

        <h3>{tx('profColor')}</h3>
        <GradientPicker label={tx('gpLabel')} value={gradCss} onchange={setGradient} />
        <div class="row end">
            <button class="btn" onpointerenter={hoverBlip} onclick={resetProf}>{tx(app.profResetArmed ? 'profResetAsk' : 'profReset')}</button>
        </div>
    {/if}
</Page>

<style>
    /* the icon on the left, the fields beside it; it stays in sight */
    .prof {
        display: flex; gap: calc(var(--u) * 2.5); align-items: flex-start;
        position: sticky; top: calc(var(--u) * -2.2); z-index: 2;
        margin: 0 calc(var(--u) * -2.6);
        padding: calc(var(--u) * .8) calc(var(--u) * 2.6) calc(var(--u) * 1.2);
        background: linear-gradient(rgba(14, 11, 40, .98) 85%, rgba(14, 11, 40, 0));
    }
    .prof .fields { flex: 1; grid-template-columns: minmax(0, 1fr); }
    .prof :global(canvas.big) { width: calc(var(--u) * 16); height: calc(var(--u) * 16); flex: none; }
    .idcode {
        height: var(--ctl-h); line-height: var(--ctl-h); white-space: nowrap;
        font-family: var(--font-mono); font-size: calc(var(--t) * 1.8);
        letter-spacing: .12em; color: var(--code);
    }
    .icongrid { display: grid; grid-template-columns: repeat(12, 1fr); gap: calc(var(--u) * .5); }
    .icongrid :global(canvas) { width: 100%; cursor: pointer; border-radius: 18%; outline: 2px solid transparent; }
    .icongrid :global(canvas.on) { outline-color: #fff; }
    .taken { color: #ffb347; font-weight: bold; }
    .shades { margin: calc(var(--u) * .4) 0 0; }
</style>
