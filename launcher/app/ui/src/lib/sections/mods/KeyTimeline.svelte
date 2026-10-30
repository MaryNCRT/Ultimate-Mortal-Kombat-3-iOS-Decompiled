<script>
    /* The timeline under a stage or an animation, as Blender's: the
     * transport (first frame, previous key, play, next key, last frame), the
     * frame shown, a ruler with a tick a frame (labels where they fit), a
     * dope sheet -- a Summary row with every key, then a row a channel --
     * and the playhead with its frame. A linear key is drawn hollow, a
     * bezier one filled, the ones on the frame shown lit. A click or a drag
     * on it seeks; the wheel over it steps a frame.
     *   rows: [{ label, keys: [{ f, i }], sub? (indented) }]
     *   onSeek(frame); onPlay() and playing, when it has a PLAY */
    let { length = 1, frame = 1, rows = [], onSeek, onPlay = null, playing = false } = $props();
    let area = $state(), width = $state(600), drag = false;

    const at = f => (length > 1 ? (f - 1) / (length - 1) * 100 : 0);
    const clamp = f => Math.max(1, Math.min(length, Math.round(f)));
    function frameAt(e) {
        const r = area.getBoundingClientRect();
        return clamp(1 + (e.clientX - r.left) / Math.max(1, r.width) * (length - 1));
    }
    function down(e) {
        if (e.button !== 0) return;
        e.preventDefault();
        drag = true;
        area.setPointerCapture(e.pointerId);
        onSeek(frameAt(e));
    }
    function move(e) { if (drag) onSeek(frameAt(e)); }
    function up() { drag = false; }
    function wheel(e) { e.preventDefault(); onSeek(clamp(frame + (e.deltaY > 0 ? 1 : -1))); }

    /* every key's frame, for the Summary row and the jumps */
    const all = $derived([...new Set(rows.flatMap(r => r.keys.map(k => k.f)))].sort((a, b) => a - b));
    const summary = $derived(all.map(f => ({ f, i: rows.some(r => r.keys.some(k => k.f === f && k.i !== 'linear')) ? 'bezier' : 'linear' })));
    const prevKey = () => { const k = [...all].reverse().find(f => f < frame); if (k) onSeek(k); };
    const nextKey = () => { const k = all.find(f => f > frame); if (k) onSeek(k); };

    /* the ruler: labels a step apart that fits the width, a tick a frame
     * when frames are 4 px or more apart, else a tick a fifth of a step */
    const ruler = $derived.by(() => {
        const px = width / Math.max(1, length - 1);
        const step = [1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000].find(s => s * px >= 46) || 1000;
        const minor = px >= 4 ? 1 : Math.max(1, step / 5);
        const labels = [], ticks = [];
        for (let f = 1; f <= length; f++) {
            if (f === 1 || f % step === 0) labels.push(f);
            else if (f % minor === 0) ticks.push(f);
        }
        return { labels, ticks };
    });
    $effect(() => {
        if (!area) return;
        const ro = new ResizeObserver(() => { width = area.clientWidth; });
        ro.observe(area);
        return () => ro.disconnect();
    });
</script>

