<script>
    /* CHARACTER EDITOR: a fighter of the roster as the game's own tables
     * have it, changed into a mod.
     *
     *   ATTACKS  the basic strikes (strike_tables: each a hit box, the
     *            reaction, the damage and the chip damage on a block), and
     *            the special moves and finishers the move list names (seq_*):
     *            what each fighter has and what is empty -- Noob Saibot,
     *            Motaro and Shao Kahn have no specials of their own;
     *   KOMBOS   the kombo strings (ct_*): each hit's button and strike;
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
    const PAGES = [['attacks', 'edAttacks'], ['kombos', 'edKombos'], ['data', 'edData'], ['texts', 'edTexts'], ['looks', 'edLooks']];

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
    /* each strike once: Motaro's table names stk_dummy three times, and the
     * hit box drawing keys its boxes by name (Mary: the editor failed on
     * Motaro) */
    const boxes = $derived(strikes.filter((s, i) => typeof s === 'string' && strikes.indexOf(s) === i).map(name => ({ name, v: [0, 1, 2, 3].map(i => s16(word(name, i)) || 0) })));
    function boxTo(v) { v.forEach((x, i) => set(selStk, i, x)); }
    const specials = $derived(seqs.filter(s => !FINISHERS.includes(s)));

    function damage(stk, which, v) {
        const p = word(stk, 5) || 0;
        const n = Math.max(0, Math.min(255, Number(v) | 0));
        set(stk, 5, which === 'full' ? (p & ~0xff00) | (n << 8) : (p & ~0xff) | n);
    }

    /* ----------------------------------------------------------- kombos -- */

    /* The kombo strings (mkcombo.c): ochar_elbow_combos / ochar_knee_combos
     * name a fighter's first list after a punch or a kick starter. A list is
     * 0x18-byte entries (six words), the last with bit 15 of word 0; in each,
     * word 2's low byte is the button (switch id: _swtab maps HP 0, LP 1,
     * BL 2, HK 3, LK 4), word 4's bits 8+ the strike (combo_strike_table),
     * word 5 the next list, or a number when the string ends there. */
    const BUTTONS = ['HP', 'LP', 'BL', 'HK', 'LK'];
    const STARTS = [['ochar_elbow_combos', 'HP'], ['ochar_knee_combos', 'HK']];
    const ref = r => {
        const m = /^(\w+?)(?:\+0x([0-9a-f]+))?$/.exec(r);
        return m ? [m[1], m[2] ? parseInt(m[2], 16) / 4 : 0] : null;
    };
    const kombos = $derived.by(() => {
        if (!tables) return [];
        const out = [];
        /* the tables in address order (logic_tables.c's): a list runs on
         * into the next symbol until bit 15 (ct_swat_knee2's second entry is
         * ct_swat_elbow2) */
        const order = Object.keys(tables);
        const walk = (r, hits, seen) => {
            const at = ref(r);
            if (!at || !tables[at[0]] || seen.has(r) || hits.length > 8) return;
            const s = new Set(seen).add(r);
            const keys = [];
            let [name, i] = at;
            for (let guard = 0; guard < 16; guard++, i += 6) {
                while (tables[name] && i + 5 >= tables[name].length) {
                    i -= tables[name].length;
                    name = order[order.indexOf(name) + 1];
                }
                const t = tables[name];
                if (!t || i < 0) break;
                const w0 = typeof t[i] === 'number' ? t[i] : 0;
                const k = word(name, i + 2) & 0xff;
                /* the port's rule (combo_scan_a11): a second entry with
                 * the same button is taken with up held */
                const hit = { t: name, i, k, up: keys.includes(k), stk: (word(name, i + 4) >>> 8) & 0xff };
                keys.push(k);
                const next = t[i + 5];
                if (typeof next === 'string') walk(next, [...hits, hit], s);
                else out.push([...hits, hit]);
                if (w0 & 0x8000) break;
            }
        };
        for (const [tab, start] of STARTS) {
            const first = tables[tab] && tables[tab][c];
            if (typeof first === 'string') walk(first, [{ start }], new Set());
        }
        return out;
    });
    const komboStrikes = $derived(tables && tables.combo_strike_table ? tables.combo_strike_table : []);
    const hitName = h => (h.start ? h.start : (h.up ? 'U+' : '') + (BUTTONS[h.k] || '#' + h.k));
    const setByte = (t, i, shift, v) => set(t, i, ((word(t, i) || 0) & ~(0xff << shift)) | ((Number(v) & 0xff) << shift));

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
        const files = await putTexture(api, src, ms.edMod, stem, got.bytes, ms.autoSmall, 'Characters/' + fname, ms.autoIce);
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
        {:else if ms.edPage === 'kombos'}
            <div class="scroll">
                <p class="note">{tx('edKomboNote')}</p>
                {#if !kombos.length}<p class="note bad">{tx('edNoKombos')}</p>{/if}
                {#each kombos as hits, n}
                    <h4>{n + 1} · {hits.map(hitName).join(', ')}</h4>
                    <table class="grid">
                        <thead><tr><th>#</th><th>{tx('edKomboButton')}</th><th>{tx('edKomboStrike')}</th><th>{tx('edDamage')}</th><th></th></tr></thead>
                        <tbody>
                            {#each hits as h, j}
                                {#if h.start}
                                    <tr class="dim"><td class="num">{j + 1}</td><td>{h.start}</td><td colspan="3">{tx('edKomboStart')}</td></tr>
                                {:else}
                                    {@const stk = komboStrikes[h.stk]}
                                    <tr class:mod={[2, 4].some(i => (h.t + '[' + (h.i + i) + ']') in edits || (h.t + '[' + (h.i + i) + ']') in saved)}>
                                        <td class="num">{j + 1}</td>
                                        <td>{h.up ? 'U+ ' : ''}<select class="sel" value={h.k} onchange={e => setByte(h.t, h.i + 2, 0, e.currentTarget.value)}>
                                            {#each BUTTONS as b, i}<option value={i}>{b}</option>{/each}</select></td>
                                        <td><select class="sel" value={h.stk} onchange={e => setByte(h.t, h.i + 4, 8, e.currentTarget.value)}>
                                            {#each komboStrikes as s, i}<option value={i}>{i} · {typeof s === 'string' ? s.replace(/^stk_/, '').toUpperCase() : '?'}</option>{/each}</select></td>
                                        <td class="dim">{typeof stk === 'string' ? ((word(stk, 5) || 0) >> 8) & 255 : ''}</td>
                                        <td><button class="x" title={tx('edReset')} onclick={() => [2, 4].forEach(i => resetWord(h.t, h.i + i))}>↺</button></td>
                                    </tr>
                                {/if}
                            {/each}
                        </tbody>
                    </table>
                {/each}
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
            <label class="chk"><input type="checkbox" bind:checked={ms.autoIce}><span>{tx('vwAutoIce')}</span></label>
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
    .sel { background: var(--well); color: inherit; border: 1px solid var(--line); border-radius: 4px; font: inherit; }
    .x { background: none; border: 0; color: var(--dim); cursor: pointer; font-size: calc(var(--t) * 1.4); }
    .x:hover { color: #fff; }
    .chips { list-style: none; display: flex; flex-wrap: wrap; gap: calc(var(--u) * .5); margin: 0; padding: 0; }
    .chips li { padding: calc(var(--u) * .25) calc(var(--u) * .7); border-radius: 999px; background: rgba(140, 125, 255, .2); font-size: calc(var(--t) * 1.2); }
    .chips li.miss { background: rgba(255, 110, 120, .15); color: #ff9aa0; }
    .ok { color: var(--ok-text); word-break: break-all; }
    .bad { color: #ff9aa0; }
</style>
