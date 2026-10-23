<script>
    /* CHARACTER EDITOR: a fighter of the roster as the game's own tables
     * have it, changed into a mod.
     *
     *   ATTACKS  the basic strikes (strike_tables: each a hit box, the
     *            reaction, the damage and the chip damage on a block), and
     *            the special moves and finishers the move list names (seq_*):
     *            what each fighter has and what is empty -- Noob Saibot,
     *            Motaro and Shao Kahn have no specials of their own;
     *   DATA     the fighter's word in every per-fighter table of numbers;
     *   TEXTS    the game's strings (names, the move list), in a copy of
     *            the language file the mod carries;
     *   LOOKS    the fighter's textures -- outfits 1 and 2, portraits, the
     *            VERSUS card -- each replaced by a PNG.
     *
     * Numbers go to the mod's data.ini, which the game lays over its tables
     * at start (runtime/mods.c); the tables come from the player's own
     * binary, extracted by the build (build/logic_tables.c). */
    import { onMount } from 'svelte';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip, doneChord, failChord } from '#lib/sound.js';
    import { ms, src, loadCatalog, refreshMods, gameTables, forgetMod } from '#lib/mods/store.svelte.js';
    import { FIGHTERS, pngBytes } from '#lib/mods/assets.js';
    import { putTexture } from '#lib/mods/image.js';
    import HitboxEditor from './HitboxEditor.svelte';

    /* CharacterNames' order: the model file and the move list's prefix */
    const ROSTER = [
        ['KANO', 'kano'], ['SONYA', 'sonya'], ['JAX', 'jax'], ['NIGHTWOLF', 'nightwolf'], ['SUBZERO', 'subzero'],
        ['STRYKER', 'stryker'], ['SINDEL', 'sindel'], ['SEKTOR', 'sektor'], ['CYRAX', 'cyrax'], ['KUNGLAO', 'kunglao'],
        ['KABAL', 'kabal'], ['SHEEVA', 'sheeva'], ['SHANGTSUNG', 'shangtsung'], ['LIUKANG', 'liukang'], ['SMOKE', 'smoke'],
        ['KITANA', 'kitana'], ['JADE', 'jade'], ['MILEENA', 'mileena'], ['SCORPION', 'scorpion'], ['REPTILE', 'reptile'],
        ['ERMAC', 'ermac'], ['OLDSUBZERO', 'clasubzero'], ['OLDSMOKE', 'humsmoke'], ['NOOBSAIBOT', 'noob'],
        ['MOTARO', 'motaro'], ['SHAOKAHN', 'shaokahn'],
    ];
    const FINISHERS = ['fatality1', 'fatality2', 'animality', 'babality', 'friendship', 'stage'];
    const PAGES = [['attacks', 'edAttacks'], ['data', 'edData'], ['texts', 'edTexts'], ['looks', 'edLooks']];

    let tables = $state(null);
    let saved = $state({});                 /* the mod's data.ini */
    let edits = $state({});                 /* changed here, not saved yet */
    let note = $state('');
    let busy = $state(false);

    const c = $derived(ms.edFighter);
    const fid = $derived(ROSTER[c][0]);
    const fname = $derived(FIGHTERS[fid] || fid);

    onMount(async () => {
        await loadCatalog();
        await refreshMods();
        tables = await gameTables();
        if (!ms.edMod) ms.edMod = (ms.layer && ms.layer !== '*' ? ms.layer : '') || (ms.list[0] && ms.list[0].folder) || '';
        await loadMod();
    });

    async function loadMod() {
        saved = ms.edMod ? (await api.modsDataRead(ms.edMod)) || {} : {};
        edits = {};
        await loadTexts();
    }

    /* a word as it stands: changed here, else the mod's, else the game's */
    const word = (t, i) => {
        const k = t + '[' + i + ']';
        if (k in edits) return edits[k];
        if (k in saved) return saved[k];
        const v = tables && tables[t] ? tables[t][i] : undefined;
        return typeof v === 'number' ? v : undefined;
    };
    const signed = v => (v === undefined ? '' : v | 0);
    /* a box word: the game sign-extends it from 16 bits (strike_check_regs) */
    const s16 = v => (v === undefined ? '' : (v << 16) >> 16);
    function set(t, i, v) {
        const n = Number(v);
        if (!Number.isFinite(n)) return;
        edits = { ...edits, [t + '[' + i + ']']: n >>> 0 };
    }

    /* ---------------------------------------------------------- attacks -- */

    const strikeRow = $derived(tables && tables.strike_tables ? tables.strike_tables[c] : null);
    const strikes = $derived(strikeRow && tables[strikeRow] ? tables[strikeRow] : []);
    const sharedWith = $derived(tables && tables.strike_tables
        ? tables.strike_tables.map((r, i) => (r === strikeRow && i !== c ? FIGHTERS[ROSTER[i][0]] : null)).filter(Boolean) : []);
    const label = s => s.replace(/^stk_[a-z]+?_/, '').replace(/_/g, ' ').toUpperCase();
    const seqs = $derived(tables ? Object.keys(tables).filter(k => k.startsWith('seq_' + ROSTER[c][1] + '_')).map(k => k.slice(5 + ROSTER[c][1].length)) : []);
    let selStk = $state('');
    $effect(() => { if (!strikes.includes(selStk)) selStk = strikes.find(x => typeof x === 'string') || ''; });
    const boxes = $derived(strikes.filter(s => typeof s === 'string').map(name => ({ name, v: [0, 1, 2, 3].map(i => s16(word(name, i)) || 0) })));
    function boxTo(v) { v.forEach((x, i) => set(selStk, i, x)); }
    const specials = $derived(seqs.filter(s => !FINISHERS.includes(s)));

    function damage(stk, which, v) {
        const p = word(stk, 5) || 0;
        const n = Math.max(0, Math.min(255, Number(v) | 0));
        set(stk, 5, which === 'full' ? (p & ~0xff00) | (n << 8) : (p & ~0xff) | n);
    }

    /* ------------------------------------------------------------- data -- */

    /* every table with one number a fighter (24 to 26 words, no pointers),
     * which is where the per-fighter constants live */
    const perFighter = $derived(tables ? Object.keys(tables).filter(k => {
        const t = tables[k];
        return t.length >= 24 && t.length <= 26 && t.every(v => typeof v === 'number') && k !== 'strike_tables';
    }).sort() : []);

    /* ------------------------------------------------------------ texts -- */

    let langs = $state([]);
    let lang = $state('');
    let texts = $state([]);                 /* the strings as they stand */
    let textHead = null;
    let textEdits = $state({});
    let query = $state('');

    async function loadTexts() {
        langs = src.files.filter(f => /_text\.dat$/i.test(f)).sort();
        if (!lang) lang = langs.find(f => /^spanish/i.test(f)) || langs.find(f => /^english/i.test(f)) || langs[0] || '';
        textEdits = {};
        texts = [];
        if (!lang) return;
        const b = (ms.edMod && await api.resRead(lang, ms.edMod)) || await api.resRead(lang, '');
        if (!b) return;
        const d = new DataView(b.buffer, b.byteOffset, b.byteLength);
        const count = d.getInt16(6, true);
        textHead = b.slice(0, 8);
        const out = [];
        let o = 8;
        const dec = new TextDecoder('utf-16le');
        for (let i = 0; i < count && o + 2 <= b.length; i++) {
            const n = d.getUint16(o, true);
            out.push(dec.decode(b.subarray(o + 2, o + 2 + n)));
            o += 2 + n;
        }
        texts = out;
    }

    function textBytes() {
        const all = texts.map((t, i) => (i in textEdits ? textEdits[i] : t));
        const parts = all.map(t => {
            const u = new Uint8Array(t.length * 2);
            for (let i = 0; i < t.length; i++) { u[i * 2] = t.charCodeAt(i) & 0xff; u[i * 2 + 1] = t.charCodeAt(i) >> 8; }
            return u;
        });
        const out = new Uint8Array(8 + parts.reduce((s, p) => s + 2 + p.length, 0));
        out.set(textHead);
        const d = new DataView(out.buffer);
        /* LoadTextData allocates the file less this word for the strings,
         * which take the file less eight: never more than eight */
        d.setInt16(0, Math.min(8, d.getInt16(0, true)), true);
        let o = 8;
        for (const p of parts) { d.setUint16(o, p.length, true); out.set(p, o + 2); o += 2 + p.length; }
        return out;
    }

    const shownTexts = $derived.by(() => {
        const q = (query || fname).trim().toLowerCase();
        return texts.map((t, i) => [i, t]).filter(([i, t]) => !q || t.toLowerCase().includes(q) || String(i) === q).slice(0, 300);
    });

    /* ------------------------------------------------------------ looks -- */

    /* the smaller copies are listed when AUTO does not write them */
    const looks = $derived(src.files.filter(f => /^Textures\//i.test(f) && f.toUpperCase().includes(fid)
        && (!ms.autoSmall || !/_LOW\d*\.|_LITE\d*\./i.test(f))).map(f => f.slice(9).replace(/\.(pvr|png)$/i, '')).filter((v, i, a) => a.indexOf(v) === i));

    async function replaceLook(stem) {
        selectBlip();
        if (!(await ensureMod())) return;
        const got = await api.pickFile('picture');
        if (!got) return;
        const files = await putTexture(api, src, ms.edMod, stem, got.bytes, ms.autoSmall, 'Characters/' + fname);
        if (!files) { note = tx('vwNotPicture'); return; }
        forgetMod();
        note = tx('vwSaved') + ms.edMod + '\\' + files.join(', ');
        await refreshMods();
    }

    async function exportLook(stem) {
        selectBlip();
        const file = src.files.find(f => f.slice(9).replace(/\.(pvr|png)$/i, '') === stem);
        const png = file && await pngBytes(src, file);
        if (png) note = tx('exDone') + ' ' + await api.exportSave('Characters/' + fname + '/Textures', { [stem + '.png']: png });
    }

    function seeIt() {
        selectBlip();
        const it = ms.cat && ms.cat.characters.find(x => x.id === fid);
        if (!it) return;
        ms.kind = 'characters';
        ms.pick = it;
        if (ms.edMod) ms.layer = ms.edMod;
        ms.tab = 'view';
    }

    /* ------------------------------------------------------------- save -- */

    async function ensureMod() {
        const name = (ms.edMod || '').trim();
        if (!name) { note = tx('edNoMod'); return false; }
        if (!ms.list.some(m => m.folder === name)) {
            const r = await api.modsCreate({ name, category: 'mixed' });
            if (!r || !r.ok) { note = tx('edNoMod'); return false; }
            await refreshMods();
        }
        return true;
    }

    const changed = $derived(Object.keys(edits).length + Object.keys(textEdits).length);

    async function save() {
        if (busy || !changed) return;
        busy = true;
        try {
            if (!(await ensureMod())) return;
            if (Object.keys(edits).length) {
                saved = { ...saved, ...edits };
                await api.modsDataWrite(ms.edMod, saved);
                edits = {};
            }
            if (Object.keys(textEdits).length && textHead) {
                await api.modsPut(ms.edMod, lang.replace(/^.*\//, ''), textBytes());
                texts = texts.map((t, i) => (i in textEdits ? textEdits[i] : t));
                textEdits = {};
            }
            forgetMod();
            await refreshMods();
            doneChord();
            note = tx('edSaved') + ' ' + ms.edMod;
        } catch (e) {
            failChord();
            note = String(e && e.message || e);
        } finally {
            busy = false;
        }
    }

    async function resetWord(t, i) {
        const k = t + '[' + i + ']';
        const e = { ...edits };
        delete e[k];
        edits = e;
        if (k in saved) {
            const s = { ...saved };
            delete s[k];
            saved = s;
            await api.modsDataWrite(ms.edMod, saved);
        }
    }
</script>

<div class="ed">
    <aside class="side">
        <label class="field"><span class="lbl">{tx('edMod')}</span>
            <input class="edt" list="edmods" spellcheck="false" bind:value={ms.edMod} onchange={loadMod}>
            <datalist id="edmods">{#each ms.list as m}<option value={m.folder}></option>{/each}</datalist></label>
        <ul class="list">
            {#each ROSTER as [id], i}
                <li><button class:on={c === i} onpointerenter={hoverBlip}
                    onclick={() => { selectBlip(); ms.edFighter = i; query = ''; }}><span class="num">{i}</span>{FIGHTERS[id] || id}</button></li>
            {/each}
        </ul>
    </aside>

    <section class="main">
        <div class="top">
            <h3>{c} · {fname}</h3>
            <span class="tabs sub">
                {#each PAGES as [p, key]}
                    <button class="tab" class:active={ms.edPage === p} onclick={() => { selectBlip(); ms.edPage = p; }}>{tx(key)}</button>
                {/each}
            </span>
            <span class="grow"></span>
            <button class="btn" onclick={seeIt}>{tx('edSee')}</button>
            <button class="btn hot" disabled={!changed || busy} onclick={save}>{tx('edSave')}{changed ? ' (' + changed + ')' : ''}</button>
        </div>
        {#if note}<p class="note ok">{note}</p>{/if}

        {#if !tables || !tables.strike_tables}
            <p class="note">{tx('edNoTables')}</p>
        {:else if ms.edPage === 'attacks'}
            <div class="scroll">
                <h4>{tx('edStrikes')} <small>{strikeRow}{sharedWith.length ? ' · ' + tx('edShared') + ' ' + sharedWith.join(', ') : ''}</small></h4>
                {#if c >= 24}<p class="note">{tx('edBossNote')}</p>{/if}
                <div class="att">
                <div class="hb">
                    <div class="hbview"><HitboxEditor {boxes} sel={selStk} onchange={boxTo} /></div>
                    <p class="note">{selStk ? label(selStk) + ' · ' + tx('edHitNote') : tx('edHitPick')}</p>
                </div>
                <table class="grid">
                    <thead><tr><th>#</th><th>{tx('edName')}</th><th>{tx('edDamage')}</th><th>{tx('edChip')}</th><th>X</th><th>Y</th><th>{tx('edW')}</th><th>{tx('edH')}</th><th>{tx('edReact')}</th><th></th></tr></thead>
                    <tbody>
                        {#each strikes as stk, k}
                            {#if typeof stk === 'string'}
                                {@const p = word(stk, 5) || 0}
                                <tr class:mod={[0, 1, 2, 3, 4, 5].some(i => (stk + '[' + i + ']') in edits || (stk + '[' + i + ']') in saved)}
                                    class:sel={selStk === stk} onclick={() => selStk = stk}>
                                    <td class="num">{k}</td><td title={stk}>{label(stk)}</td>
                                    <td><input class="n" type="number" min="0" max="255" value={(p >> 8) & 255} onchange={e => damage(stk, 'full', e.currentTarget.value)}></td>
                                    <td><input class="n" type="number" min="0" max="255" value={p & 255} onchange={e => damage(stk, 'chip', e.currentTarget.value)}></td>
                                    {#each [0, 1, 2, 3] as i}
                                        <td><input class="n" type="number" value={s16(word(stk, i))} onchange={e => set(stk, i, e.currentTarget.value)}></td>
                                    {/each}
                                    <td><input class="n" type="number" value={signed(word(stk, 4))} onchange={e => set(stk, 4, e.currentTarget.value)}></td>
                                    <td><button class="x" title={tx('edReset')} onclick={() => [0, 1, 2, 3, 4, 5].forEach(i => resetWord(stk, i))}>↺</button></td>
                                </tr>
                            {:else}
                                <tr class="empty"><td class="num">{k}</td><td colspan="9">{tx('edEmpty')}</td></tr>
                            {/if}
                        {/each}
                    </tbody>
                </table>
                </div>
                <h4>{tx('edSpecials')}</h4>
                {#if specials.length}
                    <ul class="chips">{#each specials as s}<li>{s.toUpperCase()}</li>{/each}</ul>
                {:else}<p class="note bad">{tx('edNoSpecials')}</p>{/if}
                <h4>{tx('edFinishers')}</h4>
                <ul class="chips">
                    {#each FINISHERS as s}<li class:miss={!seqs.includes(s)}>{s.toUpperCase()}{seqs.includes(s) ? '' : ' · ' + tx('edEmpty')}</li>{/each}
                </ul>
                <p class="note">{tx('edAttacksNote')}</p>
            </div>
        {:else if ms.edPage === 'data'}
            <div class="scroll">
                <p class="note">{tx('edDataNote')}</p>
                <table class="grid">
                    <thead><tr><th>{tx('edTable')}</th><th>{tx('edValue')}</th><th>{tx('edGame')}</th><th></th></tr></thead>
                    <tbody>
                        {#each perFighter as t}
                            {#if c < tables[t].length}
                                <tr class:mod={(t + '[' + c + ']') in edits || (t + '[' + c + ']') in saved}>
                                    <td>{t}</td>
                                    <td><input class="n wide" type="number" value={signed(word(t, c))} onchange={e => set(t, c, e.currentTarget.value)}></td>
                                    <td class="dim">{signed(tables[t][c])}</td>
                                    <td><button class="x" title={tx('edReset')} onclick={() => resetWord(t, c)}>↺</button></td>
                                </tr>
                            {/if}
                        {/each}
                    </tbody>
                </table>
            </div>
        {:else if ms.edPage === 'texts'}
            <div class="row">
                <select class="sel" bind:value={lang} onchange={loadTexts}>
                    {#each langs as l}<option value={l}>{l}</option>{/each}
                </select>
                <input class="edt" placeholder={fname} spellcheck="false" bind:value={query}>
            </div>
            <p class="note">{tx('edTextsNote')}</p>
            <div class="scroll">
                <table class="grid">
                    <tbody>
                        {#each shownTexts as [i, t] (i)}
                            <tr class:mod={i in textEdits}><td class="num">{i}</td>
                                <td class="txt"><input class="edt" spellcheck="false" value={i in textEdits ? textEdits[i] : t}
                                    onchange={e => { textEdits = { ...textEdits, [i]: e.currentTarget.value }; }}></td></tr>
                        {/each}
                    </tbody>
                </table>
            </div>
        {:else}
            <p class="note">{tx('edLooksNote')}</p>
            <label class="chk"><input type="checkbox" bind:checked={ms.autoSmall}><span>{tx('vwAutoSmall')}</span></label>
            <div class="scroll">
                <table class="grid">
                    <tbody>
                        {#each looks as stem}
                            <tr><td>{stem}</td>
                                <td><button class="btn" onclick={() => replaceLook(stem)}>{tx('edReplacePng')}</button></td>
                                <td><button class="btn" onclick={() => exportLook(stem)}>{tx('edExportPng')}</button></td></tr>
                        {/each}
                    </tbody>
                </table>
                <p class="note">{tx('edSlotNote')}</p>
            </div>
        {/if}
        <p class="note dim">{tx('edOnline')}</p>
    </section>
</div>

<style>
    .ed { display: grid; grid-template-columns: calc(var(--u) * 24) minmax(0, 1fr); gap: calc(var(--u) * 1.4); flex: 1; min-height: calc(var(--u) * 40); }
    .side { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; }
    .field { display: flex; flex-direction: column; gap: calc(var(--u) * .3); }
    .list { list-style: none; margin: 0; padding: 0; overflow-y: auto; flex: 1; min-height: 0; background: var(--well); border-radius: var(--r-sm); }
    .list button {
        width: 100%; text-align: left; background: none; border: 0; cursor: pointer; display: flex; gap: calc(var(--u) * .7);
        padding: calc(var(--u) * .4) calc(var(--u) * .9); font-size: calc(var(--t) * 1.4); color: var(--lav);
    }
    .list button:hover { color: #fff; background: rgba(150, 140, 255, .12); }
    .list button.on { color: #fff; background: rgba(140, 125, 255, .3); }
    .num { color: var(--dim); min-width: calc(var(--u) * 2); font-variant-numeric: tabular-nums; }
    .main { display: flex; flex-direction: column; min-height: 0; gap: calc(var(--u) * .5); }
    .top { display: flex; align-items: center; gap: calc(var(--u) * 1); flex-wrap: wrap; }
    .top h3 { margin: 0; }
    .grow { flex: 1; }
    .row { display: flex; gap: calc(var(--u) * .8); }
    .row .sel { flex: 0 0 calc(var(--u) * 20); }
    .scroll { overflow-y: auto; flex: 1; min-height: 0; padding-right: calc(var(--u) * .4); }
    h4 { margin: calc(var(--u) * 1) 0 calc(var(--u) * .4); letter-spacing: .08em; }
    h4 small { color: var(--dim); font-weight: normal; }
    .grid { width: 100%; border-collapse: collapse; font-size: calc(var(--t) * 1.3); }
    .grid th { text-align: left; color: var(--dim); font-weight: normal; padding: calc(var(--u) * .2) calc(var(--u) * .4); }
    .grid td { padding: calc(var(--u) * .15) calc(var(--u) * .4); border-top: 1px solid var(--line); }
    .grid tr.mod td { background: rgba(120, 220, 160, .08); }
    .grid tr.empty td { color: #ff9aa0; }
    .grid tr.sel td { box-shadow: inset 0 0 0 9999px rgba(255, 90, 90, .1); }
    .grid tbody tr { cursor: pointer; }
    .att { display: flex; flex-direction: row-reverse; gap: calc(var(--u) * 1.2); align-items: flex-start; }
    .att .grid { flex: 1; }
    .hb { flex: 0 0 calc(var(--u) * 24); position: sticky; top: 0; }
    .att .grid td, .att .grid th { padding: calc(var(--u) * .1) calc(var(--u) * .2); }
    .att .grid th { font-size: calc(var(--t) * 1.05); }
    .hbview { height: calc(var(--u) * 30); }
    .n { width: calc(var(--u) * 4.6); background: var(--well); color: inherit; border: 1px solid var(--line); border-radius: 4px; padding: 2px 4px; font: inherit; }
    .n.wide { width: calc(var(--u) * 12); }
    .txt { width: 100%; }
    .txt .edt { width: 100%; }
    .dim { color: var(--dim); }
    .x { background: none; border: 0; color: var(--dim); cursor: pointer; font-size: calc(var(--t) * 1.4); }
    .x:hover { color: #fff; }
    .chips { list-style: none; display: flex; flex-wrap: wrap; gap: calc(var(--u) * .5); margin: 0; padding: 0; }
    .chips li { padding: calc(var(--u) * .25) calc(var(--u) * .7); border-radius: 999px; background: rgba(140, 125, 255, .2); font-size: calc(var(--t) * 1.2); }
    .chips li.miss { background: rgba(255, 110, 120, .15); color: #ff9aa0; }
    .ok { color: var(--ok-text); word-break: break-all; }
    .bad { color: #ff9aa0; }
</style>
