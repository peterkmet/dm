/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_DIALOG_H
#define DM_UI_DIALOG_H
/* Prefixes: dialog_ */
#include <stdbool.h>
#include <stdint.h>

/* The message box with one to four choices (KAOS dialog_draw, dialog_get_choice; DIALOG.C). */

#define DIALOG_NO_CHOICE 99

extern int16_t g_dialog_choice;     /* the clicked choice, 1-4, or DIALOG_NO_CHOICE */
extern bool g_game_screen_inactive; /* set: the dialog uses the whole screen (entrance, endgame) */

/* Draws the dialog in the viewport: two messages (NULL: none) and the choices (NULL ends them). */
void dialog_draw(const char *message1, const char *message2, const char *choice1, const char *choice2,
                 const char *choice3, const char *choice4);
/* Waits for a click on one of choice_count choices and shows the pressed button. Return selects
   a single choice. Returns the choice 1-4, or 0 when the wait ends without a choice (no frames,
   quit). */
int16_t dialog_get_choice(int16_t choice_count);

#endif
