/* mp_eos.h -- ONLINE over the internet, through Epic Online Services. Port,
 * not in the binary: what mp_net.c uses to reach a player outside the local
 * network. See mp_eos.c. */
#ifndef UMK3_MP_EOS_H
#define UMK3_MP_EOS_H

enum { MPEOS_OFF,                       /* no SDK in the build, no DLL or no eos.ini */
       MPEOS_STARTING,                  /* logging in */
       MPEOS_READY,
       MPEOS_FAILED };

int  mpeos_state(void);
/* Loads the DLL and logs in, once; later calls do nothing. */
void mpeos_start(void);
/* Once a frame: EOS_Platform_Tick, which runs the callbacks. */
void mpeos_tick(void);

/* While the ONLINE screen is open, this game can be found by its code. */
void mpeos_advertise(const char *code);
void mpeos_unadvertise(void);

/* Who has this code? mpeos_lookup_result: 0 still asking, 1 found (`who`
 * is their EOS user, a string for mpeos_send), -1 nobody. */
void mpeos_lookup(const char *code);
int  mpeos_lookup_result(char *who, int n);

/* Datagrams to and from an EOS user, unreliable: mp_net.c's own channel
 * does the acknowledging, as over UDP. mpeos_recv returns the bytes, 0 when
 * nothing is waiting. */
void mpeos_send(const char *who, const void *p, int n);
int  mpeos_recv(char *who, int wn, void *buf, int max);

/* The players with the ONLINE screen open, -1 while not known (internet
 * play off, or not counted yet); counted every 15 seconds. */
int  mpeos_players(void);

/* ---- LOBBIES (docs/research/ONLINE-PHASE-PLAN.md, stage 2) ----
 *
 * EOS lobbies in the bucket "umk3-lobby", one at a time. A lobby's own
 * attributes: "ver" (MPLOBBY_PROTO; a search sees only its own), "name",
 * "host" (the owner's name), "rules" (online_rules.c's text), "priv" (1: not
 * in the browser, found by its "key" alone), "key" (six letters and digits),
 * "go" (the owner calling a member to fight: "<n>:<EOS user>"). Each
 * member's: "code" (profiles.ini's), "name", "look" (icon and colours, the
 * caller's text), "ready". The owner hosts the fight: the member it calls
 * connects to it through mp_net (mpnet_poll takes the call). */

#define MPLOBBY_PROTO       "1"
#define MPLOBBY_MAX_MEMBERS 8
#define MPLOBBY_MAX_RESULTS 32

struct mplobby_member {
    char who[40];                       /* EOS user, for kick / promote */
    char code[16];
    char name[64];
    char look[48];
    int  ready;
    int  owner;
};

struct mplobby_info {
    char id[128];
    char name[48];
    char host[64];
    char rules[64];
    char key[8];
    char owner[40];                     /* EOS user */
    int  priv;
    int  members, max_members;
};

enum { MPLOBBY_NONE,                    /* in no lobby */
       MPLOBBY_CREATING, MPLOBBY_JOINING,
       MPLOBBY_IN,
       MPLOBBY_LEAVING };

/* Why the last lobby ended or a create / join failed (mplobby_event). */
enum { MPLOBBY_EV_NONE, MPLOBBY_EV_LEFT, MPLOBBY_EV_KICKED, MPLOBBY_EV_CLOSED,
       MPLOBBY_EV_FULL, MPLOBBY_EV_NOT_FOUND, MPLOBBY_EV_FAILED, MPLOBBY_EV_OFFLINE };

int  mplobby_state(void);
int  mplobby_event(void);
/* Bumped on every change (members, attributes, state): the screen redraws
 * its copy when it moves. */
unsigned mplobby_serial(void);

/* Create: 2..MPLOBBY_MAX_MEMBERS players; a private one gets a key. Fails
 * (0) while in a lobby or off line. */
int  mplobby_create(const char *name, const char *rules, int priv, int max_members);
/* Join a lobby by its id (a search result's). */
int  mplobby_join(const char *id);
void mplobby_leave(void);

/* Search: the open public lobbies of this version, or, with a key, the one
 * private lobby that has it. `rules` non-NULL keeps those with exactly these
 * rules. mplobby_results: -1 searching, else how many (copied into out). */
void mplobby_search(const char *rules, const char *key);
int  mplobby_results(struct mplobby_info *out, int max);

/* The lobby this game is in (NULL when none) and its members. */
const struct mplobby_info *mplobby_info(void);
int  mplobby_members(struct mplobby_member *out, int max);
int  mplobby_am_owner(void);

/* This game's member attributes; sent when changed. */
void mplobby_set_me(const char *code, const char *name, const char *look, int ready);
void mplobby_set_ready(int ready);
/* The owner's: the lobby's name and rules (members see them change). */
void mplobby_set_rules(const char *rules);
void mplobby_set_name(const char *name);
/* The owner's: public or private after the lobby is made (the key stays). */
void mplobby_set_priv(int priv);
void mplobby_kick(const char *who);

/* Invites. mplobby_invite: a player by their code, found as FIND MATCH
 * finds them (their ONLINE screen open), then sent an EOS lobby invite;
 * mplobby_invite_status: 0 none, 1 looking, 3 sending, 2 sent, -1 failed.
 * mplobby_invites: the invites this game was sent (`seen` once read);
 * mplobby_accept_invite joins one (and drops it). */
struct mplobby_invite {
    char id[128];
    char lobby[128];
    char from[64];                      /* the host's name */
    char name[48];                      /* the lobby's */
    int  seen;
};
void mplobby_invite(const char *code);
/* The invite travels over P2P as one of mp_net's packets, of this kind
 * (mp_net.c on_packet gives it to mplobby_invite_got, whole). */
#define MPLOBBY_INV_KIND 0x40
void mplobby_invite_got(const unsigned char *p, int n);
int  mplobby_invite_status(void);
int  mplobby_invites(struct mplobby_invite *out, int max);
int  mplobby_accept_invite(int i);
void mplobby_drop_invite(int i);
void mplobby_promote(const char *who);
/* The owner calls a member to fight; that member's game takes the call
 * (mplobby_take_call: 1 once, with the owner's code and EOS user). */
void mplobby_call(const char *who);
int  mplobby_take_call(char *code, int cn, char *who, int wn);

#endif
