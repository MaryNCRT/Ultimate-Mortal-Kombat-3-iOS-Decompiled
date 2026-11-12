<script>
    /* FRIENDS: the account's friends (lib/account.js, the server's
     * /v1/friends). A player is found by the profile's name -- not the
     * login's -- or by the profile code, which belongs to the account. Adding
     * sends a request; it is a friendship once the other one accepts (or
     * asks back). Guests have none. Requests waiting show as a red dot on
     * PROFILE and FRIENDS, and new ones as a Windows notification (main.js,
     * pollFriends). */
    import { onMount, untrack } from 'svelte';
    import { app, tx, rebuildAfterUpdate } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';
    import FriendBanner from '#lib/components/FriendBanner.svelte';

    let st = $state(null);              /* {server, user, guest} */
    let fr = $state(null);              /* {code, name, friends, incoming, outgoing} */
    let q = $state('');
    let found = $state(null);           /* the last search's rows */
    let msg = $state('');
    let busy = $state(false);
    let adding = $state(false);         /* ADD FRIEND's panel open */
    let armed = $state('');             /* the friend QUITAR was pressed on once */

    const err = e => {
        for (const k of ['frErr_' + e, 'acErr_' + e])
            if (tx(k) !== k) return tx(k);
        return tx('acErr_server_error');
    };

    async function refresh() {
        const r = await api.account.friends();
        if (r.ok) { fr = r; app.frIn = r.incoming.length + (r.invites || []).length; }
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

    /* the lists main.js keeps asking for: an invite shows as it comes */
    $effect(() => {
        const live = app.frLive;
        if (live) untrack(() => { if (fr) fr = live; });
    });

    const invite = code => run(async () => {
        const r = await api.account.invite(code);
        msg = r.ok ? tx('frInvited') : err(r.error);
    });
    const join = code => run(async () => {
        const r = await api.account.joinInvite(code);
        if (r.rebuild) { rebuildAfterUpdate(); return; }    /* an update to compile first */
        if (!r.ok) { msg = err(r.error); return; }
        msg = tx(r.running ? 'frJoinRunning' : 'frJoining');
        await refresh();
    });
    const dropInv = code => run(async () => {
        await api.account.dropInvite(code);
        await refresh();
    });

    /* what a search row can do, from the lists */
    const has = (list, code) => fr && fr[list].some(f => f.code === code);
</script>

<div class="pane">
    {#if st && !st.user}
        <p class="note">{tx('frGuest')}</p>
    {:else if fr}
        <!-- as the game's ONLINE > FRIENDS (Mary, 0.4.4): the heading bar with
             the count and ADD FRIEND, then each player a banner -->
        <div class="bar">
            <h3>{tx('frList')} ({fr.friends.length})</h3>
            <button class="fbtn" onpointerenter={hoverBlip}
                    onclick={() => { selectBlip(); adding = !adding; }}>{tx(adding ? 'frAddClose' : 'frAddFriend')}</button>
        </div>

        {#if adding}
            <div class="add">
                <div class="me">
                    <span class="lbl">{tx('frYou')}</span>
                    <span class="who">{fr.name || '—'}</span>
                    <span class="idcode">{fr.code}</span>
                </div>
                <p class="note">{tx('frShare')}</p>
                <form class="row" onsubmit={e => { e.preventDefault(); search(); }}>
                    <input class="edt grow" maxlength="24" spellcheck="false" placeholder={tx('frSearchHint')} bind:value={q}>
                    <button class="btn hot" type="submit" disabled={busy || q.trim().length < 2}
                            onpointerenter={hoverBlip}>{tx('frFind')}</button>
                </form>
                {#if found && found.length}
                    {#each found as p (p.code)}
                        <FriendBanner {p}>
                            {#if has('friends', p.code)}
                                <span class="tag">{tx('frIsFriend')}</span>
                            {:else if has('outgoing', p.code)}
                                <span class="tag">{tx('frIsSent')}</span>
                            {:else}
                                <button class="fbtn" disabled={busy} onpointerenter={hoverBlip}
                                        onclick={() => add(p.code)}>{tx(has('incoming', p.code) ? 'frAccept' : 'frAdd')}</button>
                            {/if}
                        </FriendBanner>
                    {/each}
                {/if}
            </div>
        {/if}
        {#if msg}<p class="msg">{msg}</p>{/if}

        {#if fr.invites && fr.invites.length}
            <h4>{tx('frInvites')}</h4>
            {#each fr.invites as p (p.code)}
                <FriendBanner {p}>
                    <button class="fbtn red" disabled={busy} onpointerenter={hoverBlip} onclick={() => join(p.code)}>{tx('frJoin')}</button>
                    <button class="fbtn small" disabled={busy} onpointerenter={hoverBlip} onclick={() => dropInv(p.code)}>{tx('frDecline')}</button>
                </FriendBanner>
            {/each}
        {/if}

        {#if fr.incoming.length}
            <h4>{tx('frIncoming')}</h4>
            {#each fr.incoming as p (p.code)}
                <FriendBanner {p}>
                    <button class="fbtn red" disabled={busy} onpointerenter={hoverBlip} onclick={() => add(p.code)}>{tx('frAccept')}</button>
                    <button class="fbtn small" disabled={busy} onpointerenter={hoverBlip} onclick={() => drop(p.code)}>{tx('frDecline')}</button>
                </FriendBanner>
            {/each}
        {/if}

        {#if fr.friends.length}
            {#each fr.friends as p (p.code)}
                <FriendBanner {p}>
                    <button class="fbtn" disabled={busy} onpointerenter={hoverBlip}
                            onclick={() => invite(p.code)}>{tx('frInvite')}</button>
                    <button class="fbtn small" disabled={busy} onpointerenter={hoverBlip}
                            onclick={() => drop(p.code, true)}>{tx(armed === p.code ? 'frRemoveAsk' : 'frRemove')}</button>
                </FriendBanner>
            {/each}
        {:else}
            <p class="note">{tx('frEmpty')}</p>
        {/if}

        {#if fr.outgoing.length}
            <h4>{tx('frOutgoing')}</h4>
            {#each fr.outgoing as p (p.code)}
                <FriendBanner {p} tag={tx('frIsSent')}>
                    <button class="fbtn small" disabled={busy} onpointerenter={hoverBlip} onclick={() => drop(p.code)}>{tx('frCancel')}</button>
                </FriendBanner>
            {/each}
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
    /* you: your name and code on a steel plate */
    .me {
        display: flex; gap: calc(var(--u) * 1.4); align-items: baseline; flex-wrap: wrap;
        padding: calc(var(--u) * .9) calc(var(--u) * 1.4);
        background: var(--plate); border: 1px solid #2e3846; border-left: calc(var(--u) * .35) solid #e6bb63;
        box-shadow: var(--bevel);
    }
    .who { color: #fff; font-weight: 800; text-transform: uppercase; letter-spacing: .04em; }
    .idcode { font-family: var(--font-mono); letter-spacing: .12em; color: var(--code); }
    .me .idcode { font-size: calc(var(--u) * 1.6); user-select: all; }
    /* the heading bar: the game's black steel, the count in gold (0.4.4) */
    .bar {
        display: flex; align-items: center; justify-content: space-between; gap: calc(var(--u) * 1);
        padding: calc(var(--u) * .5) calc(var(--u) * .6) calc(var(--u) * .5) calc(var(--u) * 1.4);
        margin-bottom: calc(var(--u) * 1.2);
        background: linear-gradient(rgba(255, 255, 255, .06), rgba(255, 255, 255, 0) 50%), var(--tex-grain), linear-gradient(#1c2027, #07080a);
        border: 1px solid #5a4422; border-left: calc(var(--u) * .4) solid #b8321c;
        box-shadow: inset 0 0 0 1px #000, 0 2px 6px rgba(0, 0, 0, .7);
    }
    .bar h3 {
        margin: 0; padding: 0; border: 0;
        font-family: var(--font-head); font-weight: 900; font-style: italic; text-transform: uppercase;
        font-size: calc(var(--u) * 2.4); letter-spacing: .04em; color: #e6bb63; text-shadow: 0 2px 0 #000;
    }
    .bar .fbtn {
        min-width: calc(var(--u) * 16); height: calc(var(--u) * 3.8); padding: 0 calc(var(--u) * 1.4);
        font-family: var(--font-head); font-weight: 900; font-style: italic; text-transform: uppercase;
        font-size: calc(var(--u) * 1.4); letter-spacing: .04em; color: #fff; text-shadow: 0 2px 0 #000;
        background: linear-gradient(rgba(255, 255, 255, .1), rgba(255, 255, 255, 0) 45%), var(--tex-grain), linear-gradient(#2a2f37, #0b0d10);
        border: 2px solid #8a6a32; box-shadow: inset 0 0 0 1px #000, 0 2px 4px rgba(0, 0, 0, .7); cursor: pointer;
    }
    .bar .fbtn:hover { border-color: #e6bb63; filter: brightness(1.2); }
    .add {
        padding: calc(var(--u) * 1) calc(var(--u) * 1.2); margin-bottom: calc(var(--u) * 1.2);
        background: var(--sunk); border: 1px solid #2e3846; border-left: calc(var(--u) * .35) solid #e6bb63;
    }
    .add .me { margin-bottom: calc(var(--u) * .4); }
    h4 {
        margin: calc(var(--u) * 1.2) 0 calc(var(--u) * .3); font-style: italic; text-transform: uppercase;
        font-size: calc(var(--u) * 1.3); letter-spacing: .08em; color: #e6bb63;
    }
    .tag { text-shadow: 0 2px 0 #000; color: #fff; font-weight: 800; letter-spacing: .08em; }
    .msg { color: #fff; padding: calc(var(--u) * .6) calc(var(--u) * 1); background: var(--sunk); border-left: calc(var(--u) * .3) solid var(--red); }
    p { white-space: normal; overflow-wrap: anywhere; max-width: 100%; }
</style>
