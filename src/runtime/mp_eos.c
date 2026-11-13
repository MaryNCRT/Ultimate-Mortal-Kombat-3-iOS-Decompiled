/*
 * mp_eos.c -- ONLINE over the internet, through Epic Online Services (EOS).
 * Port, not in the binary: the iPhone game only knew Bluetooth.
 *
 * The same design as Saints Reborn's (github.com/whompay/SaintsReborn,
 * core/WhompaysCoop/eos_link.h): the SDK's DLL is loaded when the game runs,
 * so the game starts without it; the login is anonymous, with the device id
 * EOS keeps for this computer (no Epic account); the product's ids are read
 * from eos.ini beside the exe, which the product's owner writes:
 *
 *   [eos]
 *   product=...      sandbox=...      deployment=...
 *   client_id=...    client_secret=...
 *
 * Finding a player by their code: while their ONLINE screen is open, their
 * game owns a lobby in the bucket "umk3:" + the code (letters and digits,
 * upper case). FIND MATCH searches that bucket and the lobby's owner is the
 * player to talk to. Nobody joins the lobby: it is only the directory. Then
 * mp_net.c's own packets go over EOS P2P, which crosses NATs (Epic's relays
 * when it has to), so there are no ports to open.
 *
 * Built with the SDK's headers only when they are on the computer
 * (UMK3_EOS, launcher/build_game.ps1); without them every call here says
 * MPEOS_OFF and the game plays on the local network alone.
 */
#include <stdio.h>
#include <string.h>

#include "mp_eos.h"

#ifndef UMK3_EOS

int  mpeos_state(void) { return MPEOS_OFF; }
void mpeos_start(void) {}
void mpeos_tick(void) {}
void mpeos_advertise(const char *code) { (void)code; }
void mpeos_unadvertise(void) {}
void mpeos_lookup(const char *code) { (void)code; }
int  mpeos_lookup_result(char *who, int n) { (void)who; (void)n; return -1; }
void mpeos_send(const char *who, const void *p, int n) { (void)who; (void)p; (void)n; }
int  mpeos_recv(char *who, int wn, void *buf, int max)
{ (void)who; (void)wn; (void)buf; (void)max; return 0; }
int  mpeos_players(void) { return -1; }