<div class="tl">
    <div class="bar">
        <button class="tb" onclick={() => onSeek(1)} title="|◀">⏮</button>
        <button class="tb" onclick={prevKey} title="◆◀">◂◆</button>
        <button class="tb" onclick={() => onSeek(clamp(frame - 1))} title="◀">◂</button>
        {#if onPlay}<button class="tb play" class:hot={playing} onclick={onPlay}>{playing ? '❚❚' : '▶'}</button>{/if}
        <button class="tb" onclick={() => onSeek(clamp(frame + 1))} title="▶">▸</button>
        <button class="tb" onclick={nextKey} title="▶◆">◆▸</button>
        <button class="tb" onclick={() => onSeek(length)} title="▶|">⏭</button>
        <input class="cur" type="number" min="1" max={length} value={frame} onchange={e => onSeek(clamp(Number(e.currentTarget.value) || 1))}>
        <span class="range">1 – {length}</span>
        <span class="count">{all.length} ◆</span>
    </div>
    <div class="sheet">
        <div class="names">
            <span class="nm ruler"></span>
            <span class="nm sum">Summary</span>
            {#each rows as r}<span class="nm" class:sub={r.sub} title={r.label}>{r.label}</span>{/each}
        </div>
        <div class="area" bind:this={area} role="slider" tabindex="-1" aria-valuemin="1" aria-valuemax={length} aria-valuenow={frame}
             onpointerdown={down} onpointermove={move} onpointerup={up} onpointercancel={up} onwheel={wheel}>
            <div class="ruler">
                {#each ruler.ticks as f}<span class="tick" style:left="{at(f)}%"></span>{/each}
                {#each ruler.labels as f}<span class="tick big" style:left="{at(f)}%"></span><span class="lab" style:left="{at(f)}%">{f}</span>{/each}
            </div>
            <div class="row sum">
                {#each summary as k (k.f)}<span class="key" class:lin={k.i === 'linear'} class:hot={k.f === frame} style:left="{at(k.f)}%"></span>{/each}
            </div>
            {#each rows as r, j}
                <div class="row" class:odd={j % 2}>
                    {#each r.keys as k (k.f)}<span class="key" class:lin={k.i === 'linear'} class:hot={k.f === frame} style:left="{at(k.f)}%"></span>{/each}
                </div>
            {/each}
            <span class="head" style:left="{at(Math.min(frame, length))}%"><span class="badge">{frame}</span></span>
        </div>
    </div>
</div>

<style>
    .tl { display: flex; flex-direction: column; background: var(--well); border-radius: var(--r-sm); user-select: none; border: 1px solid var(--line); }
    .bar { display: flex; align-items: center; gap: calc(var(--u) * .3); padding: calc(var(--u) * .3) calc(var(--u) * .5); border-bottom: 1px solid var(--line); }
    .tb { background: rgba(150, 140, 255, .1); border: 1px solid var(--line); color: var(--lav); border-radius: 3px; cursor: pointer;
        min-width: calc(var(--u) * 2.6); height: calc(var(--u) * 2.2); font-size: calc(var(--t) * 1.05); padding: 0 calc(var(--u) * .3); }
    .tb:hover { color: #fff; background: rgba(150, 140, 255, .25); }
    .tb.hot { background: rgba(255, 160, 64, .35); color: #fff; }
    .cur { width: calc(var(--u) * 6); background: #1a1640; color: #ffc890; border: 1px solid var(--line); border-radius: 3px; padding: 0 4px; margin-left: calc(var(--u) * .5); text-align: center; }
    .range, .count { color: var(--dim); font-size: calc(var(--t) * 1); }
    .count { margin-left: auto; }
    .sheet { display: flex; padding: 0 calc(var(--u) * .9) calc(var(--u) * .3) 0; }
    .names { display: flex; flex-direction: column; flex: 0 0 calc(var(--u) * 12); min-width: 0; border-right: 1px solid var(--line); }
    .nm { height: calc(var(--u) * 1.9); line-height: calc(var(--u) * 1.9); padding-left: calc(var(--u) * .6); font-size: calc(var(--t) * 1.05); color: var(--lav); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .nm.sub { padding-left: calc(var(--u) * 1.6); color: var(--dim); }
    .nm.sum { color: #fff; background: rgba(150, 140, 255, .1); }
    .area { position: relative; flex: 1; min-width: 0; cursor: ew-resize; touch-action: none; margin-left: calc(var(--u) * .9); }
    .ruler, .row { position: relative; height: calc(var(--u) * 1.9); }
    .ruler { border-bottom: 1px solid var(--line); }
    .row { border-bottom: 1px solid rgba(150, 140, 255, .08); }
    .row.odd { background: rgba(255, 255, 255, .02); }
    .row.sum { background: rgba(150, 140, 255, .1); }
    .tick { position: absolute; bottom: 0; width: 1px; height: 25%; background: rgba(200, 195, 255, .25); }
    .tick.big { height: 45%; background: rgba(200, 195, 255, .55); }
    .lab { position: absolute; top: 0; transform: translateX(-50%); font-size: calc(var(--t) * .95); color: var(--dim); }
    .key { position: absolute; top: 50%; width: calc(var(--u) * .95); height: calc(var(--u) * .95); transform: translate(-50%, -50%) rotate(45deg);
        background: #e8e4ff; border: 1px solid #000; box-sizing: border-box; z-index: 1; }
    .key.lin { background: #1a1640; border: 2px solid #e8e4ff; }
    .key.hot { background: #ffa040; border-color: #fff; }
    .key.lin.hot { background: #1a1640; border-color: #ffa040; }
    .head { position: absolute; top: 0; bottom: 0; width: 2px; margin-left: -1px; background: #4a9cff; pointer-events: none; z-index: 2; }
    .badge { position: absolute; top: 0; left: 50%; transform: translateX(-50%); background: #4a9cff; color: #fff; border-radius: 3px;
        padding: 0 4px; font-size: calc(var(--t) * .95); line-height: calc(var(--u) * 1.7); white-space: nowrap; }
</style>
