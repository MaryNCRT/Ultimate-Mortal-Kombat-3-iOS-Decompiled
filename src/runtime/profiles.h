/* profiles.h -- the player's profile (profiles.ini); see profiles.c. Port,
 * not in the binary. */
#ifndef UMK3_PROFILES_H
#define UMK3_PROFILES_H

#define PROF_NAME_MAX  24               /* characters, not bytes */
#define PROF_ICONS     24

struct profile {
    char     id[16];                    /* XXXX-XXXX-XXXX */
    char     name[PROF_NAME_MAX * 4 + 1];   /* UTF-8 */
    int      icon;                      /* 0..23, a fighter's bio art */
    unsigned color;                     /* 0xRRGGBB */
    int      dark;                      /* the gradient: 0 light (color2,
                                           white by default, into the
                                           colour), 1 dark (the colour into
                                           color2, black by default), 2
                                           custom (the colour to color2) */
    unsigned color2;                    /* 0xRRGGBB, the gradient's other end */
    int      angle;                     /* custom: a straight gradient from the
                                           colour to color2 at this angle, in
                                           degrees as CSS has it (0 up, 180
                                           down; the launcher's gradient
                                           picker); -1 the older custom look */
    int      alt;                       /* 1: the fighter in his second outfit
                                           (<NAME>_VERSUS2.pvr) */
};

/* profiles.ini in the folder of `ini_path` (umk3.ini's path); with none,
 * the profile is made with `default_name` and a new id, and saved. */
void prof_load(const char *ini_path, const char *default_name);
void prof_save(void);
struct profile *prof_get(void);         /* NULL before prof_load */
/* name, icon and colour back to the defaults; the id stays */
void prof_reset(const char *default_name);
/* A name's length in characters. */
int  prof_name_len(const char *s);

/* The icon at S x S, RGBA, malloc'd: the fighter's bio and endings art
 * (res/Textures/<NAME>_VERSUS.PNG; alt: the second outfit's
 * <NAME>_VERSUS2.pvr, framed the same) over the colour's glossy gradient
 * (angle: see struct profile), the same as the launcher draws it. NULL when
 * the art cannot be read. */
unsigned char *prof_icon_rgba(int icon, int alt, unsigned color, int dark,
                              unsigned color2, int angle, int S);
/* color2's default for a gradient: white, black, blood red. */
unsigned prof_shade_default(int dark);
/* Changes whenever the icon would look different. */
unsigned prof_look_key(const struct profile *p);

#endif
