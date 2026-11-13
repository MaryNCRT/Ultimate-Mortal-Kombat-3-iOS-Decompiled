/* Discord Rich Presence (port, not in the binary): what the player is doing,
 * shown on their Discord profile, and a friend's "Join" from Discord taken
 * into the game's EOS lobby.
 *
 * Through the Discord desktop client already running on this PC, over its
 * local IPC (a named pipe on Windows, a Unix socket on Linux) -- the
 * unauthenticated path Discord documents for Rich Presence: only the
 * application's id, no Discord sign-in, no account linking, no SDK to ship.
 * Discord not running, or the pipe closing: nothing happens, the game goes
 * on, and it tries again every few seconds.  Nothing here blocks a frame.
 *
 * umk3.ini discord=0 turns it off (the launcher's SETTINGS); UMK3_DISCORD=0
 * too. */
#ifndef DISCORD_PRESENCE_H
#define DISCORD_PRESENCE_H

/* The Discord application ("UMK3 iOS Decomp", Mary's): its name is what
 * Discord shows as "Playing ...". */
#define DISCORD_APP_ID "1570587748946018304"
/* The large image: the same HD icon the launcher puts in Steam
 * (launcher/app/lib/artwork.js PAGE_SET icon, SteamGridDB icon 76847). */
#define DISCORD_LARGE_IMAGE "https://cdn2.steamgriddb.com/icon/ba4eb2077a7c3feeafda768fd01d599a.png"

struct discord_activity {
    char details[128];          /* first line: "Arcade", "Online lobby" */
    char state[128];            /* second line: the fighter, the opponent */
    long long start;            /* unix seconds of "elapsed", 0 none */
    char large_text[128];
    char party_id[128];         /* with join: the lobby, Discord's party */
    int  party_size, party_max;
    char join[160];             /* the join secret, "" none */
};

void discord_presence_enable(int on);
/* Every frame: connects (now and then), sends the activity when it changed
 * (at most once in 4 s, as Discord asks), reads what Discord sent. */
void discord_presence_tick(double now);
void discord_presence_set(const struct discord_activity *a);
void discord_presence_clear(void);
/* A join from Discord (a friend's "Join" or an accepted invite): its secret,
 * once (1), else 0. */
int  discord_presence_take_join(char *secret, int n);
int  discord_presence_connected(void);
void discord_presence_shutdown(void);

/* The JSON this module writes, exposed for its test (tests/discord_presence_test.c). */
int  discord_activity_json(const struct discord_activity *a, int pid, const char *nonce, char *out, int n);
int  discord_find_join_secret(const char *json, char *secret, int n);

/* game_main.c: the activity from the game's state, every frame (presence_game.c) */
void presence_game_tick(double now);

#endif
