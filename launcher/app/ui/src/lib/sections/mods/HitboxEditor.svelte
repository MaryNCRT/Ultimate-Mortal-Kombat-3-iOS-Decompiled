<script>
    /* A strike's hit box, drawn and dragged. The game's box (strike_check_regs)
     * is measured from the fighter's position, facing right:
     *
     *      left = x + reach - width     top = y + top
     *      right = x + reach            bottom = y + top + height
     *
     * so `reach` (word 0) is how far forward the front edge goes, `width`
     * (word 2) runs back from it, `top` (word 1) and `height` (word 3) down
     * from the fighter's y. The fighter is drawn as the span every one of his
     * strikes covers, and his other strikes faintly behind. Dragging the box
     * moves it; its corner handle sizes it. */
    let { boxes, sel, onchange } = $props();

    const pad = 30;
    /* the view: every strike's box, and the fighter's own x = 0 */
    const view = $derived.by(() => {
        let x0 = -40, x1 = 40, y0 = 0, y1 = 100;
        for (const b of boxes) {
            x0 = Math.min(x0, b.v[0] - b.v[2]); x1 = Math.max(x1, b.v[0]);
            y0 = Math.min(y0, b.v[1]); y1 = Math.max(y1, b.v[1] + b.v[3]);
        }
        return { x: x0 - pad, y: y0 - pad, w: x1 - x0 + pad * 2, h: y1 - y0 + pad * 2, body: y1 };
    });
    const cur = $derived(boxes.find(b => b.name === sel));

    let svg = $state();
    let drag = null;
    function at(e) {
        const p = svg.createSVGPoint();
        p.x = e.clientX; p.y = e.clientY;
        return p.matrixTransform(svg.getScreenCTM().inverse());
    }
    function down(e, mode) {
        if (!cur) return;
        e.preventDefault();
        e.currentTarget.setPointerCapture(e.pointerId);
        const p = at(e);
        drag = { mode, x: p.x, y: p.y, v: cur.v.slice() };
    }
    function move(e) {
        if (!drag) return;
        const p = at(e);
        const dx = Math.round(p.x - drag.x), dy = Math.round(p.y - drag.y);
        const v = drag.v.slice();
        if (drag.mode === 'move') { v[0] += dx; v[1] += dy; } else { v[0] += dx; v[2] = Math.max(1, v[2] + dx); v[3] = Math.max(1, v[3] + dy); }
        onchange(v);
    }
    const up = () => { drag = null; };
</script>

<svg bind:this={svg} viewBox="{view.x} {view.y} {view.w} {view.h}" onpointermove={move} onpointerup={up} role="application">
    <!-- the fighter: his x, and the height his strikes span -->
    <rect class="body" x="-22" y="0" width="44" height={view.body} rx="10"></rect>
    <line class="axis" x1="0" y1={view.y} x2="0" y2={view.y + view.h}></line>
    <text class="tag" x="-20" y="-6">▶</text>
    {#each boxes as b (b.name)}
        {#if b.name !== sel}
            <rect class="other" x={b.v[0] - b.v[2]} y={b.v[1]} width={b.v[2]} height={b.v[3]}></rect>
        {/if}
    {/each}
    {#if cur}
        <rect class="hit" x={cur.v[0] - cur.v[2]} y={cur.v[1]} width={cur.v[2]} height={cur.v[3]}
              onpointerdown={e => down(e, 'move')} role="button" tabindex="-1"></rect>
        <rect class="grip" x={cur.v[0] - 4} y={cur.v[1] + cur.v[3] - 4} width="8" height="8"
              onpointerdown={e => down(e, 'size')} role="button" tabindex="-1"></rect>
    {/if}
</svg>

<style>
    svg { width: 100%; height: 100%; background: radial-gradient(ellipse at 50% 40%, #241f55, #0a0820 80%); border-radius: var(--r-sm); border: 1px solid var(--line); touch-action: none; }
    rect, line { vector-effect: non-scaling-stroke; }
    .body { fill: rgba(150, 140, 255, .22); stroke: rgba(190, 180, 255, .7); stroke-width: 1.5; }
    .axis { stroke: rgba(255, 255, 255, .3); stroke-dasharray: 4 4; }
    .tag { fill: rgba(255, 255, 255, .5); font-size: 8px; }
    .other { fill: rgba(255, 140, 140, .05); stroke: rgba(255, 150, 150, .45); stroke-width: 1; }
    .hit { fill: rgba(255, 70, 70, .4); stroke: #ff5a5a; stroke-width: 2; cursor: move; }
    .grip { fill: #fff; cursor: nwse-resize; }
</style>
