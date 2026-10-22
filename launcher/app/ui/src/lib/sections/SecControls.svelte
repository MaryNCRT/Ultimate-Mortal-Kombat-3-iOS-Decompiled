<script>
    /* CONTROLS: three pages. KEYBOARD and GAMEPAD, for player one or two
     * (DUEL), with the 6- and 5-button layouts (each its keys, and the one
     * the game starts with); DEBUG, the debug mode's keys and its pad
     * combination. Click an action, then press its key; Esc cancels. */
    import { app, tx, CTL_TABS, KGROUPS, PAD_ROWS, keyGet, padGet, padSet, padWant, padKindOf,
             setCtlTab, setButtons, resetKeys, resetDbgKeys, resetPad, choosePad, choosePadIcons,
             setPadDbg, closePicker } from '#lib/app.svelte.js';
    import { PAD_SETS, PAD_ICON_NAMES } from '#lib/glyphs.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';
    import KeyFace from '#lib/components/KeyFace.svelte';
    import PadSlots from '#lib/components/PadSlots.svelte';

    const six = $derived(app.cfg.buttons === 6);
    const kind = $derived(padKindOf(app.pl));
    const padOn = $derived(padWant() !== 'off');
    const hint = $derived(tx(app.waitingKey ? 'press' : 'hint'));

    /* the pads to choose: none, the first one, XInput, each one seen, and
     * the chosen one if it is not connected now (● = connected) */
    const pads = $derived.by(() => {
        const seen = app.seenPads;
        const opts = [['off', tx('padOff')], ['auto', tx('padAuto')],
                      ['xinput', seen.xinput ? tx('padX') + ': ' + seen.xinput : tx('padX')]];
        for (const [v, n] of Object.entries(seen))
            if (v !== 'xinput') opts.push([v, n + '  (' + v.toUpperCase() + ')']);
        const want = padWant(), wantName = app.pl === 2 ? app.cfg.p2PadName : app.cfg.padName;
        if (!opts.some(o => o[0] === want))
            opts.push([want, (wantName || want.toUpperCase()) + '  ' + tx('padGone')]);
        return opts.map(([v, n]) => [v, (seen[v] ? '● ' : '') + n]);
    });
    const seenText = $derived(app.padLooking ? tx('padLook')
        : Object.keys(app.seenPads).length ? tx('padNow') + ': ' + Object.values(app.seenPads).join(', ')
        : tx('padSeen'));

    /* the picture of the stick and d-pad */
    const dirFiles = $derived(['Dpad', kind === 'xbox' || kind === 'nintendo' ? 'L' : 'L3']
        .map(f => app.glyphs[PAD_SETS[kind].dir + '/' + PAD_SETS[kind].extra[f]]).filter(Boolean));

    const padRowName = act => act === 'pause' ? tx('pads').start : act === 'moves' ? tx('pads').select
        : act === 'settings' ? tx('pads').settings : tx('keys')[act];

    function tab(fn, el) {
        selectBlip();
        fn();
        app.waitingKey = null;
        el.blur();
    }
    function wait(name) {
        selectBlip();
        app.waitingKey = name;
    }
</script>

