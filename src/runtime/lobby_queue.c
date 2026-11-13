/* lobby_queue.c -- a lobby's line of players and how it turns. Port, not in
 * the binary; see lobby_queue.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lobby_queue.h"

void lq_opts_default(lq_opts *o)
{
    o->mode = LQ_1V1;
    o->after = LQ_WINNER_STAYS;
    o->streak = 0;
    o->line = LQ_LINE_INOUT;
    o->autostart = 0;
    o->stage = LQ_STAGE_FIGHT;
    o->pick = 0;
    o->ask = 0;
    o->cede = 1;
    o->readyt = 0;
}

int lq_opts_encode(const lq_opts *o, char *out, int n)
{
    return snprintf(out, (size_t)n, "o%d;a%d;s%d;l%d;n%d;e%d;p%d;j%d;c%d;t%d", o->mode, o->after,
                    o->streak, o->line, o->autostart, o->stage, o->pick, o->ask, o->cede, o->readyt);
}

int lq_opts_decode(const char *s, lq_opts *o)
{
    lq_opts t;
    int seen = 0;

    if (!s || !s[0])
        return 0;
    lq_opts_default(&t);
    while (*s) {
        char k = *s++;
        char *e;
        long v = strtol(s, &e, 10);
        if (e == s)
            return 0;
        s = e;
        switch (k) {
        case 'o': t.mode = (int)v;      seen |= 1;  break;
        case 'a': t.after = (int)v;     seen |= 2;  break;
        case 's': t.streak = (int)v;    seen |= 4;  break;
        case 'l': t.line = (int)v;      seen |= 8;  break;
        case 'n': t.autostart = (int)v; seen |= 16; break;
        case 'e': t.stage = (int)v;     break;
        case 'p': t.pick = (int)v;      break;
        case 'j': t.ask = (int)v;       break;
        case 'c': t.cede = (int)v;      break;
        case 't': t.readyt = (int)v;    break;
        default: break;                 /* a later version's: skipped */
        }
        if (*s == ';')
            s++;
        else if (*s)
            return 0;
    }
    if (!(seen & 1) || t.mode < LQ_1V1 || t.mode > LQ_ROTATION
        || t.after < LQ_WINNER_STAYS || t.after > LQ_BOTH_ROTATE
        || t.streak < 0 || t.streak > 99
        || t.line < LQ_LINE_LEADER || t.line > LQ_LINE_FREE
        || (t.autostart != 0 && t.autostart != 1)
        || t.stage < 0 || t.stage >= LQ_STAGE_N || t.pick < 0 || t.pick >= LQ_STAGES
        || (t.ask != 0 && t.ask != 1) || (t.cede != 0 && t.cede != 1)
        || t.readyt < 0 || t.readyt > 30)
        return 0;
    *o = t;
    return 1;
}

void lq_clear(lq_line *l)
{
    memset(l, 0, sizeof *l);
}

static void put_ids(char *out, int n, int *at, const char (*id)[LQ_ID], int k)
{
    int i;
    for (i = 0; i < k; i++)
        *at += snprintf(out + *at, *at < n ? (size_t)(n - *at) : 0, "%s%s", i ? "," : "", id[i]);
}

int lq_encode(const lq_line *l, char *out, int n)
{
    int at = snprintf(out, (size_t)n, "%d|%s|", l->streak, l->done);
    put_ids(out, n, &at, l->id, l->n);
    at += snprintf(out + at, at < n ? (size_t)(n - at) : 0, "|");
    put_ids(out, n, &at, l->out, l->nout);
    if (l->loser[0] || l->npend)
        at += snprintf(out + at, at < n ? (size_t)(n - at) : 0, "|%s", l->loser);
    if (l->npend) {
        at += snprintf(out + at, at < n ? (size_t)(n - at) : 0, "|");
        put_ids(out, n, &at, l->pend, l->npend);
    }
    return at;
}

/* ids "a,b,c" up to the next '|' (or the end) */
static const char *get_ids(const char *s, char (*id)[LQ_ID], int *k)
{
    *k = 0;
    while (*s && *s != '|') {
        int j = 0;
        while (*s && *s != ',' && *s != '|') {
            if (j < LQ_ID - 1 && *k < LQ_MAX)
                id[*k][j++] = *s;
            s++;
        }
        if (j && *k < LQ_MAX)
            id[(*k)++][j] = 0;
        if (*s == ',')
            s++;
    }
    return s;
}

int lq_decode(const char *s, lq_line *l)
{
    lq_line t;
    char *e;
    int j = 0;

    lq_clear(&t);
    if (!s || !s[0]) {
        *l = t;
        return 1;
    }
    t.streak = (int)strtol(s, &e, 10);
    if (e == s || *e != '|' || t.streak < 0)
        return 0;
    s = e + 1;
    while (*s && *s != '|') {
        if (j < (int)sizeof t.done - 1)
            t.done[j++] = *s;
        s++;
    }
    if (*s != '|')
        return 0;
    s = get_ids(s + 1, t.id, &t.n);
    if (*s == '|')
        s = get_ids(s + 1, t.out, &t.nout);
    if (*s == '|') {
        snprintf(t.loser, sizeof t.loser, "%.*s", (int)strcspn(s + 1, "|,"), s + 1);
        s = strchr(s + 1, '|');
        if (s)
            get_ids(s + 1, t.pend, &t.npend);
    }
    *l = t;
    return 1;
}

