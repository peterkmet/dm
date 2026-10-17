/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_DIALOG_H
#define DM_UI_DIALOG_H
/* Prefixes: dialog_ */

/* The message box with one to four choices: it draws the dialog in the viewport and waits for a click on a choice.
   It owns no game state. */

#include "graphics.h"
#include <stdbool.h>
#include <stdint.h>

/* g_dialog_choice before a click. */
#define DIALOG_NO_CHOICE 99

/* The messages and choices of a dialog. */
#define DIALOG_MESSAGE_COUNT 2
#define DIALOG_CHOICE_COUNT  4

/* Frames of the pressed state of a clicked choice. */
#define DIALOG_PRESS_FRAMES 5

/* The texts of a dialog: the messages (NULL: none) and the choices (NULL ends them). */
typedef struct {
    const char *messages[DIALOG_MESSAGE_COUNT];
    const char *choices[DIALOG_CHOICE_COUNT];
} DialogText;

extern int16_t g_dialog_choice; /* the clicked choice, 1-4, or DIALOG_NO_CHOICE */

/* Shows the viewport bitmap: in its zone on the game screen, else centered on the inactive screen. */
void dialog_show_viewport(void);
/* Draws the dialog in the viewport: two messages (NULL: none) and the choices (NULL ends them). */
void dialog_draw(const DialogText *dialog);
/* The box in the viewport of a choice 1-4 of a dialog with choice_count choices. */
bool dialog_get_choice_box(int16_t choice_count, int16_t choice, Box *box);
/* Shows a button box of the viewport pressed: a raised frame, then after DIALOG_PRESS_FRAMES frames a pressed
   frame. */
void dialog_show_pressed(const Box *button);
/* Waits for a click on one of choice_count choices and shows the pressed button. Return selects
   a single choice. Returns the choice 1-4, or 0 when the wait ends without a choice (no frames,
   quit). */
int16_t dialog_get_choice(int16_t choice_count);

#endif
