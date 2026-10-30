<script>
    /* The timeline under a stage: the frames 1..length, a playhead to drag
     * (scrubbing), and a row of keyframe diamonds a channel -- the element's
     * placement, then each shape key -- as Blender's dope sheet. A diamond on
     * the frame shown is lit; a linear key is drawn hollow, a bezier one
     * filled. A click or a drag anywhere on it seeks.
     *   rows: [{ label, keys: [{ f, i }] }]
     *   onSeek(frame) */
    let { length = 1, frame = 1, rows = [], onSeek } = $props();
    let area = $state(), drag = false;

    const at = f => (length > 1 ? (f - 1) / (length - 1) * 100 : 0);
    function frameAt(e) {
        const r = area.getBoundingClientRect();
        return Math.max(1, Math.min(length, Math.round(1 + (e.clientX - r.left) / Math.max(1, r.width) * (length - 1))));
    }
    function down(e) {
        if (e.button !== 0) return;
        drag = true;
        area.setPointerCapture(e.pointerId);
        onSeek(frameAt(e));
    }
    function move(e) { if (drag) onSeek(frameAt(e)); }
    function up() { drag = false; }
    const ticks = $derived.by(() => {
        const step = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000].find(s => length / s <= 12) || 1000;
        const out = [];
        for (let f = step; f <= length; f += step) out.push(f);
        return [1, ...out.filter(f => f !== 1)];
    });
</script>

<div class="tl">
    <div class="names">
        <span class="nm ruler">{frame}</span>
        {#each rows as r}<span class="nm" title={r.label}>{r.label}</span>{/each}
    </div>
    <div class="area" bind:this={area} role="slider" tabindex="-1" aria-valuemin="1" aria-valuemax={length} aria-valuenow={frame}
         onpointerdown={down} onpointermove={move} onpointerup={up} onpointercancel={up}>
        <div class="ruler">
            {#each ticks as f}<span class="tick" style:left="{at(f)}%">{f}</span>{/each}
        </div>
        {#each rows as r}
            <div class="row">
                {#each r.keys as k (k.f)}<span class="key" class:lin={k.i === 'linear'} class:hot={k.f === frame} style:left="{at(k.f)}%"></span>{/each}
            </div>
        {/each}
        <span class="head" style:left="{at(Math.min(frame, length))}%"></span>
    </div>
</div>

<style>
    .tl { display: flex; gap: calc(var(--u) * .6); background: var(--well); border-radius: var(--r-sm); padding: calc(var(--u) * .3) calc(var(--u) * .8) calc(var(--u) * .3) calc(var(--u) * .5); user-select: none; }
    .names { display: flex; flex-direction: column; flex: 0 0 calc(var(--u) * 11); min-width: 0; }
    .nm { height: calc(var(--u) * 1.8); line-height: calc(var(--u) * 1.8); font-size: calc(var(--t) * 1.05); color: var(--lav); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .nm.ruler { color: #ffc890; }
    .area { position: relative; flex: 1; min-width: 0; cursor: ew-resize; touch-action: none; }
    .ruler, .row { position: relative; height: calc(var(--u) * 1.8); border-bottom: 1px solid rgba(150, 140, 255, .12); }
    .tick { position: absolute; top: 0; transform: translateX(-50%); font-size: calc(var(--t) * .95); color: var(--dim); }
    .key { position: absolute; top: 50%; width: calc(var(--u) * .9); height: calc(var(--u) * .9); transform: translate(-50%, -50%) rotate(45deg);
        background: #e8e4ff; border: 1px solid #000; box-sizing: border-box; }
    .key.lin { background: transparent; border: 2px solid #e8e4ff; }
    .key.hot { background: #ffa040; border-color: #fff; }
    .key.lin.hot { background: transparent; border-color: #ffa040; }
    .head { position: absolute; top: 0; bottom: 0; width: 2px; margin-left: -1px; background: #ffa040; pointer-events: none; }
</style>
