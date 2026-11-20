/* fatal_trainer.h -- the FATALITY TRAINER (fatal_trainer.c), port only */
#ifndef FATAL_TRAINER_H
#define FATAL_TRAINER_H

#include <stdint.h>

/* a finisher's kind, in the moves list's order */
enum { FT_FATALITY1, FT_FATALITY2, FT_STAGE, FT_ANIMALITY, FT_MERCY,
       FT_FRIENDSHIP, FT_BABALITY, FT_KINDS };

#define FT_DMAX   480                   /* the farthest distance asked */
#define FT_MOVES  16

typedef struct {
    int kind;
    int button;                         /* which ochar_ table (fatal_trainer.c) */
    const uint32_t *entry;              /* its secret-move table entry */
} FT_MOVE;

typedef struct {
    int on;                             /* a fight, both fighters there */
    int me_x, him_x, d, side;           /* arcade units; side -1 left of him */
    int lo, hi;                         /* the first run of distances the game
                                         * accepts (hi FT_DMAX: no bound), lo
                                         * -1 when none now */
    int bound_lo, bound_hi;             /* the band the predicate measures
                                         * against, when it sets one */
    int any;                            /* every distance */
    int in;                             /* standing inside lo..hi */
    int now;                            /* the game would take it now */
    int window;                         /* the finishing window (G+0x45c 3) */
    int mercy;                          /* a mercy was given (G+0x45a) */
} FT_LIVE;

extern int PortFT, PortFTP1, PortFTP2, PortFTStage, PortFTMove;
extern int PortFTUnlimited, PortFTDistance, PortFTOneButton, PortFTInput;

int  ft_scan(int ch, FT_MOVE *out, int max);
void ft_tick(const FT_MOVE *m);
const FT_LIVE *ft_live(void);
int  ft_fire(const FT_MOVE *m);
void ft_draw3d(void);
const FT_MOVE *ft_moves_of(int ch, int *n);
const FT_MOVE *ft_selected(void);
extern int PortFTGo;

#endif