static int find_in(const char (*id)[LQ_ID], int k, const char *who)
{
    int i;
    for (i = 0; who && i < k; i++)
        if (!strcmp(id[i], who))
            return i;
    return -1;
}

int lq_find(const lq_line *l, const char *id) { return find_in(l->id, l->n, id); }
int lq_is_out(const lq_line *l, const char *id) { return find_in(l->out, l->nout, id) >= 0; }

static void drop_at(char (*id)[LQ_ID], int *k, int i)
{
    if (i < 0 || i >= *k)
        return;
    memmove(id[i], id[i + 1], (size_t)(*k - i - 1) * LQ_ID);
    (*k)--;
}

static void push_back(char (*id)[LQ_ID], int *k, const char *who)
{
    if (*k < LQ_MAX && who && who[0]) {
        snprintf(id[*k], LQ_ID, "%s", who);
        (*k)++;
    }
}

int lq_is_pending(const lq_line *l, const char *id) { return find_in(l->pend, l->npend, id) >= 0; }

int lq_accept(lq_line *l, const char *id)
{
    int i = find_in(l->pend, l->npend, id);

    if (i < 0)
        return 0;
    drop_at(l->pend, &l->npend, i);
    push_back(l->id, &l->n, id);
    return 1;
}

int lq_sync(lq_line *l, const char (*members)[LQ_ID], int n, int ask, const char *leader)
{
    int i, changed = 0, front = l->n ? 1 : 0;
    char first[LQ_ID];

    snprintf(first, sizeof first, "%s", front ? l->id[0] : "");
    for (i = l->n - 1; i >= 0; i--)
        if (find_in(members, n, l->id[i]) < 0) {
            drop_at(l->id, &l->n, i);
            changed = 1;
        }
    for (i = l->nout - 1; i >= 0; i--)
        if (find_in(members, n, l->out[i]) < 0) {
            drop_at(l->out, &l->nout, i);
            changed = 1;
        }
    for (i = l->npend - 1; i >= 0; i--)
        if (find_in(members, n, l->pend[i]) < 0) {
            drop_at(l->pend, &l->npend, i);
            changed = 1;
        }
    if (!ask)                           /* free again: those asking come in */
        while (l->npend) {
            push_back(l->id, &l->n, l->pend[0]);
            drop_at(l->pend, &l->npend, 0);
            changed = 1;
        }
    for (i = 0; i < n; i++)
        if (members[i][0] && lq_find(l, members[i]) < 0 && !lq_is_out(l, members[i])
            && !lq_is_pending(l, members[i])) {
            if (ask && !(leader && !strcmp(leader, members[i])))
                push_back(l->pend, &l->npend, members[i]);
            else
                push_back(l->id, &l->n, members[i]);
            changed = 1;
        }
    if (front && (!l->n || strcmp(first, l->id[0])))
        l->streak = 0;                  /* another one at the front */
    return changed;
}

int lq_move(lq_line *l, const char *id, int dir)
{
    int i = lq_find(l, id), j = i + (dir < 0 ? -1 : 1);
    char t[LQ_ID];

    if (i < 0 || j < 0 || j >= l->n)
        return 0;
    memcpy(t, l->id[i], LQ_ID);
    memcpy(l->id[i], l->id[j], LQ_ID);
    memcpy(l->id[j], t, LQ_ID);
    if (i == 0 || j == 0)
        l->streak = 0;
    return 1;
}

int lq_put(lq_line *l, const char *id, int at)
{
    int i = lq_find(l, id);

    if (!id || !id[0] || i == at)
        return 0;
    if (i < 0) {
        if (l->n >= LQ_MAX)
            return 0;
        drop_at(l->out, &l->nout, find_in(l->out, l->nout, id));
    } else
        drop_at(l->id, &l->n, i);
    if (at > l->n)
        at = l->n;
    if (at < 0)
        at = 0;
    memmove(l->id[at + 1], l->id[at], (size_t)(l->n - at) * LQ_ID);
    snprintf(l->id[at], LQ_ID, "%s", id);
    l->n++;
    if (at == 0 || i == 0)
        l->streak = 0;
    return 1;
}

int lq_swap(lq_line *l, const char *a, const char *b)
{
    int i = lq_find(l, a), j = lq_find(l, b);

    if (i < 0 || !b || !b[0] || !strcmp(a, b))
        return 0;
    if (j < 0) {                        /* b was out: b takes a's place, a goes out */
        int o = find_in(l->out, l->nout, b);
        if (o < 0)
            return 0;
        snprintf(l->out[o], LQ_ID, "%s", a);
        snprintf(l->id[i], LQ_ID, "%s", b);
    } else {
        char t[LQ_ID];
        memcpy(t, l->id[i], LQ_ID);
        memcpy(l->id[i], l->id[j], LQ_ID);
        memcpy(l->id[j], t, LQ_ID);
    }
    if (i == 0 || j == 0)
        l->streak = 0;
    return 1;
}

