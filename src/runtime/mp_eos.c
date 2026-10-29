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
    X(EOS_P2P_AddNotifyPeerConnectionRequest)

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

void mpeos_advertise(const char *code)
{
    bucket_of(code, g_adv_want, sizeof g_adv_want);
    lobby_step();
}

void mpeos_unadvertise(void)
{
    g_adv_want[0] = 0;
    lobby_step();
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
    lobby_step();
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
    if (g_plat)
        p_EOS_Platform_Tick(g_plat);
}

int mpeos_state(void) { return g_state; }

#endif
