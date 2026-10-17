/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_DIALOG_H
#define DM_UI_DIALOG_H
/* Prefixes: dialog_ */

/* The message box with one to four choices: it draws the dialog in the viewport and waits for a click on a choice.
   It owns no game state. */

#include <stdbool.h>
#include <stdint.h>

/* g_dialog_choice before a click. */
#define DIALOG_NO_CHOICE 99

/* The messages and choices of a dialog. */
#define DIALOG_MESSAGE_COUNT 2
#define DIALOG_CHOICE_COUNT  4

/* The texts of a dialog: the messages (NULL: none) and the choices (NULL ends them). */
typedef struct {
    const char *messages[DIALOG_MESSAGE_COUNT];
    const char *choices[DIALOG_CHOICE_COUNT];
} DialogText;

extern int16_t g_dialog_choice; /* the clicked choice, 1-4, or DIALOG_NO_CHOICE */

/* Draws the dialog in the viewport: two messages (NULL: none) and the choices (NULL ends them). */
void dialog_draw(const DialogText *dialog);
/* Waits for a click on one of choice_count choices and shows the pressed button. Return selects
   a single choice. Returns the choice 1-4, or 0 when the wait ends without a choice (no frames,
   quit). */
int16_t dialog_get_choice(int16_t choice_count);

#endif