int  mplobby_state(void) { return MPLOBBY_NONE; }
int  mplobby_event(void) { return MPLOBBY_EV_OFFLINE; }
unsigned mplobby_serial(void) { return 0; }
int  mplobby_create(const char *name, const char *rules, int priv, int max_members)
{ (void)name; (void)rules; (void)priv; (void)max_members; return 0; }
int  mplobby_join(const char *id) { (void)id; return 0; }
void mplobby_leave(void) {}
void mplobby_search(const char *rules, const char *key) { (void)rules; (void)key; }
int  mplobby_results(struct mplobby_info *out, int max) { (void)out; (void)max; return 0; }
const struct mplobby_info *mplobby_info(void) { return NULL; }
int  mplobby_members(struct mplobby_member *out, int max) { (void)out; (void)max; return 0; }
int  mplobby_am_owner(void) { return 0; }
void mplobby_set_me(const char *code, const char *name, const char *look, int ready)
{ (void)code; (void)name; (void)look; (void)ready; }
void mplobby_set_ready(int ready) { (void)ready; }
void mplobby_set_rules(const char *rules) { (void)rules; }
void mplobby_set_name(const char *name) { (void)name; }
void mplobby_set_priv(int priv) { (void)priv; }
void mplobby_set_key(const char *key) { (void)key; }
void mplobby_key_clean(const char *in, char *out, int n)
{
    int i, j = 0;
    for (i = 0; in && in[i] && j < MPLOBBY_KEY_MAX && j < n - 1; i++) {
        int c = (unsigned char)in[i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
            out[j++] = (char)c;
    }
    out[j] = 0;
}
void mplobby_invite(const char *code) { (void)code; }
int  mplobby_invite_status(void) { return 0; }
int  mplobby_invites(struct mplobby_invite *out, int max) { (void)out; (void)max; return 0; }
int  mplobby_accept_invite(int i) { (void)i; return 0; }
void mplobby_drop_invite(int i) { (void)i; }
void mplobby_invite_got(const unsigned char *p, int n) { (void)p; (void)n; }
void mplobby_kick(const char *who) { (void)who; }
void mplobby_promote(const char *who) { (void)who; }
void mplobby_dissolve(void) {}
void mplobby_leave_now(void) {}
void mplobby_set_room(const char *opts) { (void)opts; }
void mplobby_set_line(const char *line) { (void)line; }
const char *mplobby_room(void) { return ""; }
const char *mplobby_line(void) { return ""; }
void mplobby_want(const char *act) { (void)act; }
void mplobby_report(const char *res) { (void)res; }
int  mplobby_go_parts(const char *go, char *n, int nn, char *a, int an, char *b, int bn)
{ (void)go; (void)n; (void)nn; (void)a; (void)an; (void)b; (void)bn; return 0; }
void mplobby_call(const char *a, const char *b, int stage) { (void)a; (void)b; (void)stage; }
int  mplobby_go_stage(const char *go) { (void)go; return -1; }
void mplobby_vote(int stage) { (void)stage; }
int  mplobby_take_call(char *code, int cn, char *who, int wn)
{ (void)code; (void)cn; (void)who; (void)wn; return 0; }

#else

#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#define EOS_LIB   "EOSSDK-Win64-Shipping.dll"
#define SEP       "\\"
#define lib_open(p)     ((void *)LoadLibraryA(p))
#define lib_sym(h, n)   ((void *)GetProcAddress((HMODULE)(h), n))
#define make_dir(p)     CreateDirectoryA(p, NULL)
static unsigned ticks(void) { return (unsigned)GetTickCount(); }
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define EOS_LIB   "libEOSSDK-Linux-Shipping.so"
#define SEP       "/"
#define MAX_PATH  4096
#define lib_open(p)     dlopen(p, RTLD_NOW | RTLD_LOCAL)
#define lib_sym(h, n)   dlsym(h, n)
#define make_dir(p)     mkdir(p, 0755)
static unsigned ticks(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}
#endif

#include "eos_sdk.h"
#include "eos_logging.h"
#include "eos_connect.h"
#include "eos_lobby.h"
#include "eos_p2p.h"

#define SOCKET_NAME "UMK3"

/* The DLL's functions, looked up by name. */
#define EOS_FUNCS(X) \
    X(EOS_Initialize) X(EOS_Platform_Create) X(EOS_Platform_Tick) \
    X(EOS_Platform_GetConnectInterface) X(EOS_Platform_GetLobbyInterface) \
    X(EOS_Platform_GetP2PInterface) X(EOS_Logging_SetCallback) \
    X(EOS_Logging_SetLogLevel) X(EOS_EResult_ToString) \
    X(EOS_ProductUserId_ToString) X(EOS_ProductUserId_FromString) \
    X(EOS_Connect_CreateDeviceId) X(EOS_Connect_DeleteDeviceId) X(EOS_Connect_Login) X(EOS_Connect_CreateUser) \
    X(EOS_Lobby_CreateLobby) X(EOS_Lobby_DestroyLobby) \
    X(EOS_Lobby_CreateLobbySearch) X(EOS_LobbySearch_SetParameter) \
    X(EOS_LobbySearch_Find) X(EOS_LobbySearch_GetSearchResultCount) \
    X(EOS_LobbySearch_CopySearchResultByIndex) X(EOS_LobbySearch_Release) \
    X(EOS_LobbyDetails_GetLobbyOwner) X(EOS_LobbyDetails_Release) \
    X(EOS_P2P_SendPacket) X(EOS_P2P_ReceivePacket) \
    X(EOS_P2P_GetNextReceivedPacketSize) X(EOS_P2P_AcceptConnection) \
    X(EOS_P2P_AddNotifyPeerConnectionRequest) \
    X(EOS_Lobby_JoinLobbyById) X(EOS_Lobby_LeaveLobby) \
    X(EOS_Lobby_UpdateLobbyModification) X(EOS_Lobby_UpdateLobby) \
    X(EOS_Lobby_PromoteMember) X(EOS_Lobby_KickMember) \
    X(EOS_Lobby_AddNotifyLobbyUpdateReceived) \
    X(EOS_Lobby_AddNotifyLobbyMemberUpdateReceived) \
    X(EOS_Lobby_AddNotifyLobbyMemberStatusReceived) \
    X(EOS_Lobby_CopyLobbyDetailsHandle) X(EOS_Lobby_Attribute_Release) \
    X(EOS_LobbyModification_AddAttribute) X(EOS_LobbyModification_AddMemberAttribute) \
    X(EOS_LobbyModification_Release) \
    X(EOS_LobbyDetails_CopyInfo) X(EOS_LobbyDetails_Info_Release) \
    X(EOS_LobbyDetails_CopyAttributeByKey) X(EOS_LobbyDetails_CopyMemberAttributeByKey) \
    X(EOS_LobbyDetails_GetMemberCount) X(EOS_LobbyDetails_GetMemberByIndex) \
    X(EOS_Lobby_SendInvite) X(EOS_Lobby_AddNotifyLobbyInviteReceived) \
    X(EOS_Lobby_CopyLobbyDetailsHandleByInviteId)

#define DECL(f) static __typeof__(f) *p_##f;
EOS_FUNCS(DECL)
#undef DECL

static int g_state = MPEOS_OFF;
static int g_tried;
static EOS_HPlatform  g_plat;
static EOS_HConnect   g_conn;
static EOS_HLobby     g_lobby;
static EOS_HP2P       g_p2p;
static EOS_ProductUserId g_me;
static EOS_P2P_SocketId  g_socket;

/* eos.ini */
static char k_product[64], k_sandbox[64], k_deploy[64], k_id[96], k_secret[96];

/* the lobby that says "this code is here" */
static char g_adv_want[24];             /* the bucket wanted, "" for none */
static char g_adv_have[24];             /* the bucket of g_lobby_id */
static char g_lobby_id[128];
static int  g_lobby_busy;               /* a create or destroy in flight */

/* FIND MATCH */
static int  g_look_pending;             /* asked before the login finished */
static char g_look_bucket[24];
static int  g_look_result;              /* 0 asking, 1 found, -1 none */
static char g_look_who[40];

static void bucket_of(const char *code, char *out, int n)
{
    int k = snprintf(out, (size_t)n, "umk3:");
    for (; *code && k < n - 1; code++) {
        int c = (unsigned char)*code;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
            out[k++] = (char)c;
    }
    out[k] = 0;
}

static const char *res(EOS_EResult r)
{
    return p_EOS_EResult_ToString ? p_EOS_EResult_ToString(r) : "?";
}

static void id_string(EOS_ProductUserId u, char *out, int n)
{
    int32_t len = n;
    out[0] = 0;
    if (!u || p_EOS_ProductUserId_ToString(u, out, &len) != EOS_Success)
        out[0] = 0;
}

static void EOS_CALL on_log(const EOS_LogMessage *m)
{
    printf("eos: %s: %s\n", m->Category, m->Message);
}

/* ------------------------------------------------------------- eos.ini */

static void exe_dir(char *out, int n)
{
    char *s;
#ifdef _WIN32
    DWORD k = GetModuleFileNameA(NULL, out, (DWORD)n);
    if (k == 0 || k >= (DWORD)n) {
        out[0] = 0;
        return;
    }
    s = strrchr(out, '\\');
#else
    ssize_t k = readlink("/proc/self/exe", out, (size_t)n - 1);
    if (k <= 0) {
        out[0] = 0;
        return;
    }
    out[k] = 0;
    s = strrchr(out, '/');
#endif
    if (s)
        s[1] = 0;
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
}

static int read_ini(const char *dir)
{
    char path[MAX_PATH + 16], line[256];
    FILE *f;

    snprintf(path, sizeof path, "%seos.ini", dir);
    f = fopen(path, "r");
    if (!f) {
        printf("online: no eos.ini beside the exe: internet play is off\n");
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        char *v = strchr(line, '='), *k = line;
        if (!v || line[0] == '[' || line[0] == ';' || line[0] == '#')
            continue;
        *v++ = 0;
        while (*k == ' ' || *k == '\t') k++;
        while (*v == ' ' || *v == '\t') v++;
        trim(k);
        trim(v);
#define KEY(name, dst) if (!strcmp(k, name)) snprintf(dst, sizeof dst, "%s", v);
        KEY("product", k_product) KEY("sandbox", k_sandbox)
        KEY("deployment", k_deploy) KEY("client_id", k_id)
        KEY("client_secret", k_secret)
#undef KEY
    }
    fclose(f);
    if (!k_product[0] || !k_sandbox[0] || !k_deploy[0] || !k_id[0] || !k_secret[0]) {
        printf("online: eos.ini is missing product, sandbox, deployment, "
               "client_id or client_secret: internet play is off\n");
        return 0;
    }
    return 1;
}

/* --------------------------------------------------------------- lobby */

static void lobby_step(void);

static void EOS_CALL on_destroyed(const EOS_Lobby_DestroyLobbyCallbackInfo *d)
{
    (void)d;
    g_lobby_busy = 0;
    lobby_step();
}

static void EOS_CALL on_created(const EOS_Lobby_CreateLobbyCallbackInfo *d)
{
    g_lobby_busy = 0;
    if (d->ResultCode != EOS_Success) {
        printf("online: EOS lobby not created: %s\n", res(d->ResultCode));
        g_adv_have[0] = 0;
        g_adv_want[0] = 0;              /* not again every frame */
        return;
    }
    snprintf(g_lobby_id, sizeof g_lobby_id, "%s", d->LobbyId);
    printf("online: findable over the internet (%s)\n", g_adv_have);
    lobby_step();
}

/* Brings the lobby in line with g_adv_want, one call at a time. */
static void lobby_step(void)
{
    if (g_state != MPEOS_READY || g_lobby_busy)
        return;
    if (g_lobby_id[0] && strcmp(g_adv_have, g_adv_want) != 0) {
        EOS_Lobby_DestroyLobbyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
        o.LocalUserId = g_me;
        o.LobbyId = g_lobby_id;
        g_lobby_busy = 1;
        p_EOS_Lobby_DestroyLobby(g_lobby, &o, NULL, on_destroyed);
        g_lobby_id[0] = 0;
        g_adv_have[0] = 0;
        return;
    }
    if (!g_lobby_id[0] && g_adv_want[0]) {
        EOS_Lobby_CreateLobbyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
        o.LocalUserId = g_me;
        o.MaxLobbyMembers = 2;
        o.PermissionLevel = EOS_LPL_PUBLICADVERTISED;
        o.bPresenceEnabled = EOS_FALSE;
        o.bAllowInvites = EOS_FALSE;
        o.BucketId = g_adv_want;
        o.bDisableHostMigration = EOS_TRUE;
        snprintf(g_adv_have, sizeof g_adv_have, "%s", g_adv_want);
        g_lobby_busy = 1;
        p_EOS_Lobby_CreateLobby(g_lobby, &o, NULL, on_created);
    }
}

/* ------------------------------------------------------ PLAYERS ONLINE
 *
 * Every game with the ONLINE screen open owns, beside the lobby named after
 * its code, one in the bucket HERE_BUCKET; the lobbies there are the
 * players on line (Mary: "how many players are active"). Nobody joins it
 * either. Games older than this one do not make it, so they are not
 * counted. */
#define HERE_BUCKET "umk3-online"
static char g_here_id[128];
static int  g_here_want, g_here_busy;
static int  g_players = -1, g_count_busy;
static unsigned g_count_at;

static void here_step(void);

static void EOS_CALL on_here_destroyed(const EOS_Lobby_DestroyLobbyCallbackInfo *d)
{
    (void)d;
    g_here_busy = 0;
    here_step();
}

static void EOS_CALL on_here_created(const EOS_Lobby_CreateLobbyCallbackInfo *d)
{
    g_here_busy = 0;
    if (d->ResultCode != EOS_Success) {
        printf("online: EOS presence lobby not created: %s\n", res(d->ResultCode));
        g_here_want = 0;                /* not again every frame */
        return;
    }
    snprintf(g_here_id, sizeof g_here_id, "%s", d->LobbyId);
    g_count_at = 0;                     /* count again, with this one */
    here_step();
}

static void here_step(void)
{
    if (g_state != MPEOS_READY || g_here_busy)
        return;
    if (g_here_id[0] && !g_here_want) {
        EOS_Lobby_DestroyLobbyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
        o.LocalUserId = g_me;
        o.LobbyId = g_here_id;
        g_here_busy = 1;
        p_EOS_Lobby_DestroyLobby(g_lobby, &o, NULL, on_here_destroyed);
        g_here_id[0] = 0;
    } else if (!g_here_id[0] && g_here_want) {
        EOS_Lobby_CreateLobbyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
        o.LocalUserId = g_me;
        o.MaxLobbyMembers = 2;
        o.PermissionLevel = EOS_LPL_PUBLICADVERTISED;
        o.bPresenceEnabled = EOS_FALSE;
        o.bAllowInvites = EOS_FALSE;
        o.BucketId = HERE_BUCKET;
        o.bDisableHostMigration = EOS_TRUE;
        g_here_busy = 1;
        p_EOS_Lobby_CreateLobby(g_lobby, &o, NULL, on_here_created);
    }
}

static void EOS_CALL on_counted(const EOS_LobbySearch_FindCallbackInfo *d)
{
    EOS_HLobbySearch s = (EOS_HLobbySearch)d->ClientData;

    g_count_busy = 0;
    if (d->ResultCode == EOS_Success) {
        EOS_LobbySearch_GetSearchResultCountOptions c;
        memset(&c, 0, sizeof c);
        c.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
        g_players = (int)p_EOS_LobbySearch_GetSearchResultCount(s, &c);
    } else if (d->ResultCode == EOS_NotFound)
        g_players = 0;
    p_EOS_LobbySearch_Release(s);
}

static void count_now(void)
{
    EOS_Lobby_CreateLobbySearchOptions o;
    EOS_LobbySearch_SetParameterOptions sp;
    EOS_LobbySearch_FindOptions fo;
    EOS_Lobby_AttributeData a;
    EOS_HLobbySearch s = NULL;

    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    o.MaxResults = 200;
    if (p_EOS_Lobby_CreateLobbySearch(g_lobby, &o, &s) != EOS_Success) {
        g_count_busy = 0;
        return;
    }
    memset(&a, 0, sizeof a);
    a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    a.Key = EOS_LOBBY_SEARCH_BUCKET_ID;
    a.Value.AsUtf8 = HERE_BUCKET;
    a.ValueType = EOS_AT_STRING;
    memset(&sp, 0, sizeof sp);
    sp.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
    sp.Parameter = &a;
    sp.ComparisonOp = EOS_CO_EQUAL;
    p_EOS_LobbySearch_SetParameter(s, &sp);
    memset(&fo, 0, sizeof fo);
    fo.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
    fo.LocalUserId = g_me;
    p_EOS_LobbySearch_Find(s, &fo, s, on_counted);
}

/* The players on line, -1 while not known; asked again every 15 seconds
 * while someone asks (the ONLINE screen, every frame). */
int mpeos_players(void)
{
    if (g_state != MPEOS_READY)
        return -1;
    if (!g_count_busy && (!g_count_at || ticks() - g_count_at > 15000)) {
        g_count_at = ticks();
        g_count_busy = 1;
        count_now();
    }
    return g_players;
}

void mpeos_advertise(const char *code)
{
    bucket_of(code, g_adv_want, sizeof g_adv_want);
    lobby_step();
    g_here_want = 1;
    here_step();
}

void mpeos_unadvertise(void)
{
    g_adv_want[0] = 0;
    lobby_step();
    g_here_want = 0;
    here_step();
}

/* -------------------------------------------------------------- lookup */

static void EOS_CALL on_found(const EOS_LobbySearch_FindCallbackInfo *d)
{
    EOS_HLobbySearch s = (EOS_HLobbySearch)d->ClientData;
    EOS_LobbySearch_GetSearchResultCountOptions c;
    EOS_LobbySearch_CopySearchResultByIndexOptions ci;
    EOS_LobbyDetails_GetLobbyOwnerOptions oo;
    EOS_HLobbyDetails det = NULL;
    uint32_t i, n;

    if (g_look_result != 0)             /* a newer search took its place */
        goto done;
    if (d->ResultCode != EOS_Success) {
        printf("online: EOS search: %s\n", res(d->ResultCode));
        g_look_result = -1;
        goto done;
    }
    memset(&c, 0, sizeof c);
    c.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
    n = p_EOS_LobbySearch_GetSearchResultCount(s, &c);
    g_look_result = -1;
    for (i = 0; i < n; i++) {
        char who[40];
        memset(&ci, 0, sizeof ci);
        ci.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
        ci.LobbyIndex = i;
        if (p_EOS_LobbySearch_CopySearchResultByIndex(s, &ci, &det) != EOS_Success)
            continue;
        memset(&oo, 0, sizeof oo);
        oo.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
        id_string(p_EOS_LobbyDetails_GetLobbyOwner(det, &oo), who, sizeof who);
        p_EOS_LobbyDetails_Release(det);
        if (who[0]) {
            char mine[40];
            id_string(g_me, mine, sizeof mine);
            if (!strcmp(who, mine))     /* a lobby of ours left over */
                continue;
            snprintf(g_look_who, sizeof g_look_who, "%s", who);
            g_look_result = 1;
            printf("online: found over the internet (%s)\n", who);
            break;
        }
    }
done:
    p_EOS_LobbySearch_Release(s);
}

static void look_now(void)
{
    EOS_Lobby_CreateLobbySearchOptions o;
    EOS_LobbySearch_SetParameterOptions sp;
    EOS_LobbySearch_FindOptions fo;
    EOS_Lobby_AttributeData a;
    EOS_HLobbySearch s = NULL;

    g_look_pending = 0;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    o.MaxResults = 4;
    if (p_EOS_Lobby_CreateLobbySearch(g_lobby, &o, &s) != EOS_Success) {
        g_look_result = -1;
        return;
    }
    memset(&a, 0, sizeof a);
    a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    a.Key = EOS_LOBBY_SEARCH_BUCKET_ID;
    a.Value.AsUtf8 = g_look_bucket;
    a.ValueType = EOS_AT_STRING;
    memset(&sp, 0, sizeof sp);
    sp.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
    sp.Parameter = &a;
    sp.ComparisonOp = EOS_CO_EQUAL;
    p_EOS_LobbySearch_SetParameter(s, &sp);
    memset(&fo, 0, sizeof fo);
    fo.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
    fo.LocalUserId = g_me;
    p_EOS_LobbySearch_Find(s, &fo, s, on_found);
}

void mpeos_lookup(const char *code)
{
    bucket_of(code, g_look_bucket, sizeof g_look_bucket);
    g_look_result = 0;
    g_look_who[0] = 0;
    if (g_state == MPEOS_READY)
        look_now();
    else if (g_state == MPEOS_STARTING)
        g_look_pending = 1;
    else
        g_look_result = -1;
}

int mpeos_lookup_result(char *who, int n)
{
    if (g_look_result == 1)
        snprintf(who, (size_t)n, "%s", g_look_who);
    return g_look_result;
}

/* ----------------------------------------------------------------- P2P */

static void EOS_CALL on_request(const EOS_P2P_OnIncomingConnectionRequestInfo *d)
{
    EOS_P2P_AcceptConnectionOptions o;

    if (!d->SocketId || strcmp(d->SocketId->SocketName, SOCKET_NAME) != 0)
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
    o.LocalUserId = g_me;
    o.RemoteUserId = d->RemoteUserId;
    o.SocketId = &g_socket;
    p_EOS_P2P_AcceptConnection(g_p2p, &o);
}

void mpeos_send(const char *who, const void *p, int n)
{
    EOS_P2P_SendPacketOptions o;
    EOS_ProductUserId to;
    EOS_EResult r;

    if (g_state != MPEOS_READY || !who[0] || n <= 0 || n > EOS_P2P_MAX_PACKET_SIZE)
        return;
    to = p_EOS_ProductUserId_FromString(who);
    if (!to)
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
    o.LocalUserId = g_me;
    o.RemoteUserId = to;
    o.SocketId = &g_socket;
    o.Channel = 0;
    o.DataLengthBytes = (uint32_t)n;
    o.Data = p;
    o.bAllowDelayedDelivery = EOS_TRUE; /* held until the connection is up */
    o.Reliability = EOS_PR_UnreliableUnordered;
    r = p_EOS_P2P_SendPacket(g_p2p, &o);
    if (r != EOS_Success) {
        static unsigned last;
        if (ticks() - last > 2000) {
            last = ticks();
            printf("online: EOS send: %s\n", res(r));
        }
    }
}

int mpeos_recv(char *who, int wn, void *buf, int max)
{
    EOS_P2P_GetNextReceivedPacketSizeOptions so;
    EOS_P2P_ReceivePacketOptions ro;
    EOS_ProductUserId from = NULL;
    EOS_P2P_SocketId sock;
    uint8_t ch = 0;
    uint32_t size = 0, got = 0;

    if (g_state != MPEOS_READY)
        return 0;
    memset(&so, 0, sizeof so);
    so.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
    so.LocalUserId = g_me;
    if (p_EOS_P2P_GetNextReceivedPacketSize(g_p2p, &so, &size) != EOS_Success)
        return 0;
    memset(&ro, 0, sizeof ro);
    ro.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
    ro.LocalUserId = g_me;
    ro.MaxDataSizeBytes = (uint32_t)max;
    memset(&sock, 0, sizeof sock);
    if (p_EOS_P2P_ReceivePacket(g_p2p, &ro, &from, &sock, &ch, buf, &got) != EOS_Success)
        return 0;
    if (strcmp(sock.SocketName, SOCKET_NAME) != 0 || size > (uint32_t)max)
        return -1;                      /* not ours, or cut: skip it */
    id_string(from, who, wn);
    return (int)got;
}

/* ------------------------------------------------------------- LOBBIES
 *
 * Port, not in the binary (the iPhone game had only Bluetooth). One lobby at
 * a time; what the screen asks for is kept here and sent to EOS one call at
 * a time from mpeos_tick (lobby_flush). See mp_eos.h for the attributes. */
#define LOBBY_BUCKET "umk3-lobby"

static int      L_state = MPLOBBY_NONE, L_event;
static unsigned L_serial;
static char     L_id[128];
static struct mplobby_info   L_info;
static struct mplobby_member L_mem[MPLOBBY_MAX_MEMBERS];
static int      L_nmem;
static int      L_busy;                 /* an UpdateLobby in flight */
static unsigned L_retry_at;             /* after a failed update */

/* what this game wants the lobby to say */
static char L_me_code[16], L_me_name[64], L_me_look[48];
static int  L_me_ready, L_me_dirty;
static char L_w_name[48], L_w_rules[64], L_w_key[MPLOBBY_KEY_MAX + 4], L_w_go[96];
static char L_w_room[48], L_w_q[520];   /* the room's options, its line (lobby_queue.h) */
static char L_info_q[520];              /* the lobby's line, as read */
static char L_me_want[72], L_me_res[32];
static int  L_me_wins, L_me_vote = -1;
static int  L_want_n;
static char L_my_go[24];                /* the number of the last call this game was in */
static char L_next_key[MPLOBBY_KEY_MAX + 4];  /* the host's own key for the next lobby */
static int  L_w_priv, L_w_max, L_attr_dirty, L_go_n;

/* the fight call this game was given */
static char L_go_seen[96];
static int  L_go_primed;               /* the call there on joining is not ours */
static void make_key(char *out);
static int  L_call;
static char L_call_code[16], L_call_who[40];

/* search */
static int  L_search = 0;               /* 0 idle / done, 1 asking */
static struct mplobby_info L_res[MPLOBBY_MAX_RESULTS];
static int  L_nres;
static EOS_HLobbySearch L_search_h;     /* the newest search; older answers dropped */

static void copy_str(char *dst, int n, const char *s)
{
    snprintf(dst, (size_t)n, "%s", s ? s : "");
}

static void my_id(char *out, int n)
{
    id_string(g_me, out, n);
}

/* an attribute as text (an int64 written out), "" when missing */
static void attr_text(EOS_HLobbyDetails d, const char *key, const char *member_who,
                      char *out, int n)
{
    EOS_Lobby_Attribute *a = NULL;
    EOS_EResult r;

    out[0] = 0;
    if (member_who) {
        EOS_LobbyDetails_CopyMemberAttributeByKeyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBYDETAILS_COPYMEMBERATTRIBUTEBYKEY_API_LATEST;
        o.TargetUserId = p_EOS_ProductUserId_FromString(member_who);
        o.AttrKey = key;
        r = p_EOS_LobbyDetails_CopyMemberAttributeByKey(d, &o, &a);
    } else {
        EOS_LobbyDetails_CopyAttributeByKeyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;
        o.AttrKey = key;
        r = p_EOS_LobbyDetails_CopyAttributeByKey(d, &o, &a);
    }
    if (r != EOS_Success || !a)
        return;
    if (a->Data) {
        if (a->Data->ValueType == EOS_AT_STRING)
            copy_str(out, n, a->Data->Value.AsUtf8);
        else if (a->Data->ValueType == EOS_AT_INT64)
            snprintf(out, (size_t)n, "%lld", (long long)a->Data->Value.AsInt64);
        else if (a->Data->ValueType == EOS_AT_BOOLEAN)
            snprintf(out, (size_t)n, "%d", a->Data->Value.AsBool ? 1 : 0);
    }
    p_EOS_Lobby_Attribute_Release(a);
}

/* A lobby's own fields from its details (a search result or ours) */
static void read_info(EOS_HLobbyDetails d, struct mplobby_info *out)
{
    EOS_LobbyDetails_CopyInfoOptions o;
    EOS_LobbyDetails_Info *inf = NULL;
    char t[16];

    memset(out, 0, sizeof *out);
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
    if (p_EOS_LobbyDetails_CopyInfo(d, &o, &inf) == EOS_Success && inf) {
        copy_str(out->id, sizeof out->id, inf->LobbyId);
        id_string(inf->LobbyOwnerUserId, out->owner, sizeof out->owner);
        out->max_members = (int)inf->MaxMembers;
        out->members = (int)(inf->MaxMembers - inf->AvailableSlots);
        p_EOS_LobbyDetails_Info_Release(inf);
    }
    attr_text(d, "name", NULL, out->name, sizeof out->name);
    attr_text(d, "host", NULL, out->host, sizeof out->host);
    attr_text(d, "rules", NULL, out->rules, sizeof out->rules);
    attr_text(d, "key", NULL, out->key, sizeof out->key);
    attr_text(d, "room", NULL, out->room, sizeof out->room);
    attr_text(d, "priv", NULL, t, sizeof t);
    out->priv = t[0] == '1';
}

static void EOS_CALL on_destroyed_quiet(const EOS_Lobby_DestroyLobbyCallbackInfo *d)
{
    (void)d;
}

static void EOS_CALL on_left_quiet(const EOS_Lobby_LeaveLobbyCallbackInfo *d)
{
    (void)d;
}

static void lobby_reset(int ev)
{
    if (L_state != MPLOBBY_NONE || ev != MPLOBBY_EV_NONE)
        L_event = ev;
    L_state = MPLOBBY_NONE;
    L_id[0] = 0;
    L_nmem = 0;
    L_busy = 0;
    L_me_dirty = L_attr_dirty = 0;
    L_go_seen[0] = 0;
    L_go_primed = 0;
    L_call = 0;
    L_info_q[0] = 0;
    L_me_want[0] = L_me_res[0] = 0;
    L_me_wins = 0;
    L_me_vote = -1;
    L_my_go[0] = 0;
    memset(&L_info, 0, sizeof L_info);
    L_serial++;
}

/* Reads our lobby again: its fields, its members and theirs. */
static void lobby_refresh(void)
{
    EOS_Lobby_CopyLobbyDetailsHandleOptions o;
    EOS_LobbyDetails_GetMemberCountOptions mc;
    EOS_HLobbyDetails d = NULL;
    char mine[40], go[96];
    uint32_t i, n;

    if (L_state != MPLOBBY_IN || !L_id[0])
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    o.LobbyId = L_id;
    o.LocalUserId = g_me;
    if (p_EOS_Lobby_CopyLobbyDetailsHandle(g_lobby, &o, &d) != EOS_Success || !d)
        return;
    read_info(d, &L_info);
    my_id(mine, sizeof mine);
    memset(&mc, 0, sizeof mc);
    mc.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
    n = p_EOS_LobbyDetails_GetMemberCount(d, &mc);
    L_nmem = 0;
    for (i = 0; i < n && L_nmem < MPLOBBY_MAX_MEMBERS; i++) {
        EOS_LobbyDetails_GetMemberByIndexOptions mi;
        struct mplobby_member *m = &L_mem[L_nmem];
        char t[16];

        memset(&mi, 0, sizeof mi);
        mi.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;
        mi.MemberIndex = i;
        memset(m, 0, sizeof *m);
        id_string(p_EOS_LobbyDetails_GetMemberByIndex(d, &mi), m->who, sizeof m->who);
        if (!m->who[0])
            continue;
        attr_text(d, "code", m->who, m->code, sizeof m->code);
        attr_text(d, "name", m->who, m->name, sizeof m->name);
        attr_text(d, "look", m->who, m->look, sizeof m->look);
        attr_text(d, "ready", m->who, t, sizeof t);
        m->ready = t[0] == '1';         /* before t is used again */
        attr_text(d, "want", m->who, m->want, sizeof m->want);
        attr_text(d, "res", m->who, m->res, sizeof m->res);
        attr_text(d, "wins", m->who, t, sizeof t);
        m->wins = atoi(t);
        attr_text(d, "vote", m->who, t, sizeof t);
        m->vote = t[0] ? atoi(t) : -1;
        m->owner = !strcmp(m->who, L_info.owner);
        L_nmem++;
    }
    attr_text(d, "q", NULL, L_info_q, sizeof L_info_q);
    /* the owner's fight call: "<n>:<A>:<B>", taken once; B connects to A */
    attr_text(d, "go", NULL, go, sizeof go);
    copy_str(L_info.go, sizeof L_info.go, go);
    if (!L_go_primed) {                 /* an older call, from before this game came in */
        L_go_primed = 1;
        copy_str(L_go_seen, sizeof L_go_seen, go);
    } else if (go[0] && strcmp(go, L_go_seen) != 0) {
        char gn[24], ga[40], gb[40];
        copy_str(L_go_seen, sizeof L_go_seen, go);
        if (mplobby_go_parts(go, gn, sizeof gn, ga, sizeof ga, gb, sizeof gb)
            && (!strcmp(ga, mine) || !strcmp(gb, mine)) && strcmp(ga, gb) != 0) {
            copy_str(L_my_go, sizeof L_my_go, gn);
            L_me_res[0] = 0;            /* this call's end not told yet */
            if (!strcmp(gb, mine)) {    /* B: connect to A */
                for (i = 0; i < (uint32_t)L_nmem; i++)
                    if (!strcmp(L_mem[i].who, ga)) {
                        copy_str(L_call_code, sizeof L_call_code, L_mem[i].code);
                        copy_str(L_call_who, sizeof L_call_who, L_mem[i].who);
                        L_call = 1;
                        printf("online: lobby: called to fight %s (call %s)\n", L_mem[i].name, gn);
                    }
            } else
                printf("online: lobby: called to fight, hosting (call %s)\n", gn);
        }
    }
    p_EOS_LobbyDetails_Release(d);
    L_serial++;
}

static int add_attr(EOS_HLobbyModification m, int member, const char *key,
                    const char *val)
{
    EOS_Lobby_AttributeData a;

    memset(&a, 0, sizeof a);
    a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    a.Key = key;
    a.Value.AsUtf8 = val;
    a.ValueType = EOS_AT_STRING;
    if (member) {
        EOS_LobbyModification_AddMemberAttributeOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST;
        o.Attribute = &a;
        o.Visibility = EOS_LAT_PUBLIC;
        return p_EOS_LobbyModification_AddMemberAttribute(m, &o) == EOS_Success;
    } else {
        EOS_LobbyModification_AddAttributeOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
        o.Attribute = &a;
        o.Visibility = EOS_LAT_PUBLIC;
        return p_EOS_LobbyModification_AddAttribute(m, &o) == EOS_Success;
    }
}

static void EOS_CALL on_lobby_updated(const EOS_Lobby_UpdateLobbyCallbackInfo *d)
{
    L_busy = 0;
    if (d->ResultCode != EOS_Success) {
        printf("online: lobby update: %s\n", res(d->ResultCode));
        L_retry_at = ticks() + 2000;
        L_me_dirty = L_attr_dirty = 1;  /* again, a little later */
        return;
    }
    lobby_refresh();
}

/* Sends what changed, one UpdateLobby at a time. */
static void lobby_flush(void)
{
    EOS_Lobby_UpdateLobbyModificationOptions mo;
    EOS_Lobby_UpdateLobbyOptions uo;
    EOS_HLobbyModification m = NULL;
    char t[8], tw[12], tv[12];          /* each its own: EOS reads them at UpdateLobby */
    int owner, ok = 1;

    if (L_state != MPLOBBY_IN || L_busy || (!L_me_dirty && !L_attr_dirty))
        return;
    if (L_retry_at && (int)(ticks() - L_retry_at) < 0)
        return;
    L_retry_at = 0;
    owner = mplobby_am_owner();
    if (!owner)
        L_attr_dirty = 0;               /* only the owner writes the lobby's */
    if (!L_me_dirty && !L_attr_dirty)
        return;
    memset(&mo, 0, sizeof mo);
    mo.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
    mo.LocalUserId = g_me;
    mo.LobbyId = L_id;
    if (p_EOS_Lobby_UpdateLobbyModification(g_lobby, &mo, &m) != EOS_Success || !m)
        return;
    if (L_attr_dirty) {
        ok &= add_attr(m, 0, "ver", MPLOBBY_PROTO);
        ok &= add_attr(m, 0, "name", L_w_name);
        ok &= add_attr(m, 0, "host", L_me_name);
        ok &= add_attr(m, 0, "rules", L_w_rules);
        ok &= add_attr(m, 0, "priv", L_w_priv ? "1" : "0");
        ok &= add_attr(m, 0, "key", L_w_key);
        if (L_w_go[0])
            ok &= add_attr(m, 0, "go", L_w_go);
        if (L_w_room[0])
            ok &= add_attr(m, 0, "room", L_w_room);
        if (L_w_q[0])
            ok &= add_attr(m, 0, "q", L_w_q);
    }
    if (L_me_dirty) {
        ok &= add_attr(m, 1, "code", L_me_code);
        ok &= add_attr(m, 1, "name", L_me_name);
        ok &= add_attr(m, 1, "look", L_me_look);
        snprintf(t, sizeof t, "%d", L_me_ready ? 1 : 0);
        ok &= add_attr(m, 1, "ready", t);
        if (L_me_want[0])
            ok &= add_attr(m, 1, "want", L_me_want);
        if (L_me_res[0])
            ok &= add_attr(m, 1, "res", L_me_res);
        snprintf(tw, sizeof tw, "%d", L_me_wins);
        ok &= add_attr(m, 1, "wins", tw);
        snprintf(tv, sizeof tv, "%d", L_me_vote);
        ok &= add_attr(m, 1, "vote", tv);
    }
    if (!ok)
        printf("online: lobby: an attribute was refused\n");
    L_me_dirty = L_attr_dirty = 0;
    memset(&uo, 0, sizeof uo);
    uo.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
    uo.LobbyModificationHandle = m;
    L_busy = 1;
    p_EOS_Lobby_UpdateLobby(g_lobby, &uo, NULL, on_lobby_updated);
    p_EOS_LobbyModification_Release(m);
}

/* ---- notifications: any lobby of ours changed */

static void EOS_CALL on_lobby_update(const EOS_Lobby_LobbyUpdateReceivedCallbackInfo *d)
{
    if (d->LobbyId && !strcmp(d->LobbyId, L_id))
        lobby_refresh();
}

static void EOS_CALL on_member_update(const EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo *d)
{
    if (d->LobbyId && !strcmp(d->LobbyId, L_id))
        lobby_refresh();
}

static void EOS_CALL on_member_status(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo *d)
{
    char who[40], mine[40];

    if (!d->LobbyId || strcmp(d->LobbyId, L_id) != 0)
        return;
    id_string(d->TargetUserId, who, sizeof who);
    my_id(mine, sizeof mine);
    if (d->CurrentStatus == EOS_LMS_CLOSED) {
        printf("online: lobby: closed\n");
        lobby_reset(MPLOBBY_EV_CLOSED);
        return;
    }
    if (!strcmp(who, mine) && d->CurrentStatus == EOS_LMS_KICKED) {
        printf("online: lobby: kicked out\n");
        lobby_reset(MPLOBBY_EV_KICKED);
        return;
    }
    lobby_refresh();
    if (d->CurrentStatus == EOS_LMS_PROMOTED && !strcmp(who, mine)) {
        /* the new owner writes its name as host, and the lobby's own
         * fields as they are: what it would write is still empty */
        copy_str(L_w_name, sizeof L_w_name, L_info.name);
        copy_str(L_w_rules, sizeof L_w_rules, L_info.rules);
        copy_str(L_w_key, sizeof L_w_key, L_info.key);
        copy_str(L_w_room, sizeof L_w_room, L_info.room);
        copy_str(L_w_q, sizeof L_w_q, L_info_q);
        copy_str(L_w_go, sizeof L_w_go, "");
        L_w_priv = L_info.priv;
        if (!L_w_key[0])
            make_key(L_w_key);
        L_attr_dirty = 1;
    }
}

static void EOS_CALL on_invite_received(const EOS_Lobby_LobbyInviteReceivedCallbackInfo *d);
static void make_key(char *out);

static void lobby_notifies(void)
{
    EOS_Lobby_AddNotifyLobbyInviteReceivedOptions iv;

    EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions a;
    EOS_Lobby_AddNotifyLobbyMemberUpdateReceivedOptions b;
    EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions c;

    memset(&a, 0, sizeof a);
    a.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;
    p_EOS_Lobby_AddNotifyLobbyUpdateReceived(g_lobby, &a, NULL, on_lobby_update);
    memset(&b, 0, sizeof b);
    b.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERUPDATERECEIVED_API_LATEST;
    p_EOS_Lobby_AddNotifyLobbyMemberUpdateReceived(g_lobby, &b, NULL, on_member_update);
    memset(&c, 0, sizeof c);
    c.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;
    p_EOS_Lobby_AddNotifyLobbyMemberStatusReceived(g_lobby, &c, NULL, on_member_status);
    memset(&iv, 0, sizeof iv);
    iv.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYINVITERECEIVED_API_LATEST;
    p_EOS_Lobby_AddNotifyLobbyInviteReceived(g_lobby, &iv, NULL, on_invite_received);
}

/* ---- create, join, leave */

static void EOS_CALL on_lobby_created(const EOS_Lobby_CreateLobbyCallbackInfo *d)
{
    if (L_state != MPLOBBY_CREATING) {  /* left before it was made */
        if (d->ResultCode == EOS_Success) {
            EOS_Lobby_DestroyLobbyOptions o;
            memset(&o, 0, sizeof o);
            o.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
            o.LocalUserId = g_me;
            o.LobbyId = d->LobbyId;
            p_EOS_Lobby_DestroyLobby(g_lobby, &o, NULL, on_destroyed_quiet);
        }
        return;
    }
    if (d->ResultCode != EOS_Success) {
        printf("online: lobby not created: %s\n", res(d->ResultCode));
        lobby_reset(MPLOBBY_EV_FAILED);
        return;
    }
    copy_str(L_id, sizeof L_id, d->LobbyId);
    L_state = MPLOBBY_IN;
    L_event = MPLOBBY_EV_NONE;
    L_attr_dirty = L_me_dirty = 1;
    printf("online: lobby created (%s)\n", L_w_priv ? L_w_key : L_w_name);
    lobby_refresh();
    lobby_flush();
}

static void EOS_CALL on_lobby_joined(const EOS_Lobby_JoinLobbyByIdCallbackInfo *d)
{
    if (L_state != MPLOBBY_JOINING) {   /* left before it answered */
        if (d->ResultCode == EOS_Success) {
            EOS_Lobby_LeaveLobbyOptions o;
            memset(&o, 0, sizeof o);
            o.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
            o.LocalUserId = g_me;
            o.LobbyId = d->LobbyId;
            p_EOS_Lobby_LeaveLobby(g_lobby, &o, NULL, on_left_quiet);
        }
        return;
    }
    if (d->ResultCode != EOS_Success) {
        printf("online: lobby not joined: %s\n", res(d->ResultCode));
        lobby_reset(d->ResultCode == EOS_Lobby_TooManyPlayers ? MPLOBBY_EV_FULL
                    : d->ResultCode == EOS_NotFound ? MPLOBBY_EV_NOT_FOUND
                    : MPLOBBY_EV_FAILED);
        return;
    }
    copy_str(L_id, sizeof L_id, d->LobbyId);
    L_state = MPLOBBY_IN;
    L_event = MPLOBBY_EV_NONE;
    L_me_dirty = 1;
    printf("online: lobby joined\n");
    lobby_refresh();
    lobby_flush();
}

/* Six letters and digits for a private lobby (no 0/O, 1/I) */
static void make_key(char *out)
{
    static const char abc[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static unsigned seed;
    char mine[40];
    int i;

    my_id(mine, sizeof mine);
    if (!seed)
        for (i = 0; mine[i]; i++)
            seed = seed * 31u + (unsigned char)mine[i];
    seed ^= ticks();
    for (i = 0; i < 6; i++) {
        seed = seed * 1103515245u + 12345u;
        out[i] = abc[(seed >> 16) % (sizeof abc - 1)];
    }
    out[6] = 0;
}

int mplobby_create(const char *name, const char *rules, int priv, int max_members)
{
    EOS_Lobby_CreateLobbyOptions o;

    if (g_state != MPEOS_READY) {
        L_event = MPLOBBY_EV_OFFLINE;
        return 0;
    }
    if (L_state != MPLOBBY_NONE)
        return 0;
    if (max_members < 2)
        max_members = 2;
    if (max_members > MPLOBBY_MAX_MEMBERS)
        max_members = MPLOBBY_MAX_MEMBERS;
    copy_str(L_w_name, sizeof L_w_name, name);
    copy_str(L_w_rules, sizeof L_w_rules, rules);
    L_w_priv = priv != 0;
    L_w_max = max_members;
    L_w_go[0] = 0;
    L_w_q[0] = 0;                       /* the owner's line, made as members come */
    L_go_n = 0;
    if (L_next_key[0]) {                /* the host's own (mplobby_set_key) */
        copy_str(L_w_key, sizeof L_w_key, L_next_key);
        L_next_key[0] = 0;
    } else
        make_key(L_w_key);
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    o.LocalUserId = g_me;
    o.MaxLobbyMembers = (uint32_t)max_members;
    o.PermissionLevel = EOS_LPL_PUBLICADVERTISED;   /* private: hidden by "priv" */
    o.bPresenceEnabled = EOS_FALSE;
    o.BucketId = LOBBY_BUCKET;
    o.bDisableHostMigration = EOS_FALSE;            /* the owner may leave */
    o.bEnableJoinById = EOS_TRUE;
    L_state = MPLOBBY_CREATING;
    L_event = MPLOBBY_EV_NONE;
    L_serial++;
    p_EOS_Lobby_CreateLobby(g_lobby, &o, NULL, on_lobby_created);
    return 1;
}

int mplobby_join(const char *id)
{
    EOS_Lobby_JoinLobbyByIdOptions o;

    if (g_state != MPEOS_READY) {
        L_event = MPLOBBY_EV_OFFLINE;
        return 0;
    }
    if (L_state != MPLOBBY_NONE || !id || !id[0])
        return 0;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_JOINLOBBYBYID_API_LATEST;
    o.LobbyId = id;
    o.LocalUserId = g_me;
    o.bPresenceEnabled = EOS_FALSE;
    L_state = MPLOBBY_JOINING;
    L_event = MPLOBBY_EV_NONE;
    L_serial++;
    p_EOS_Lobby_JoinLobbyById(g_lobby, &o, NULL, on_lobby_joined);
    return 1;
}

static void EOS_CALL on_lobby_left(const EOS_Lobby_LeaveLobbyCallbackInfo *d)
{
    if (d->ResultCode != EOS_Success)
        printf("online: lobby leave: %s\n", res(d->ResultCode));
    if (L_state == MPLOBBY_LEAVING)
        lobby_reset(MPLOBBY_EV_LEFT);
}

static void EOS_CALL on_lobby_destroyed(const EOS_Lobby_DestroyLobbyCallbackInfo *d)
{
    if (d->ResultCode != EOS_Success)
        printf("online: lobby destroy: %s\n", res(d->ResultCode));
    if (L_state == MPLOBBY_LEAVING)
        lobby_reset(MPLOBBY_EV_LEFT);
}

/* The owner alone destroys the lobby; anyone else leaves it (an owner with
 * others in it too: EOS makes one of them the owner). */
void mplobby_leave(void)
{
    if (L_state == MPLOBBY_CREATING || L_state == MPLOBBY_JOINING) {
        lobby_reset(MPLOBBY_EV_LEFT);   /* the answer is undone when it comes */
        return;
    }
    if (L_state != MPLOBBY_IN)
        return;
    L_state = MPLOBBY_LEAVING;
    L_serial++;
    if (mplobby_am_owner() && L_nmem <= 1) {
        EOS_Lobby_DestroyLobbyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
        o.LocalUserId = g_me;
        o.LobbyId = L_id;
        p_EOS_Lobby_DestroyLobby(g_lobby, &o, NULL, on_lobby_destroyed);
    } else {
        EOS_Lobby_LeaveLobbyOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
        o.LocalUserId = g_me;
        o.LobbyId = L_id;
        p_EOS_Lobby_LeaveLobby(g_lobby, &o, NULL, on_lobby_left);
    }
}

/* ---- search */

static void EOS_CALL on_lobby_search(const EOS_LobbySearch_FindCallbackInfo *d)
{
    EOS_HLobbySearch s = (EOS_HLobbySearch)d->ClientData;
    EOS_LobbySearch_GetSearchResultCountOptions c;
    uint32_t i, n;
    char mine[40];

    if (L_search != 1 || s != L_search_h)
        goto done;
    L_search = 0;
    L_nres = 0;
    L_serial++;
    if (d->ResultCode != EOS_Success) {
        if (d->ResultCode != EOS_NotFound)
            printf("online: lobby search: %s\n", res(d->ResultCode));
        goto done;
    }
    my_id(mine, sizeof mine);
    memset(&c, 0, sizeof c);
    c.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
    n = p_EOS_LobbySearch_GetSearchResultCount(s, &c);
    for (i = 0; i < n && L_nres < MPLOBBY_MAX_RESULTS; i++) {
        EOS_LobbySearch_CopySearchResultByIndexOptions ci;
        EOS_HLobbyDetails det = NULL;
        struct mplobby_info *r = &L_res[L_nres];

        memset(&ci, 0, sizeof ci);
        ci.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
        ci.LobbyIndex = i;
        if (p_EOS_LobbySearch_CopySearchResultByIndex(s, &ci, &det) != EOS_Success)
            continue;
        read_info(det, r);
        p_EOS_LobbyDetails_Release(det);
        if (!r->id[0] || !strcmp(r->owner, mine) || !strcmp(r->id, L_id))
            continue;                   /* our own */
        L_nres++;
    }
done:
    p_EOS_LobbySearch_Release(s);
}

static void search_param(EOS_HLobbySearch s, const char *key, const char *val,
                         int64_t num, EOS_EComparisonOp op)
{
    EOS_LobbySearch_SetParameterOptions sp;
    EOS_Lobby_AttributeData a;

    memset(&a, 0, sizeof a);
    a.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    a.Key = key;
    if (val) {
        a.Value.AsUtf8 = val;
        a.ValueType = EOS_AT_STRING;
    } else {
        a.Value.AsInt64 = num;
        a.ValueType = EOS_AT_INT64;
    }
    memset(&sp, 0, sizeof sp);
    sp.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
    sp.Parameter = &a;
    sp.ComparisonOp = op;
    p_EOS_LobbySearch_SetParameter(s, &sp);
}

void mplobby_search(const char *rules, const char *key)
{
    EOS_Lobby_CreateLobbySearchOptions o;
    EOS_LobbySearch_FindOptions fo;
    EOS_HLobbySearch s = NULL;
    char k[MPLOBBY_KEY_MAX + 4];

    L_nres = 0;
    L_search = 0;
    L_search_h = NULL;
    L_serial++;
    if (g_state != MPEOS_READY)
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    o.MaxResults = MPLOBBY_MAX_RESULTS;
    if (p_EOS_Lobby_CreateLobbySearch(g_lobby, &o, &s) != EOS_Success)
        return;
    search_param(s, EOS_LOBBY_SEARCH_BUCKET_ID, LOBBY_BUCKET, 0, EOS_CO_EQUAL);
    search_param(s, "ver", MPLOBBY_PROTO, 0, EOS_CO_EQUAL);
    if (key && key[0]) {
        mplobby_key_clean(key, k, sizeof k);
        search_param(s, "key", k, 0, EOS_CO_EQUAL);
    } else {
        search_param(s, "priv", "0", 0, EOS_CO_EQUAL);
        search_param(s, EOS_LOBBY_SEARCH_MINSLOTSAVAILABLE, NULL, 1, EOS_CO_GREATERTHANOREQUAL);
    }
    if (rules && rules[0])
        search_param(s, "rules", rules, 0, EOS_CO_EQUAL);
    memset(&fo, 0, sizeof fo);
    fo.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
    fo.LocalUserId = g_me;
    L_search = 1;
    L_search_h = s;
    p_EOS_LobbySearch_Find(s, &fo, s, on_lobby_search);
}

int mplobby_results(struct mplobby_info *out, int max)
{
    int i;

    if (L_search == 1)
        return -1;
    for (i = 0; i < L_nres && i < max; i++)
        out[i] = L_res[i];
    return i;
}

/* ---- the lobby we are in */

int mplobby_state(void) { return L_state; }
int mplobby_event(void) { return L_event; }
unsigned mplobby_serial(void) { return L_serial; }

const struct mplobby_info *mplobby_info(void)
{
    return L_state == MPLOBBY_IN ? &L_info : NULL;
}

int mplobby_members(struct mplobby_member *out, int max)
{
    int i;

    if (L_state != MPLOBBY_IN)
        return 0;
    for (i = 0; i < L_nmem && i < max; i++)
        out[i] = L_mem[i];
    return i;
}

int mplobby_am_owner(void)
{
    char mine[40];

    if (L_state != MPLOBBY_IN || !L_info.owner[0])
        return 0;
    my_id(mine, sizeof mine);
    return !strcmp(mine, L_info.owner);
}

void mplobby_set_me(const char *code, const char *name, const char *look, int ready)
{
    if (strcmp(L_me_code, code ? code : "") || strcmp(L_me_name, name ? name : "")
        || strcmp(L_me_look, look ? look : "") || L_me_ready != (ready != 0)) {
        int host = strcmp(L_me_name, name ? name : "") != 0;
        copy_str(L_me_code, sizeof L_me_code, code);
        copy_str(L_me_name, sizeof L_me_name, name);
        copy_str(L_me_look, sizeof L_me_look, look);
        L_me_ready = ready != 0;
        L_me_dirty = 1;
        if (host)
            L_attr_dirty = 1;           /* the owner's name is the lobby's "host" */
    }
}

void mplobby_set_ready(int ready)
{
    if (L_me_ready != (ready != 0)) {
        L_me_ready = ready != 0;
        L_me_dirty = 1;
    }
}

void mplobby_set_rules(const char *rules)
{
    if (strcmp(L_w_rules, rules ? rules : "")) {
        copy_str(L_w_rules, sizeof L_w_rules, rules);
        L_attr_dirty = 1;
    }
}

void mplobby_set_name(const char *name)
{
    if (strcmp(L_w_name, name ? name : "")) {
        copy_str(L_w_name, sizeof L_w_name, name);
        L_attr_dirty = 1;
    }
}

static void EOS_CALL on_kicked(const EOS_Lobby_KickMemberCallbackInfo *d)
{
    if (d->ResultCode != EOS_Success)
        printf("online: lobby kick: %s\n", res(d->ResultCode));
    lobby_refresh();
}

static void EOS_CALL on_promoted(const EOS_Lobby_PromoteMemberCallbackInfo *d)
{
    if (d->ResultCode != EOS_Success)
        printf("online: lobby promote: %s\n", res(d->ResultCode));
    lobby_refresh();
}

void mplobby_kick(const char *who)
{
    EOS_Lobby_KickMemberOptions o;

    if (!mplobby_am_owner() || !who || !who[0])
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_KICKMEMBER_API_LATEST;
    o.LobbyId = L_id;
    o.LocalUserId = g_me;
    o.TargetUserId = p_EOS_ProductUserId_FromString(who);
    if (o.TargetUserId)
        p_EOS_Lobby_KickMember(g_lobby, &o, NULL, on_kicked);
}

void mplobby_promote(const char *who)
{
    EOS_Lobby_PromoteMemberOptions o;

    if (!mplobby_am_owner() || !who || !who[0])
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_PROMOTEMEMBER_API_LATEST;
    o.LobbyId = L_id;
    o.LocalUserId = g_me;
    o.TargetUserId = p_EOS_ProductUserId_FromString(who);
    if (o.TargetUserId)
        p_EOS_Lobby_PromoteMember(g_lobby, &o, NULL, on_promoted);
}

void mplobby_call(const char *a, const char *b, int stage)
{
    if (!mplobby_am_owner() || !a || !a[0] || !b || !b[0] || !strcmp(a, b))
        return;
    /* the clock in it: a promoted owner's first call differs from the old one's */
    snprintf(L_w_go, sizeof L_w_go, "%u.%d:%s:%s:%d", ticks() & 0xffffu, ++L_go_n, a, b, stage);
    L_attr_dirty = 1;
}

int mplobby_go_parts(const char *go, char *n, int nn, char *a, int an, char *b, int bn)
{
    const char *c1 = go ? strchr(go, ':') : NULL, *c2 = c1 ? strchr(c1 + 1, ':') : NULL;

    if (!c1 || !c2 || c1 == go || c2 == c1 + 1 || !c2[1])
        return 0;
    snprintf(n, (size_t)nn, "%.*s", (int)(c1 - go), go);
    snprintf(a, (size_t)an, "%.*s", (int)(c2 - c1 - 1), c1 + 1);
    snprintf(b, (size_t)bn, "%.*s", (int)strcspn(c2 + 1, ":"), c2 + 1);
    return 1;
}

int mplobby_go_stage(const char *go)
{
    const char *c = go ? strchr(go, ':') : NULL;
    int i;

    for (i = 0; c && i < 2; i++)
        c = strchr(c + 1, ':');
    return c && c[1] ? atoi(c + 1) : -1;
}

void mplobby_vote(int stage)
{
    if (L_me_vote != stage) {
        L_me_vote = stage;
        L_me_dirty = 1;
    }
}

void mplobby_set_room(const char *opts)
{
    if (strcmp(L_w_room, opts ? opts : "")) {
        copy_str(L_w_room, sizeof L_w_room, opts);
        L_attr_dirty = 1;
    }
}

void mplobby_set_line(const char *line)
{
    if (strcmp(L_w_q, line ? line : "")) {
        copy_str(L_w_q, sizeof L_w_q, line);
        L_attr_dirty = 1;
    }
}

const char *mplobby_room(void)
{
    return mplobby_am_owner() && L_w_room[0] ? L_w_room : L_info.room;
}

const char *mplobby_line(void)
{
    return mplobby_am_owner() ? L_w_q : L_info_q;
}

void mplobby_want(const char *act)
{
    if (L_state != MPLOBBY_IN || !act || !act[0])
        return;
    snprintf(L_me_want, sizeof L_me_want, "%u.%d:%s", ticks() & 0xffffu, ++L_want_n, act);
    L_me_dirty = 1;
}

void mplobby_report(const char *r)
{
    if (L_state != MPLOBBY_IN || !L_my_go[0] || !r || !r[0])
        return;
    snprintf(L_me_res, sizeof L_me_res, "%s:%s", L_my_go, r);
    if (r[0] == 'W')
        L_me_wins++;
    L_me_dirty = 1;
    printf("online: lobby: the fight of call %s ended %s\n", L_my_go, r);
}

void mplobby_leave_now(void)
{
    unsigned t0;

    if (L_state != MPLOBBY_IN || !g_plat)
        return;
    mplobby_leave();
    t0 = ticks();
    while (L_state == MPLOBBY_LEAVING && (int)(ticks() - t0) < 1000) {
        p_EOS_Platform_Tick(g_plat);
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    printf("online: lobby: left on closing (%s)\n", L_state == MPLOBBY_NONE ? "done" : "no answer yet");
}

void mplobby_dissolve(void)
{
    EOS_Lobby_DestroyLobbyOptions o;

    if (!mplobby_am_owner())
        return;
    L_state = MPLOBBY_LEAVING;
    L_serial++;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
    o.LocalUserId = g_me;
    o.LobbyId = L_id;
    printf("online: lobby: dissolved by its owner\n");
    p_EOS_Lobby_DestroyLobby(g_lobby, &o, NULL, on_lobby_destroyed);
}

int mplobby_take_call(char *code, int cn, char *who, int wn)
{
    if (!L_call || L_state != MPLOBBY_IN)
        return 0;
    L_call = 0;
    copy_str(code, cn, L_call_code);
    copy_str(who, wn, L_call_who);
    return 1;
}

/* ---- public / private, invites (port; stage 3, Mary 2026-11-12) */

/* The key as the host types it: letters and digits, upper case,
 * MPLOBBY_KEY_MAX at most (Mary: a key of their own). */
void mplobby_key_clean(const char *in, char *out, int n)
{
    int i, j = 0;

    for (i = 0; in && in[i] && j < MPLOBBY_KEY_MAX && j < n - 1; i++) {
        int c = (unsigned char)in[i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
            out[j++] = (char)c;
    }
    out[j] = 0;
}

void mplobby_set_key(const char *key)
{
    char k[MPLOBBY_KEY_MAX + 4];

    mplobby_key_clean(key, k, sizeof k);
    if ((int)strlen(k) < MPLOBBY_KEY_MIN)
        k[0] = 0;                       /* too short: one made up */
    if (L_state == MPLOBBY_NONE || L_state == MPLOBBY_CREATING) {
        copy_str(L_next_key, sizeof L_next_key, k);
        return;
    }
    if (L_state != MPLOBBY_IN || !mplobby_am_owner() || !k[0] || !strcmp(k, L_w_key))
        return;
    copy_str(L_w_key, sizeof L_w_key, k);
    L_attr_dirty = 1;
}

void mplobby_set_priv(int priv)
{
    if (L_w_priv != (priv != 0)) {
        L_w_priv = priv != 0;
        if (!L_w_key[0])
            make_key(L_w_key);
        L_attr_dirty = 1;
    }
}

/* An invite asked for: the player's code is looked up as FIND MATCH looks
 * it up (their ONLINE screen open), then the invite goes to the user found
 * over P2P, mp_net's channel (lobby_invite_tick, from mpeos_tick): a packet
 * "U3" MPLOBBY_INV_KIND 1, then the lobby's id, the host's name and the
 * lobby's, each ending in 0; mp_net.c hands it to mplobby_invite_got.
 * EOS_Lobby_SendInvite answered EOS_AccessDenied with this product's client
 * policy (2026-11-12). */
static char L_inv_code[16];
static int  L_inv_state;                /* 0 none, 1 looking, 3 sending, 2 sent, -1 failed */

static void EOS_CALL on_invite_sent(const EOS_Lobby_SendInviteCallbackInfo *d)
{
    L_inv_state = d->ResultCode == EOS_Success ? 2 : -1;
    printf("online: lobby: invite to %s %s\n", L_inv_code,
           d->ResultCode == EOS_Success ? "sent" : res(d->ResultCode));
    L_serial++;
}

void mplobby_invite(const char *code)
{
    if (L_state != MPLOBBY_IN || !code || !code[0])
        return;
    copy_str(L_inv_code, sizeof L_inv_code, code);
    L_inv_state = 1;
    mpeos_lookup(code);
    L_serial++;
}

int mplobby_invite_status(void) { return L_inv_state; }

static void lobby_invite_tick(void)
{
    char who[40];
    int r;

    if (L_inv_state != 1)
        return;
    if (L_state != MPLOBBY_IN) {
        L_inv_state = -1;
        return;
    }
    r = mpeos_lookup_result(who, sizeof who);
    if (r < 0) {
        L_inv_state = -1;
        printf("online: lobby: %s not found to invite\n", L_inv_code);
        L_serial++;
    } else if (r == 1) {
        unsigned char b[4 + 128 + 64 + 48];
        int n = 4, k;
        b[0] = 'U'; b[1] = '3'; b[2] = MPLOBBY_INV_KIND; b[3] = 1;
        n += snprintf((char *)b + n, sizeof b - (size_t)n, "%s", L_id) + 1;
        n += snprintf((char *)b + n, sizeof b - (size_t)n, "%s", L_me_name) + 1;
        n += snprintf((char *)b + n, sizeof b - (size_t)n, "%s", L_info.name) + 1;
        if (n > (int)sizeof b)
            n = (int)sizeof b;
        for (k = 0; k < 3; k++)         /* unreliable: three times, kept once */
            mpeos_send(who, b, n);
        L_inv_state = 2;
        printf("online: lobby: invite sent to %s\n", L_inv_code);
        L_serial++;
        (void)on_invite_sent;
    }
}

/* the invites this game was sent, newest last */
#define L_MAX_INV 8
static struct mplobby_invite L_inv[L_MAX_INV];
static int L_ninv;

static void invite_store(const char *invite_id, const char *lobby, const char *from,
                         const char *name)
{
    int i;

    if (!lobby || !lobby[0] || !strcmp(lobby, L_id))
        return;
    for (i = 0; i < L_ninv; i++)        /* the same lobby again: the newer one */
        if (!strcmp(L_inv[i].lobby, lobby))
            break;
    if (i < L_ninv && !strcmp(L_inv[i].from, from ? from : ""))
        return;                         /* a copy (the packet comes three times) */
    if (i == L_ninv) {
        if (L_ninv == L_MAX_INV) {
            memmove(L_inv, L_inv + 1, sizeof L_inv[0] * (L_MAX_INV - 1));
            L_ninv--;
        }
        i = L_ninv++;
    }
    copy_str(L_inv[i].id, sizeof L_inv[i].id, invite_id);
    copy_str(L_inv[i].lobby, sizeof L_inv[i].lobby, lobby);
    copy_str(L_inv[i].from, sizeof L_inv[i].from, from);
    copy_str(L_inv[i].name, sizeof L_inv[i].name, name);
    L_inv[i].seen = 0;
    printf("online: lobby: invited to %s by %s\n", name ? name : "", from ? from : "");
    L_serial++;
}

void mplobby_invite_got(const unsigned char *p, int n)
{
    char t[4 + 128 + 64 + 48 + 3];
    const char *lobby, *from, *name;

    if (n <= 4 || n >= (int)sizeof t)
        return;
    memcpy(t, p, (size_t)n);
    t[n] = t[n + 1] = t[n + 2] = 0;
    lobby = t + 4;
    from = lobby + strlen(lobby) + 1;
    name = from + strlen(from) + 1;
    invite_store("", lobby, from, name);
}

static void EOS_CALL on_invite_received(const EOS_Lobby_LobbyInviteReceivedCallbackInfo *d)
{
    EOS_Lobby_CopyLobbyDetailsHandleByInviteIdOptions o;
    EOS_HLobbyDetails h = NULL;
    struct mplobby_info in;

    if (!d->InviteId)
        return;
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLEBYINVITEID_API_LATEST;
    o.InviteId = d->InviteId;
    if (p_EOS_Lobby_CopyLobbyDetailsHandleByInviteId(g_lobby, &o, &h) != EOS_Success || !h)
        return;
    read_info(h, &in);
    p_EOS_LobbyDetails_Release(h);
    invite_store(d->InviteId, in.id, in.host, in.name);
}

int mplobby_invites(struct mplobby_invite *out, int max)
{
    int i;

    for (i = 0; i < L_ninv && i < max; i++)
        out[i] = L_inv[i];
    for (i = 0; i < L_ninv; i++)
        L_inv[i].seen = 1;
    return L_ninv < max ? L_ninv : max;
}

void mplobby_drop_invite(int i)
{
    if (i < 0 || i >= L_ninv)
        return;
    memmove(L_inv + i, L_inv + i + 1, sizeof L_inv[0] * (size_t)(L_ninv - i - 1));
    L_ninv--;
    L_serial++;
}

int mplobby_accept_invite(int i)
{
    char id[128];

    if (i < 0 || i >= L_ninv || L_state != MPLOBBY_NONE)
        return 0;
    copy_str(id, sizeof id, L_inv[i].lobby);
    mplobby_drop_invite(i);
    return mplobby_join(id);
}

/* --------------------------------------------------------------- login */

static void logged_in(EOS_ProductUserId u)
{
    EOS_P2P_AddNotifyPeerConnectionRequestOptions n;
    char s[40];

    g_me = u;
    g_state = MPEOS_READY;
    id_string(u, s, sizeof s);
    printf("online: EOS ready (%s)\n", s);
    memset(&n, 0, sizeof n);
    n.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
    n.LocalUserId = g_me;
    n.SocketId = &g_socket;
    p_EOS_P2P_AddNotifyPeerConnectionRequest(g_p2p, &n, NULL, on_request);
    lobby_notifies();
    lobby_step();
    here_step();
    if (g_look_pending)
        look_now();
}

static void fail(const char *what, EOS_EResult r)
{
    printf("online: EOS %s: %s; internet play is off\n", what, res(r));
    g_state = MPEOS_FAILED;
    if (g_look_pending) {
        g_look_pending = 0;
        g_look_result = -1;
    }
}

static void EOS_CALL on_user(const EOS_Connect_CreateUserCallbackInfo *d)
{
    if (d->ResultCode == EOS_Success)
        logged_in(d->LocalUserId);
    else
        fail("create user", d->ResultCode);
}

static void EOS_CALL on_login(const EOS_Connect_LoginCallbackInfo *d)
{
    if (d->ResultCode == EOS_Success) {
        logged_in(d->LocalUserId);
    } else if (d->ResultCode == EOS_InvalidUser && d->ContinuanceToken) {
        /* first time on this computer: make the user */
        EOS_Connect_CreateUserOptions o;
        memset(&o, 0, sizeof o);
        o.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
        o.ContinuanceToken = d->ContinuanceToken;
        p_EOS_Connect_CreateUser(g_conn, &o, NULL, on_user);
    } else {
        fail("login", d->ResultCode);
    }
}

static void login(void)
{
    static EOS_Connect_Credentials c;
    static EOS_Connect_UserLoginInfo u;
    EOS_Connect_LoginOptions o;

    memset(&c, 0, sizeof c);
    c.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
    c.Token = NULL;
    c.Type = EOS_ECT_DEVICEID_ACCESS_TOKEN;
    memset(&u, 0, sizeof u);
    u.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
    u.DisplayName = "UMK3 Player";
    memset(&o, 0, sizeof o);
    o.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
    o.Credentials = &c;
    o.UserLoginInfo = &u;
    p_EOS_Connect_Login(g_conn, &o, NULL, on_login);
}

static void EOS_CALL on_device(const EOS_Connect_CreateDeviceIdCallbackInfo *d)
{
    /* DuplicateNotAllowed: this computer has its device id already */
    if (d->ResultCode == EOS_Success || d->ResultCode == EOS_DuplicateNotAllowed)
        login();
    else
        fail("device id", d->ResultCode);
}

/* UMK3_EOS_NEW_DEVICE: the old id is gone, make the new one */
static void EOS_CALL on_deleted(const EOS_Connect_DeleteDeviceIdCallbackInfo *d)
{
    EOS_Connect_CreateDeviceIdOptions dv;

    (void)d;
    memset(&dv, 0, sizeof dv);
    dv.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    dv.DeviceModel = "PC";
    p_EOS_Connect_CreateDeviceId(g_conn, &dv, NULL, on_device);
}

/* The SDK's memory comes from a heap of its own, above the game's: the 64-bit
 * game keeps its memory below 4 GB in arenas of its own (savestate.c,
 * lime_platform.c), and nothing of the SDK's should land in or near them. */
#ifdef _WIN32
static HANDLE g_eos_heap;
static void *EOS_MEMORY_CALL eos_alloc(size_t n, size_t align)
{
    unsigned char *raw, *p;
    if (!g_eos_heap)
        g_eos_heap = HeapCreate(0, 0, 0);
    if (align < 16)
        align = 16;
    raw = (unsigned char *)HeapAlloc(g_eos_heap, 0, n + align + sizeof(void *));
    if (!raw)
        return NULL;
    p = (unsigned char *)(((uintptr_t)raw + sizeof(void *) + align - 1) & ~(uintptr_t)(align - 1));
    ((void **)p)[-1] = raw;
    return p;
}
static void EOS_MEMORY_CALL eos_release(void *p)
{
    if (p)
        HeapFree(g_eos_heap, 0, ((void **)p)[-1]);
}
static void *EOS_MEMORY_CALL eos_realloc(void *p, size_t n, size_t align)
{
    void *q;
    size_t old;
    if (!p)
        return eos_alloc(n, align);
    q = eos_alloc(n, align);
    if (!q)
        return NULL;
    old = HeapSize(g_eos_heap, 0, ((void **)p)[-1])
          - (size_t)((unsigned char *)p - (unsigned char *)((void **)p)[-1]);
    memcpy(q, p, old < n ? old : n);
    eos_release(p);
    return q;
}
#else
static void *EOS_MEMORY_CALL eos_alloc(size_t n, size_t align)
{
    void *p = NULL;
    if (align < sizeof(void *))
        align = sizeof(void *);
    return posix_memalign(&p, align, n) == 0 ? p : NULL;
}
static void EOS_MEMORY_CALL eos_release(void *p) { free(p); }
static void *EOS_MEMORY_CALL eos_realloc(void *p, size_t n, size_t align)
{
    (void)align;
    return realloc(p, n);
}
#endif

void mpeos_start(void)
{
    char dir[MAX_PATH], dll[MAX_PATH + 32], cache[MAX_PATH + 32];
    const char *sd;
    void *h;
    EOS_InitializeOptions io;
    EOS_Platform_Options po;
    EOS_Connect_CreateDeviceIdOptions dv;
    EOS_EResult r;

    if (g_tried)
        return;
    g_tried = 1;
    exe_dir(dir, sizeof dir);
    snprintf(dll, sizeof dll, "%s" EOS_LIB, dir);
    h = lib_open(dll);
    if (!h) {
        printf("online: no " EOS_LIB " beside the exe: internet play is off\n");
        return;
    }
#define LOOK(f) p_##f = (__typeof__(f) *)lib_sym(h, #f); \
    if (!p_##f) { printf("online: the EOS DLL has no " #f "\n"); return; }
    EOS_FUNCS(LOOK)
#undef LOOK
    if (!read_ini(dir))
        return;

    memset(&io, 0, sizeof io);
    io.ApiVersion = EOS_INITIALIZE_API_LATEST;
    io.AllocateMemoryFunction = eos_alloc;
    io.ReallocateMemoryFunction = eos_realloc;
    io.ReleaseMemoryFunction = eos_release;
    io.ProductName = "UMK3 PC port";
    io.ProductVersion = "1.2.59";
    r = p_EOS_Initialize(&io);
    if (r != EOS_Success && r != EOS_AlreadyConfigured) {
        fail("initialize", r);
        return;
    }
    p_EOS_Logging_SetCallback(on_log);
    p_EOS_Logging_SetLogLevel(EOS_LC_ALL_CATEGORIES, EOS_LOG_Warning);

    /* the cache beside the save folder, one per game (two on one PC) */
    sd = getenv("UMK3_SAVE_DIR");
    if (sd && sd[0])
        snprintf(cache, sizeof cache, "%s" SEP "eos", sd);
    else
        snprintf(cache, sizeof cache, "%ssave" SEP "eos", dir);
    make_dir(cache);

    memset(&po, 0, sizeof po);
    po.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
    po.ProductId = k_product;
    po.SandboxId = k_sandbox;
    po.DeploymentId = k_deploy;
    po.ClientCredentials.ClientId = k_id;
    po.ClientCredentials.ClientSecret = k_secret;
    po.bIsServer = EOS_FALSE;
    po.Flags = EOS_PF_DISABLE_OVERLAY;
    po.CacheDirectory = cache;
    g_plat = p_EOS_Platform_Create(&po);
    if (!g_plat) {
        fail("platform", EOS_UnexpectedError);
        return;
    }
    g_conn = p_EOS_Platform_GetConnectInterface(g_plat);
    g_lobby = p_EOS_Platform_GetLobbyInterface(g_plat);
    g_p2p = p_EOS_Platform_GetP2PInterface(g_plat);
    memset(&g_socket, 0, sizeof g_socket);
    g_socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
    snprintf(g_socket.SocketName, sizeof g_socket.SocketName, "%s", SOCKET_NAME);

    g_state = MPEOS_STARTING;
    /* Test aid: two games on one PC share the device id, so they would be
     * one EOS user and could not reach each other. UMK3_EOS_NEW_DEVICE=1
     * drops the stored id first and makes another (a game already running
     * keeps the one it logged in with). */
    sd = getenv("UMK3_EOS_NEW_DEVICE");
    if (sd && sd[0] == '1') {
        EOS_Connect_DeleteDeviceIdOptions del;
        memset(&del, 0, sizeof del);
        del.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;
        p_EOS_Connect_DeleteDeviceId(g_conn, &del, NULL, on_deleted);
        return;
    }
    memset(&dv, 0, sizeof dv);
    dv.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    dv.DeviceModel = "PC";
    p_EOS_Connect_CreateDeviceId(g_conn, &dv, NULL, on_device);
}

void mpeos_tick(void)
{
    if (g_plat) {
        p_EOS_Platform_Tick(g_plat);
        lobby_flush();
        lobby_invite_tick();
    }
}

int mpeos_state(void) { return g_state; }

#endif
