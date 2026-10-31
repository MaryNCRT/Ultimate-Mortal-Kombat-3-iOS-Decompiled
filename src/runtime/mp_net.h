/* mp_net.h -- the network session under limeMPGameSpecific.c. Port, not in
 * the binary: the iPhone game ran its multiplayer over GKSession (Bluetooth,
 * the limeMPSession class); this is what takes that class's place, so the
 * game's own packet code (decomp/gamecode/limeMPGameSpecific.c) runs over a
 * PC network. See mp_net.c. */
#ifndef UMK3_MP_NET_H
#define UMK3_MP_NET_H

/* ---- what limeMPGameSpecific.m asked of limeMPSession ---- */

int  mpnet_session_exists(void);        /* gSession != nil */
void mpnet_session_create(void);        /* alloc, init, mpSetupSession */
void mpnet_session_destroy(void);       /* mpDestroySession, release */
int  mpnet_connection_state(void);      /* mpGetConnectionState: 2 = connected */
int  mpnet_session_state(void);         /* mpSessionState: 2 = connected */
int  mpnet_am_server(void);             /* mpAmIServer */
int  mpnet_is_working(void);            /* mpIsWorking */
/* mpSendPacket / mpSendPacketUnreliable: the 4-byte type goes first, big
 * endian, as the original put it on the wire. */
void mpnet_send(long type, const void *data, int len, int reliable);
/* the Bluetooth lobby's list (FE_Task_Multiplayer): always empty here */
int  mpnet_peer_count(void);

/* ---- the port's own: the ONLINE screen ---- */

/* The other player, from the handshake. */
struct mpnet_peer {
    char     code[16];                  /* XXXX-XXXX-XXXX, profiles.ini id */
    char     name[100];                 /* UTF-8 */
    int      icon;
    unsigned color, color2;
    int      dark, angle, alt;          /* struct profile's */
};

enum { MPNET_IDLE, MPNET_SEARCHING, MPNET_NOT_FOUND, MPNET_CONNECTED,
       MPNET_LOST, MPNET_BUSY };

/* Look for the player with this code and connect to them (they must have
 * the ONLINE screen open). Whoever is found hosts the fight. */
void mpnet_find(const char *code);
void mpnet_cancel_find(void);
int  mpnet_status(void);                /* MPNET_* */
/* Internet play (Epic Online Services, mp_eos.h): MPEOS_OFF, _STARTING,
 * _READY or _FAILED. */
int  mpnet_internet(void);
const struct mpnet_peer *mpnet_peer(void);   /* NULL before a connection */
/* Called once a frame by game_main.c: reads the socket, hands each game
 * packet to processPacket, resends what was not acknowledged. */
void mpnet_poll(void);
/* Puts what was just sent on the wire now: EOS only sends in its tick, which
 * mpnet_poll does at the start of the next frame (rollback.c's inputs). */
void mpnet_flush(void);

/* ---- MULTIPLAYER: the local network alone ---- */

/* Set before startMP: 1 for PLAY > MULTIPLAYER (no EOS, the games on the
 * local network listed), 0 for ONLINE. */
void mpnet_set_lan_only(int on);
/* The lobby's row i (NULL past the end), and asking that game to play. */
const struct mpnet_peer *mpnet_lobby_peer(int i);
void mpnet_lobby_connect(int i);

#endif
