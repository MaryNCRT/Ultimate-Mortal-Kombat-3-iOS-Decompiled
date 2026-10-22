<script>
    /* PROFILE: profiles.ini, which the game's settings menu edits too. The
     * name, the id (one per installation, it never changes), the fighter,
     * the background colour and its gradient, and a reset. The icon stays in
     * sight while the page scrolls to the colours, so every change shows. */
    import { app, tx, updateProf, resetProf } from '#lib/app.svelte.js';
    import { PROF_COLORS } from '#lib/profileIcon.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import IconCanvas from '#lib/components/IconCanvas.svelte';

    const p = $derived(app.prof);
    let name = $state('');
    let typing = $state(false);
    /* the name follows the profile, except while it is being typed */
    $effect(() => {
        if (p && !typing) name = p.name;
    });

    const color2Lbl = $derived(p ? tx(p.shade === 'C' ? 'profColor2' : p.shade === 'D' ? 'profColorDark' : 'profColorLight') : '');
    const SHADES = [['L', 'profLight'], ['D', 'profDark'], ['C', 'profCustom']];

    function pick(fields) {
        selectBlip();
        updateProf(fields);
    }
</script>

{#snippet swatches(cur, key)}
    <div class="palette">
        {#each PROF_COLORS as c}
            <button class="swatch" class:on={c === cur} style:background="#{c}" aria-label={c}
                    onclick={() => pick({ [key]: c })}></button>
        {/each}
    </div>
{/snippet}

<Page>
    {#if p}
        <div class="prof">
            <IconCanvas class="big" size={192} icon={p.icon} color={p.color} shade={p.shade} color2={p.color2} />
            <div class="fields">
                <div class="field wide">
                    <label class="lbl" for="profName">{tx('profName')}</label>
                    <input id="profName" class="edt" maxlength="24" spellcheck="false" bind:value={name}
                           onfocus={() => typing = true}
                           onblur={() => typing = false}
                           oninput={() => updateProf({ name: name.replace(/[|\r\n\t]/g, ' ') })}
                           onchange={() => { if (!name.trim()) updateProf({ name: tx('profDefault') }); }}>
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
                            color2={p.color2} onclick={() => pick({ icon: i })} />
            {/each}
        </div>

        <h3>{tx('profColor')}</h3>
        <div class="row top">
            {@render swatches(p.color, 'color')}
            <input type="color" class="colorpick" title="+" value={'#' + p.color.toLowerCase()}
                   oninput={e => updateProf({ color: e.currentTarget.value.replace('#', '').toUpperCase() })}>
        </div>

        <h3>{tx('profShadeLbl')}</h3>
        <span class="tabs sub shades">
            {#each SHADES as [s, key]}
                <button class="tab" class:active={p.shade === s} onpointerenter={hoverBlip}
                        onclick={() => pick({ shade: s })}>{tx(key)}</button>
            {/each}
        </span>
        <div>
            <span class="lbl">{color2Lbl}</span>
            <div class="row top">
                {@render swatches(p.color2, 'color2')}
                <input type="color" class="colorpick" title="+" value={'#' + p.color2.toLowerCase()}
                       oninput={e => updateProf({ color2: e.currentTarget.value.replace('#', '').toUpperCase() })}>
            </div>
        </div>
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
        font-family: var(--font-mono); font-size: calc(var(--u) * 1.8);
        letter-spacing: .12em; color: var(--code);
    }
    .icongrid { display: grid; grid-template-columns: repeat(12, 1fr); gap: calc(var(--u) * .5); }
    .icongrid :global(canvas) { width: 100%; cursor: pointer; border-radius: 18%; outline: 2px solid transparent; }
    .icongrid :global(canvas.on) { outline-color: #fff; }
    /* the palette: 16 across, as the game's */
    .palette { display: grid; grid-template-columns: repeat(16, 1fr); gap: calc(var(--u) * .35); flex: 1; }
    .swatch {
        aspect-ratio: 1; width: 100%; border-radius: 50%;
        border: 1.5px solid rgba(255, 255, 255, .35); cursor: pointer; padding: 0;
    }
    .swatch:hover { border-color: #fff; }
    .swatch.on { border-color: #fff; box-shadow: 0 0 0 2px var(--blue), 0 0 calc(var(--u) * .8) #fff; }
    .colorpick { width: calc(var(--u) * 5); height: var(--ctl-h); padding: 0; border: 0; background: none; cursor: pointer; }
    .shades { margin: calc(var(--u) * .4) 0 0; }
</style>
