<script>
    /* GRAPHICS: four pages. VIDEO, the window; GRAPHICS, what draws into
     * it; TEXTURES; INTERFACE, the shape of the screen and what is drawn over
     * the game. Every change writes umk3.ini at once. */
    import { app, tx, GFX_TABS, RES, ASPECT, AA, TEXQ, UISIZE, RENDERERS, save } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';
    import SharpSliders from '#lib/components/SharpSliders.svelte';
    import AudioSliders from '#lib/components/AudioSliders.svelte';
    import SecControls from '#lib/sections/SecControls.svelte';

    const c = $derived(app.cfg);
    const label = (r, i) => r.w + ' × ' + r.h + (i === 0 ? '  ' + tx('original') : '');
    const idx = (list, v, d) => Math.max(0, list.indexOf(v === undefined || v === null || v === '' ? d : v));

    /* the monitors the game reports, and one chosen that is gone */
    const monitors = $derived.by(() => {
        const out = [{ value: '0', name: tx('monitorAuto') }];
        for (const m of app.monitors)
            out.push({ value: String(m.value), name: tx('monitorLbl') + ' ' + m.name.replace(' *', '  (' + tx('monitorMain') + ')') });
        if (c.monitor && !app.monitors.some(m => m.value === c.monitor))
            out.push({ value: String(c.monitor), name: tx('monitorLbl') + ' ' + c.monitor });
        return out;
    });

    /* the GPUs and sound outputs the game reports, and one chosen that is gone */
    const named = (list, cur, auto) => {
        const out = [{ value: 'auto', name: auto }];
        for (const n of list) out.push({ value: n, name: n });
        if (cur && cur !== 'auto' && !list.includes(cur)) out.push({ value: cur, name: cur });
        return out;
    };
    const gpus = $derived(named(app.gpus, c.gpu, tx('gpuAuto')));
    const audios = $derived(named(app.audios, c.audioDevice, tx('audioAuto')));

    /* GRAPHICS QUALITY, as lib/ini.js graphicsPreset / setGraphicsPreset
     * (and the game's gfx_preset.c): the preset is what the switches match */
    const BLOOMS = [0, 15, 30, 50];
    const GQ = {
        original: { rtShadows: false, ao: 0, contactShadows: false, bloom: 0, tonemap: false },
        enhanced: { rtShadows: true, ao: 2, contactShadows: true, bloom: 30, tonemap: true },
    };
    const gq = $derived(!c.rtShadows && !c.bloom && !c.tonemap ? 'original'
        : c.rtShadows && c.ao && c.contactShadows && c.bloom === 30 && c.tonemap ? 'enhanced' : 'custom');
    const bloomIdx = $derived(BLOOMS.reduce((b, v, i) => Math.abs(v - (c.bloom || 0)) < Math.abs(BLOOMS[b] - (c.bloom || 0)) ? i : b, 0));

    /* DLSS 5 (lib/dlss5.js, the game's neural_mod.c): what the game says
     * stands in the way, asked when the GRAPHICS page opens and after an
     * import; the player's NVIDIA files and consumer add-on imported */
    let dl = $state(null);
    let dlMsgs = $state([]);
    async function dlRefresh() {
        try { dl = (await api.dlss5Status?.()) || null; } catch { dl = null; }
    }
    $effect(() => { if (app.gfxTab === 'gfx') dlRefresh(); });
    async function dlImport() {
        selectBlip();
        const r = await api.dlss5Import?.();
        if (r && !r.canceled) {
            dlMsgs = r.results;
            await dlRefresh();
        }
    }

    async function frame() {
        const p = await api.browseFrame();
        if (p) await save({ marco: p });
    }
</script>

