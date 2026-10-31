<script>
    /* FRIENDS: the account's friends (lib/account.js, the server's
     * /v1/friends). A player is found by the profile's name -- not the
     * login's -- or by the profile code, which belongs to the account. Adding
     * sends a request; it is a friendship once the other one accepts (or
     * asks back). Guests have none. Requests waiting show as a red dot on
     * PROFILE and FRIENDS, and new ones as a Windows notification (main.js,
     * pollFriends). */
    import { onMount } from 'svelte';
    import { app, tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';

    let st = $state(null);              /* {server, user, guest} */
    let fr = $state(null);              /* {code, name, friends, incoming, outgoing} */
    let q = $state('');
    let found = $state(null);           /* the last search's rows */
    let msg = $state('');
    let busy = $state(false);
    let armed = $state('');             /* the friend QUITAR was pressed on once */

    const err = e => {
        for (const k of ['frErr_' + e, 'acErr_' + e])
            if (tx(k) !== k) return tx(k);
        return tx('acErr_server_error');
    };

    async function refresh() {
        const r = await api.account.friends();
        if (r.ok) { fr = r; app.frIn = r.incoming.length; }
        else msg = err(r.error);
    }

    onMount(async () => {
        st = await api.account.status();
        if (st.user) await refresh();
    });

    async function run(job) {
        if (busy) return;
        selectBlip();
        busy = true;
        msg = '';
        try {
            await job();
        } finally {
            busy = false;
        }
    }

    const search = () => run(async () => {
        const r = await api.account.find(q.trim());
        if (!r.ok) { msg = err(r.error); return; }
        found = r.rows;
        if (!found.length) msg = tx('frNone');
    });

    const add = code => run(async () => {
        const r = await api.account.addFriend(code);
        if (!r.ok) { msg = err(r.error); return; }
        msg = tx(r.state === 'friends' ? 'frNowFriends' : 'frSent');
        await refresh();
    });

    function drop(code, ask) {
        if (ask && armed !== code) { selectBlip(); armed = code; return; }
        run(async () => {
            armed = '';
            const r = await api.account.removeFriend(code);
            if (!r.ok) { msg = err(r.error); return; }
            await refresh();
        });
    }

    /* what a search row can do, from the lists */
    const has = (list, code) => fr && fr[list].some(f => f.code === code);
</script>

<div class="pane">
    {#if st && !st.user}
        <p class="note">{tx('frGuest')}</p>
    {:else if fr}
        <div class="me">
            <span class="lbl">{tx('frYou')}</span>
            <span class="who">{fr.name || '—'}</span>
            <span class="idcode">{fr.code}</span>
        </div>
        <p class="note">{tx('frShare')}</p>

        <h3>{tx('frSearch')}</h3>
        <form class="row" onsubmit={e => { e.preventDefault(); search(); }}>
            <input class="edt grow" maxlength="24" spellcheck="false" placeholder={tx('frSearchHint')} bind:value={q}>
            <button class="btn hot" type="submit" disabled={busy || q.trim().length < 2}
                    onpointerenter={hoverBlip}>{tx('frFind')}</button>
        </form>
        {#if found && found.length}
            <ul class="list">
                {#each found as p (p.code)}
                    <li>
                        <span class="who">{p.name || '—'}</span><span class="idcode">{p.code}</span>
                        {#if has('friends', p.code)}
                            <span class="tag">{tx('frIsFriend')}</span>
                        {:else if has('outgoing', p.code)}
                            <span class="tag">{tx('frIsSent')}</span>
                        {:else}
                            <button class="btn" disabled={busy} onpointerenter={hoverBlip}
                                    onclick={() => add(p.code)}>{tx(has('incoming', p.code) ? 'frAccept' : 'frAdd')}</button>
                        {/if}
                    </li>
                {/each}
            </ul>
        {/if}
        {#if msg}<p class="msg">{msg}</p>{/if}

        {#if fr.incoming.length}
            <h3>{tx('frIncoming')}</h3>
            <ul class="list">
                {#each fr.incoming as p (p.code)}
                    <li>
                        <span class="who">{p.name || '—'}</span><span class="idcode">{p.code}</span>
                        <button class="btn hot" disabled={busy} onpointerenter={hoverBlip} onclick={() => add(p.code)}>{tx('frAccept')}</button>
                        <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={() => drop(p.code)}>{tx('frDecline')}</button>
                    </li>
                {/each}
            </ul>
        {/if}

        <h3>{tx('frList')} ({fr.friends.length})</h3>
        {#if fr.friends.length}
            <ul class="list">
                {#each fr.friends as p (p.code)}
                    <li>
                        <span class="who">{p.name || '—'}</span><span class="idcode">{p.code}</span>
                        <button class="btn" disabled={busy} onpointerenter={hoverBlip}
                                onclick={() => drop(p.code, true)}>{tx(armed === p.code ? 'frRemoveAsk' : 'frRemove')}</button>
                    </li>
                {/each}
            </ul>
        {:else}
            <p class="note">{tx('frEmpty')}</p>
        {/if}

        {#if fr.outgoing.length}
            <h3>{tx('frOutgoing')}</h3>
            <ul class="list">
                {#each fr.outgoing as p (p.code)}
                    <li>
                        <span class="who">{p.name || '—'}</span><span class="idcode">{p.code}</span>
                        <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={() => drop(p.code)}>{tx('frCancel')}</button>
                    </li>
                {/each}
            </ul>
        {/if}

        <div class="row"><button class="btn" disabled={busy} onpointerenter={hoverBlip}
                                 onclick={() => run(refresh)}>{tx('frRefresh')}</button></div>
    {:else if msg}
        <p class="msg">{msg}</p>
    {/if}
</div>

<style>
    .row { display: flex; gap: calc(var(--u) * 1); flex-wrap: wrap; margin: calc(var(--u) * .8) 0; align-items: center; }
    .grow { flex: 1; min-width: 0; max-width: calc(var(--u) * 40); }
    .me { display: flex; gap: calc(var(--u) * 1.4); align-items: baseline; flex-wrap: wrap; }
    .who { color: #fff; font-weight: bold; }
    .idcode { font-family: var(--font-mono); letter-spacing: .12em; color: var(--code); }
    .me .idcode { font-size: calc(var(--t) * 1.6); user-select: all; }
    .list { list-style: none; margin: 0; padding: 0; }
    .list li {
        display: flex; gap: calc(var(--u) * 1.2); align-items: center; flex-wrap: wrap;
        padding: calc(var(--u) * .4) 0; border-bottom: 1px solid rgba(255, 255, 255, .08);
    }
    .list .idcode { margin-right: auto; }
    .tag { color: var(--code); }
    .msg { color: #fff; }
    p { white-space: normal; overflow-wrap: anywhere; max-width: 100%; }
</style>
