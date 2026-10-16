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
    int      dark;                      /* the gradient: 0 white to the
                                           colour, 1 black to the colour */
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
 * (res/Textures/<NAME>_VERSUS.PNG) over the colour's glossy gradient, the
 * same as the launcher draws it. NULL when the art cannot be read. */
unsigned char *prof_icon_rgba(int icon, unsigned color, int dark, int S);

#endif