int lq_set_in(lq_line *l, const char *id, int in)
{
    int i = lq_find(l, id), o = find_in(l->out, l->nout, id);

    if (!id || !id[0])
        return 0;
    if (in) {
        if (i >= 0)
            return 0;
        drop_at(l->out, &l->nout, o);
        push_back(l->id, &l->n, id);
        return 1;
    }
    if (i < 0)
        return 0;
    if (i == 0)
        l->streak = 0;
    drop_at(l->id, &l->n, i);
    if (o < 0)
        push_back(l->out, &l->nout, id);
    return 1;
}

int lq_result(lq_line *l, const lq_opts *o, const char *winner, const char *loser)
{
    int w = lq_find(l, winner), d = lq_find(l, loser), kept;

    if (loser)
        snprintf(l->loser, sizeof l->loser, "%s", loser);   /* the next stage may be theirs */

    if (o->mode != LQ_ROTATION || w < 0 || d < 0 || w == d)
        return 0;
    kept = l->n && !strcmp(l->id[0], winner);   /* the winner was at the front */
    if (w > d) {
        drop_at(l->id, &l->n, w);
        drop_at(l->id, &l->n, d);
    } else {
        drop_at(l->id, &l->n, d);
        drop_at(l->id, &l->n, w);
    }
    if (o->after == LQ_BOTH_ROTATE) {
        push_back(l->id, &l->n, winner);
        push_back(l->id, &l->n, loser);
        l->streak = 0;
        return 1;
    }
    l->streak = kept ? l->streak + 1 : 1;
    if (o->streak > 0 && l->streak >= o->streak && l->n > 0) {
        /* enough in a row: the winner to the back too, after the loser */
        push_back(l->id, &l->n, loser);
        push_back(l->id, &l->n, winner);
        l->streak = 0;
        return 1;
    }
    memmove(l->id[1], l->id[0], (size_t)l->n * LQ_ID);
    snprintf(l->id[0], LQ_ID, "%s", winner);
    l->n++;
    push_back(l->id, &l->n, loser);
    return 1;
}

int lq_draw(lq_line *l, const lq_opts *o, const char *a, const char *b)
{
    lq_opts both = *o;

    both.after = LQ_BOTH_ROTATE;
    return lq_result(l, &both, a, b);
}

int lq_allowed(const lq_opts *o, const char *act)
{
    if (!act)
        return 0;
    if (!strcmp(act, "in") || !strcmp(act, "out"))
        return o->line >= LQ_LINE_INOUT;
    if (!strcmp(act, "up") || !strcmp(act, "down"))
        return o->line >= LQ_LINE_FREE;
    if (!strcmp(act, "pass") || !strncmp(act, "cede:", 5))
        return o->cede;
    return 0;
}

static int vote_of(const char (*who)[LQ_ID], const int *votes, int n, const char *id)
{
    int i = find_in(who, n, id);
    return i >= 0 && votes[i] >= 0 && votes[i] < LQ_STAGES ? votes[i] : -1;
}

int lq_stage(const lq_opts *o, const lq_line *l, const char (*who)[LQ_ID], const int *votes,
             int n, const char *a, const char *b, unsigned roll)
{
    int count[LQ_STAGES] = { 0 }, best = -1, nbest = 0, i, va, vb;

    switch (o->stage) {
    case LQ_STAGE_LEADER:
        return o->pick;
    case LQ_STAGE_VOTE:
        for (i = 0; i < n; i++)
            if (votes[i] >= 0 && votes[i] < LQ_STAGES)
                count[votes[i]]++;
        for (i = 0; i < LQ_STAGES; i++)
            if (count[i] && (best < 0 || count[i] > count[best]))
                best = i;
        if (best < 0)
            return (int)(roll % LQ_STAGES);
        for (i = 0; i < LQ_STAGES; i++)     /* a tie: one of the tied, drawn */
            nbest += count[i] == count[best];
        nbest = (int)(roll % (unsigned)nbest);
        for (i = 0; i < LQ_STAGES; i++)
            if (count[i] == count[best] && nbest-- == 0)
                return i;
        return best;
    case LQ_STAGE_FIGHTERS:
        va = vote_of(who, votes, n, a);
        vb = vote_of(who, votes, n, b);
        if (va < 0 && vb < 0)
            return (int)(roll % LQ_STAGES);
        if (va < 0 || vb < 0)
            return va < 0 ? vb : va;
        return roll & 1 ? vb : va;          /* the same, or one of the two */
    case LQ_STAGE_LOSER:
        va = l->loser[0] ? vote_of(who, votes, n, l->loser) : -1;
        return va >= 0 ? va : (int)(roll % LQ_STAGES);
    case LQ_STAGE_RANDOM:
        return (int)(roll % LQ_STAGES);
    default:
        return -1;
    }
}
