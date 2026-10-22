<script>
    /* MODS: three pages -- the MOD MANAGER, the MODEL VIEWER and EXPORT
     * (sections/mods/). */
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';
    import { ms, MODS_TABS } from '#lib/mods/store.svelte.js';
    import ModManager from './mods/ModManager.svelte';
    import ModViewer from './mods/ModViewer.svelte';
    import ModExporter from './mods/ModExporter.svelte';
</script>

<Page>
    <SubNav tabs={MODS_TABS} current={ms.tab} pick={v => ms.tab = v} />
    {#if ms.tab === 'mgr'}
        <ModManager />
    {:else if ms.tab === 'view'}
        <ModViewer />
    {:else}
        <ModExporter />
    {/if}
</Page>
