/* What the player is doing, for Discord (discord_presence.h): read from the
 * game's own state every frame, sent when it changes.
 *
 *   CurrentTask 6        a fight: GameMode 0 ARCADE, 3 KARNAGE, 4 SURVIVAL,
 *                        5 the Lost Treasures, 6 two players (PortPractice:
 *                        TRAINING), 1 or a connected session: ONLINE
 *   CurrentTask 3        the menus: FE_CurrentTask 0 the main menu, 0x1b the
 *                        select screen, 51 the ONLINE screen, 0x2a its select
 *                        screen, 0x2b its versus screen; in an EOS lobby (any
 *                        screen): the lobby, with a party and a join secret
 *                        so a friend's "Join" on Discord comes in
 *
 * A join from Discord: its secret is "umk3lobby:<EOS lobby id>". It waits
 * here until the game is in the menus; from the main menu the ONLINE screen
 * is opened on LOBBIES (FrontEnd.c PortOnlineOpenLobbies), whose tick takes
 * it (presence_take_lobby) and joins the lobby as the list's JOIN does. */
#include "discord_presence.h"
#include "mp_eos.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern int  CurrentTask;
extern int  FE_CurrentTask;
extern long GameMode;
extern int  PortPractice;
extern long Character1, Character2;
extern char Language[10];
extern const char *CharacterNames[26];
long isMPConnected(void);
int  mpnet_am_server(void);
void PortOnlineOpenLobbies(void);

#define JOIN_PREFIX "umk3lobby:"

static long long g_started;
static char g_lobby[128];               /* a join from Discord, waiting */
static double g_opened_at = -1.0;

static int es(void) { return Language[0] == 'E' && Language[1] == 'S'; }
static const char *T(const char *e, const char *en) { return es() ? e : en; }

/* "SUB-ZERO" -> "Sub-Zero", "KUNG LAO" -> "Kung Lao" */
static void fighter(long i, char *out, int n)
{
    const char *s = (i >= 0 && i < 26) ? CharacterNames[i] : NULL;
    int k = 0, up = 1;
    if (!s) { out[0] = 0; return; }
    for (; *s && k < n - 1; s++) {
        unsigned char c = (unsigned char)*s;
        out[k++] = (char)(up ? toupper(c) : tolower(c));
        up = !isalpha(c);
    }
    out[k] = 0;
}

static void versus(char *out, int n, long a, long b)
{
    char fa[32], fb[32];
    fighter(a, fa, sizeof fa);
    fighter(b, fb, sizeof fb);
    if (fa[0] && fb[0]) snprintf(out, (size_t)n, "%s vs %s", fa, fb);
    else snprintf(out, (size_t)n, "%s", fa);
}

static void build(struct discord_activity *a)
{
    const int online = isMPConnected() != 0;
    const int lobby = mplobby_state() == MPLOBBY_IN;

    memset(a, 0, sizeof *a);
    a->start = g_started;
    snprintf(a->large_text, sizeof a->large_text, "Ultimate Mortal Kombat 3 IOS Decomp");

    if (CurrentTask == 6) {
        if (online || GameMode == 1) {
            long me = mpnet_am_server() ? Character1 : Character2;
            long them = mpnet_am_server() ? Character2 : Character1;
            snprintf(a->details, sizeof a->details, "%s", T("Pelea online", "Online match"));
            versus(a->state, sizeof a->state, me, them);
        } else if (GameMode == 6 && PortPractice) {
            snprintf(a->details, sizeof a->details, "%s", T("Entrenamiento", "Training"));
            fighter(Character1, a->state, sizeof a->state);
        } else {
            const char *m = GameMode == 6 ? T("Versus local", "Local versus")
                          : GameMode == 4 ? T("Supervivencia", "Survival")
                          : GameMode == 3 ? "Karnage"
                          : GameMode == 5 ? T("Tesoros de Shao Kahn", "Shao Kahn's Lost Treasures")
                          : "Arcade";
            snprintf(a->details, sizeof a->details, "%s", m);
            versus(a->state, sizeof a->state, Character1, Character2);
        }
    } else if (CurrentTask == 3 && FE_CurrentTask == 0x2a) {
        snprintf(a->details, sizeof a->details, "%s", T("Online", "Online"));
        snprintf(a->state, sizeof a->state, "%s", T("Eligiendo luchador", "Choosing a fighter"));
    } else if (CurrentTask == 3 && FE_CurrentTask == 0x2b) {
        snprintf(a->details, sizeof a->details, "%s", T("Online", "Online"));
        versus(a->state, sizeof a->state, mpnet_am_server() ? Character1 : Character2,
               mpnet_am_server() ? Character2 : Character1);
    } else if (CurrentTask == 3 && FE_CurrentTask == 0x1b) {
        snprintf(a->details, sizeof a->details, "%s",
                 GameMode == 6 && PortPractice ? T("Entrenamiento", "Training")
                 : GameMode == 6 ? T("Versus local", "Local versus") : "Arcade");
        snprintf(a->state, sizeof a->state, "%s", T("Eligiendo luchador", "Choosing a fighter"));
    } else if (CurrentTask == 3 && FE_CurrentTask == 51) {
        snprintf(a->details, sizeof a->details, "%s", T("Men\xc3\xba online", "Online menu"));
    } else if (CurrentTask == 3 && FE_CurrentTask == 0) {
        snprintf(a->details, sizeof a->details, "%s", T("Men\xc3\xba principal", "Main menu"));
    } else {
        snprintf(a->details, sizeof a->details, "%s", T("En los men\xc3\xbas", "In the menus"));
    }

    /* an EOS lobby, whatever the screen: the party and its join */
    if (lobby && CurrentTask == 3) {
        const struct mplobby_info *in = mplobby_info();
        if (in && in->id[0]) {
            snprintf(a->details, sizeof a->details, "%s", T("En una sala online", "In an online lobby"));
            snprintf(a->state, sizeof a->state, "%s", in->name[0] ? in->name : T("Sala", "Lobby"));
            snprintf(a->party_id, sizeof a->party_id, "umk3-%s", in->id);
            a->party_size = in->members > 0 ? in->members : 1;
            a->party_max = in->max_members > 0 ? in->max_members : 2;
            if (a->party_size < a->party_max)
                snprintf(a->join, sizeof a->join, JOIN_PREFIX "%s", in->id);
        }
    }
}

void presence_game_tick(double now)
{
    struct discord_activity a;
    char secret[160];

    if (!g_started)
        g_started = (long long)time(NULL);
    discord_presence_tick(now);

    if (discord_presence_take_join(secret, sizeof secret)
        && !strncmp(secret, JOIN_PREFIX, strlen(JOIN_PREFIX))) {
        snprintf(g_lobby, sizeof g_lobby, "%s", secret + strlen(JOIN_PREFIX));
        g_opened_at = -1.0;
        printf("discord: a join for lobby %s\n", g_lobby);
    }
    /* waiting: from the main menu, the ONLINE screen on LOBBIES (once; again
     * after 10 s if the player went back) */
    if (g_lobby[0] && CurrentTask == 3 && FE_CurrentTask == 0
        && (g_opened_at < 0.0 || now - g_opened_at > 10.0)) {
        g_opened_at = now;
        PortOnlineOpenLobbies();
    }

    build(&a);
    discord_presence_set(&a);
}

/* FrontEnd.c PortLobbyTick: the lobby a Discord join waits to enter, once */
int presence_take_lobby(char *id, int n)
{
    if (!g_lobby[0])
        return 0;
    snprintf(id, (size_t)n, "%s", g_lobby);
    g_lobby[0] = 0;
    return 1;
}
