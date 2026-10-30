<script>
    /* The mod a change goes to: a name typed (a new mod), or one of the
     * installed ones from the list under ▾. A <datalist> shows only the
     * names that start with what is in the box, so with one picked it
     * showed that one alone; this list shows every mod, always. */
    import { ms } from '../../mods/store.svelte.js';
    let { value = $bindable(''), placeholder = '' } = $props();
    let open = $state(false), box = $state();

    function pick(f) { value = f; open = false; }
    $effect(() => {
        if (!open) return;
        const off = e => { if (box && !box.contains(e.target)) open = false; };
        window.addEventListener('pointerdown', off, true);
        return () => window.removeEventListener('pointerdown', off, true);
    });
</script>

<span class="mt" bind:this={box}>
    <input class="edt" spellcheck="false" bind:value {placeholder}>
    <button type="button" class="dd" class:on={open} onclick={() => (open = !open)} title="▾">▾</button>
    {#if open}
        <ul class="list">
            {#each ms.list as m (m.folder)}
                <li><button type="button" class:cur={m.folder === value} onclick={() => pick(m.folder)}>{m.folder}</button></li>
            {/each}
            {#if !ms.list.length}<li class="none">—</li>{/if}
        </ul>
    {/if}
</span>

<style>
    .mt { position: relative; display: flex; width: 100%; min-width: 0; }
    .mt .edt { flex: 1; min-width: 0; width: auto; border-top-right-radius: 0; border-bottom-right-radius: 0; }
    .dd { flex: none; width: calc(var(--u) * 2.4); background: rgba(150, 140, 255, .15); border: 1px solid var(--line); border-left: 0;
        color: var(--lav); cursor: pointer; border-radius: 0 3px 3px 0; }
    .dd:hover, .dd.on { background: rgba(150, 140, 255, .35); color: #fff; }
    .list { position: absolute; left: 0; right: 0; top: 100%; z-index: 20; margin: 2px 0 0; padding: 2px; list-style: none;
        max-height: calc(var(--u) * 20); overflow-y: auto; background: #14112e; border: 1px solid var(--line); border-radius: 3px; }
    .list button { display: block; width: 100%; text-align: left; background: none; border: 0; color: var(--lav);
        padding: calc(var(--u) * .3) calc(var(--u) * .5); cursor: pointer; font: inherit; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .list button:hover { background: rgba(150, 140, 255, .25); color: #fff; }
    .list button.cur { color: #ffc890; }
    .none { color: var(--dim); padding: calc(var(--u) * .3) calc(var(--u) * .5); }
</style>
