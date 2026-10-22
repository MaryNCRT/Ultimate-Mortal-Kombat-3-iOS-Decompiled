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

#endif
