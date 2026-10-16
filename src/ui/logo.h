#ifndef LOGO_H
#define LOGO_H
/* Prefixes: logo_ */

/* The FTL logo of SWSH.FTL (not KAOS): the original shows it before the DM title. */

/* Shows the logo with its sound until the sound ends. Returns -1 when the file cannot be read. */
int logo_play(const char *path);

#endif