{#snippet keygroup(title, group, extra)}
    <div class="kg">
        <h3>{tx(title)}</h3>
        <div class="keys">
            {#each KGROUPS[group]() as name (name)}
                <button class="keybtn" class:waiting={app.waitingKey === name}
                        onpointerenter={hoverBlip} onclick={() => wait(name)}>
                    <span class="kname">{tx('keys')[name]}</span>
                    <KeyFace vk={keyGet(name)} />
                </button>
            {/each}
        </div>
        {@render extra?.()}
    </div>
{/snippet}

<Page>
    <SubNav tabs={CTL_TABS} current={app.ctlTab} pick={setCtlTab} />

    <h2>{app.ctlTab === 'p' ? tx(app.pl === 2 ? 'hControls2' : 'hControls')
        : app.ctlTab === 'pad' ? tx(app.pl === 2 ? 'hPad2' : 'hPad') : tx('hDbgKeys')}</h2>

    <div class="tabs">
        {#if app.ctlTab !== 'dbg'}
            <span class="tabs sub">
                {#each [1, 2] as p}
                    <button class="tab" class:active={app.pl === p} onpointerenter={hoverBlip}
                            onclick={e => tab(() => { app.pl = p; closePicker(); }, e.currentTarget)}>{tx('tabP' + p)}</button>
                {/each}
            </span>
            <span class="tabs sub">
                {#each [6, 5] as b}
                    <button class="tab" class:active={(six ? 6 : 5) === b} onpointerenter={hoverBlip}
                            onclick={e => tab(() => setButtons(b), e.currentTarget)}>{tx('tab' + b)}</button>
                {/each}
            </span>
        {/if}
        {#if app.ctlTab === 'p'}
            <button class="btn reset" onpointerenter={hoverBlip} onclick={resetKeys}>{tx('reskey')}</button>
        {:else if app.ctlTab === 'dbg'}
            <button class="btn reset" onpointerenter={hoverBlip} onclick={resetDbgKeys}>{tx('reskey')}</button>
        {:else if padOn}
            <button class="btn reset" onpointerenter={hoverBlip} onclick={resetPad}>{tx('reskey')}</button>
        {/if}
    </div>

    {#if app.ctlTab === 'p'}
        <div class="kgroups">
            {@render keygroup('gMove', 'move')}
            {@render keygroup('gButtons', 'buttons')}
            {#if app.pl !== 2}
                {#snippet keyhint()}<p class="hint">{hint}</p>{/snippet}
                {@render keygroup('gSystem', 'system', keyhint)}
            {/if}
        </div>
        <p class="note">{tx('schemes')}</p>
    {:else if app.ctlTab === 'pad'}
        <div class="field devfield">
            <label class="lbl" for="pad">{tx('padLbl')}</label>
            <div class="row">
                <select id="pad" class="sel" value={padWant()} onchange={e => choosePad(e.currentTarget.value)}>
                    {#each pads as [v, n]}<option value={v}>{n}</option>{/each}
                </select>
                <span class="hint">{seenText}</span>
            </div>
            <label class="lbl" for="padIcons">{tx('padIconsLbl')}</label>
            <div class="row">
                <select id="padIcons" class="sel" value={(app.pl === 2 ? app.cfg.p2PadIcons : app.cfg.padIcons) || 'auto'}
                        onchange={e => choosePadIcons(e.currentTarget.value)}>
                    {#each tx('padIcons') as n, i}<option value={PAD_ICON_NAMES[i]}>{n}</option>{/each}
                </select>
            </div>
        </div>
        {#if padOn}
            <div class="kg padmap">
                <h3>{tx('gPad')}</h3>
                <div class="keys padkeys">
                    <div class="keybtn ro">
                        <span class="kname">{tx('pads').dir}</span>
                        <span class="kkey">
                            {#each dirFiles as src}<img class="glyph" {src} alt="" draggable="false">{:else}D-PAD / L-STICK{/each}
                        </span>
                    </div>
                    {#each PAD_ROWS[six ? 6 : 5] as act (act)}
                        <div class="keybtn ro">
                            <span class="kname">{padRowName(act)}</span>
                            <PadSlots value={padGet(act)} {kind} change={v => padSet(act, v)} />
                        </div>
                    {/each}
                </div>
            </div>
        {/if}
        <p class="note">{tx('padNote')}</p>
    {:else}
        <div class="kgroups">
            {@render keygroup('gDbgMenu', 'dbgmenu')}
            {@render keygroup('gDbgFight', 'dbgfight')}
            {@render keygroup('gDbgScreens', 'dbgscr')}
        </div>
        <div class="field devfield">
            <span class="lbl">{tx('padDbgLbl')}</span>
            <div class="row">
                <PadSlots value={app.cfg.padDbgMenu || ''} kind={padKindOf(1)} change={setPadDbg} />
            </div>
        </div>
        <p class="note">{tx('dbgKeysNote')}</p>
    {/if}
</Page>

<style>
    .reset { margin-left: auto; }
    .kg .hint { margin: calc(var(--u) * 1.4) 0 0; }
    .padmap { margin-top: calc(var(--u) * 1.6); }
    /* the GAMEPAD page: one row an action, its name and two equal selectors */
    .padkeys { display: grid; grid-template-columns: 1fr 1fr; gap: calc(var(--u) * .5) calc(var(--u) * 1.6); }
    .padkeys .keybtn {
        display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: calc(var(--u) * .8);
        min-height: calc(var(--u) * 2.8); padding: calc(var(--u) * .3) calc(var(--u) * .8);
        font-size: calc(var(--u) * 1.3);
    }
    .padkeys .kname { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
</style>
