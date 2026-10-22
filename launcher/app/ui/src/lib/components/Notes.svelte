<script>
    /* A release's notes (markdown.js), drawn as text, never as HTML. */
    import { tx } from '#lib/app.svelte.js';
    import { parseNotes } from '#lib/markdown.js';

    let { md } = $props();
    const blocks = $derived(parseNotes(md));
</script>

{#snippet spans(list)}
    {#each list as s}
        {#if s.kind === 'b'}<b>{s.text}</b>
        {:else if s.kind === 'code'}<code>{s.text}</code>
        {:else if s.kind === 'link'}<u>{s.text}</u>
        {:else if s.kind === 'em'}<i>{s.text}</i>
        {:else}{s.text}{/if}
    {/each}
{/snippet}

{#snippet items(list)}
    <ul>
        {#each list as it}
            <li>{@render spans(it.spans)}{#if it.sub.length}{@render items(it.sub)}{/if}</li>
        {/each}
    </ul>
{/snippet}

<div class="notes">
    {#each blocks as bl}
        {#if bl.kind === 'h'}<h4>{@render spans(bl.spans)}</h4>
        {:else if bl.kind === 'p'}<p>{@render spans(bl.spans)}</p>
        {:else}{@render items(bl.items)}{/if}
    {:else}
        {md === null ? '' : tx('updNoNotes')}
    {/each}
</div>

<style>
    .notes {
        flex: 1 0 auto; min-height: calc(var(--u) * 22); max-height: calc(var(--u) * 40); overflow: auto; user-select: text;
        padding: calc(var(--u) * .6) calc(var(--u) * 1.2); background: rgba(0, 0, 0, .55);
        border: 1px solid var(--line); font-size: calc(var(--u) * 1.4); line-height: 1.35;
    }
    h4 { margin: calc(var(--u) * .8) 0 calc(var(--u) * .3); color: var(--gold1); font-size: calc(var(--u) * 1.6); }
    p { margin: calc(var(--u) * .4) 0; }
    ul { margin: calc(var(--u) * .2) 0; padding-left: calc(var(--u) * 2.2); }
    code { font-family: var(--font-mono); color: var(--code); }
    u { text-decoration: none; color: var(--code); }
</style>