<Page>
    <SubNav tabs={GFX_TABS} current={app.gfxTab} pick={v => app.gfxTab = v} />

    {#if app.gfxTab === 'video'}
        <h3 class="first">{tx('gScreen')}</h3>
        <div class="fields">
            <div class="field">
                <label class="lbl" for="rend">{tx('rend')}</label>
                <select id="rend" class="sel" value={idx(RENDERERS, c.renderer, 'opengl')}
                        onchange={e => save({ renderer: RENDERERS[e.currentTarget.selectedIndex] })}>
                    {#each RENDERERS as r}<option value={RENDERERS.indexOf(r)}>{tx('rends')[r]}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="mode">{tx('mode')}</label>
                <select id="mode" class="sel" value={c.fullscreen ? 1 : c.borderless ? 2 : 0}
                        onchange={e => { const m = e.currentTarget.selectedIndex; save({ fullscreen: m === 1, borderless: m === 2 }); }}>
                    {#each tx('modes') as m, i}<option value={i}>{m}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="monitor">{tx('monitorLbl')}</label>
                <select id="monitor" class="sel" value={String(c.monitor || 0)}
                        onchange={e => { selectBlip(); save({ monitor: Number(e.currentTarget.value) || 0 }); }}>
                    {#each monitors as m}<option value={m.value}>{m.name}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="gpu">{tx('gpuLbl')}</label>
                <select id="gpu" class="sel" value={c.gpu || 'auto'} title={c.renderer === 'opengl' || !c.renderer ? tx('gpuGl') : ''}
                        onchange={e => { selectBlip(); save({ gpu: e.currentTarget.value }); }}>
                    {#each gpus as g}<option value={g.value}>{g.name}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="res">{tx('res')}</label>
                <select id="res" class="sel" value={c.res >= 0 ? c.res : 1}
                        onchange={e => save({ res: e.currentTarget.selectedIndex })}>
                    {#each RES as r, i}<option value={i}>{label(r, i)}</option>{/each}
                </select>
            </div>
        </div>
    {:else if app.gfxTab === 'audio'}
        <h3 class="first">{tx('hGAudio')}</h3>
        <AudioSliders />
        <div class="fields">
            <div class="field">
                <label class="lbl" for="audioDev">{tx('audioLbl')}</label>
                <select id="audioDev" class="sel" value={c.audioDevice || 'auto'}
                        onchange={e => { selectBlip(); save({ audioDevice: e.currentTarget.value }); }}>
                    {#each audios as a}<option value={a.value}>{a.name}</option>{/each}
                </select>
            </div>
        </div>
        <p class="note">{tx('audioNote')}</p>
    {:else if app.gfxTab === 'gfx'}
        <h3 class="first">{tx('gImage')}</h3>
        <div class="fields">
            <div class="field">
                <label class="lbl" for="rres">{tx('rres')}</label>
                <select id="rres" class="sel" value={c.rres > 0 ? c.rres : 0}
                        onchange={e => save({ rres: e.currentTarget.selectedIndex })}>
                    <option value={0}>{tx('sameRes')}</option>
                    {#each RES as r, i}<option value={i + 1}>{label(r, i)}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="gq">{tx('gq')}</label>
                <select id="gq" class="sel" value={gq}
                        onchange={e => { selectBlip(); const v = e.currentTarget.value; if (GQ[v]) save(GQ[v]); }}>
                    <option value="original">{tx('gqs')[0]}</option>
                    <option value="enhanced">{tx('gqs')[1]}</option>
                    {#if gq === 'custom'}<option value="custom" disabled>{tx('gqs')[2]}</option>{/if}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="bloom">{tx('bloom')}</label>
                <select id="bloom" class="sel" value={bloomIdx}
                        onchange={e => save({ bloom: BLOOMS[e.currentTarget.selectedIndex] })}>
                    {#each tx('blooms') as b, i}<option value={i}>{b}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="aa">{tx('aa')}</label>
                <select id="aa" class="sel" value={idx(AA, c.antialiasing, 0)}
                        onchange={e => save({ antialiasing: AA[e.currentTarget.selectedIndex] })}>
                    {#each AA as n, i}<option value={i}>{n ? 'x' + n : tx('aaOff')}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="smodel">{tx('smodel')}</label>
                <select id="smodel" class="sel" value={c.smoothModels || 0}
                        onchange={e => save({ smoothModels: e.currentTarget.selectedIndex })}>
                    {#each tx('smodels') as m, i}<option value={i}>{m}</option>{/each}
                </select>
            </div>
            <div class="field wide">
                <SharpSliders disabled={!c.smoothModels} />
            </div>
            <label class="chk wide"><input type="checkbox" checked={c.vsync !== false}
                onchange={e => save({ vsync: e.currentTarget.checked })}><span>{tx('vsync')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={c.lowLatency !== false}
                onchange={e => save({ lowLatency: e.currentTarget.checked })}><span>{tx('lowLatency')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={!!c.smoothAnims}
                onchange={e => save({ smoothAnims: e.currentTarget.checked })}><span>{tx('smooth')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={!!c.rtShadows}
                onchange={e => save({ rtShadows: e.currentTarget.checked })}><span>{tx('rtshadow')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={(c.ao ?? 2) !== 0} disabled={!c.rtShadows}
                onchange={e => save({ ao: e.currentTarget.checked ? 2 : 0 })}><span>{tx('ao')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={!!c.contactShadows} disabled={!c.rtShadows || !(c.ao ?? 2)}
                onchange={e => save({ contactShadows: e.currentTarget.checked })}><span>{tx('contact')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={!!c.tonemap}
                onchange={e => save({ tonemap: e.currentTarget.checked })}><span>{tx('tonemap')}</span></label>
            <div class="field wide">
                <span class="lbl">{tx('frame')}</span>
                <div class="row">
                    <input class="edt" readonly spellcheck="false" value={c.marco || ''}>
                    <button class="btn" onpointerenter={hoverBlip} onclick={frame}>{tx('browse')}</button>
                    <button class="btn" onpointerenter={hoverBlip} onclick={() => save({ marco: '' })}>{tx('clear')}</button>
                </div>
            </div>
        </div>
        <p class="note">{tx('rendNote')}</p>

        <h3>DLSS 5</h3>
        <div class="fields">
            <label class="chk wide"><input type="checkbox" checked={!!c.dlss5}
                disabled={dl && (dl.status === 'need_nvidia_gpu' || dl.status === 'not_installed')}
                onchange={e => save({ dlss5: e.currentTarget.checked })}><span>{tx('dlss5On')}</span></label>
            <div class="field wide">
                <span class="lbl">{tx('dlss5State')}</span>
                <div class="row">
                    <input class="edt" readonly value={dl ? (tx('dlss5St')[dl.status] || dl.text) : '...'}>
                    <button class="btn" onpointerenter={hoverBlip} onclick={dlImport}>{tx('dlss5Add')}</button>
                    <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); api.dlss5Open?.(); }}>{tx('dlss5Folder')}</button>
                </div>
            </div>
        </div>
        {#if dl && dl.files}
            <p class="note">nvngx_dlss.dll: {dl.files.dlss ? '✓' : '✗'} &nbsp; nvngx_dlssnr.dll: {dl.files.dlssnr ? '✓' : '✗'} &nbsp; {tx('dlss5Consumer')}: {dl.files.consumer || '✗'}</p>
        {/if}
        {#each dlMsgs as m}
            <p class="note">{m.ok ? '✓' : '✗'} {m.name}: {m.why}</p>
        {/each}
        <p class="note">{tx('dlss5Note')}</p>
    {:else if app.gfxTab === 'tex'}
        <div class="fields">
            <div class="field">
                <label class="lbl" for="texq">{tx('texq')}</label>
                <select id="texq" class="sel" value={idx(TEXQ, c.textureQuality, 'iphone')}
                        onchange={e => save({ textureQuality: TEXQ[e.currentTarget.selectedIndex] })}>
                    {#each tx('texqs') as n, i}<option value={i}>{n}</option>{/each}
                </select>
            </div>
        </div>
        <p class="note">{tx('texNote')}</p>
    {:else if app.gfxTab === 'ctl'}
        <SecControls embedded />
    {:else}
        <div class="fields">
            <div class="field">
                <label class="lbl" for="aspect">{tx('aspect')}</label>
                <select id="aspect" class="sel" value={idx(ASPECT, c.aspect, '3:2')}
                        onchange={e => save({ aspect: ASPECT[e.currentTarget.selectedIndex] })}>
                    {#each tx('aspects') as a, i}<option value={i}>{a}</option>{/each}
                </select>
            </div>
            <div class="field">
                <label class="lbl" for="uisize">{tx('uisize')}</label>
                <select id="uisize" class="sel" value={idx(UISIZE, c.uiSize, 'iphone')}
                        onchange={e => save({ uiSize: UISIZE[e.currentTarget.selectedIndex] })}>
                    {#each tx('uisizes') as n, i}<option value={i}>{n}</option>{/each}
                </select>
            </div>
            <label class="chk wide"><input type="checkbox" checked={!!c.hideControls}
                onchange={e => save({ hideControls: e.currentTarget.checked })}><span>{tx('hidectl')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={c.cornerButtons !== false}
                onchange={e => save({ cornerButtons: e.currentTarget.checked })}><span>{tx('corner')}</span></label>
        </div>
        <p class="note">{tx('wsNote')}</p>
    {/if}
</Page>
