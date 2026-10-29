<script>
    /* GRAPHICS: three pages. VIDEO, the window and what draws into it;
     * TEXTURES; INTERFACE, the shape of the screen and what is drawn over
     * the game. Every change writes umk3.ini at once. */
    import { app, tx, GFX_TABS, RES, ASPECT, AA, TEXQ, UISIZE, RENDERERS, save } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';
    import SharpSliders from '#lib/components/SharpSliders.svelte';

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
                <label class="lbl" for="res">{tx('res')}</label>
                <select id="res" class="sel" value={c.res >= 0 ? c.res : 1}
                        onchange={e => save({ res: e.currentTarget.selectedIndex })}>
                    {#each RES as r, i}<option value={i}>{label(r, i)}</option>{/each}
                </select>
            </div>
        </div>
        <h3>{tx('gImage')}</h3>
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
            <label class="chk wide"><input type="checkbox" checked={!!c.smoothAnims}
                onchange={e => save({ smoothAnims: e.currentTarget.checked })}><span>{tx('smooth')}</span></label>
            <label class="chk wide"><input type="checkbox" checked={!!c.rtShadows}
                onchange={e => save({ rtShadows: e.currentTarget.checked })}><span>{tx('rtshadow')}</span></label>
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
        </div>
        <p class="note">{tx('wsNote')}</p>
    {/if}
</Page>
