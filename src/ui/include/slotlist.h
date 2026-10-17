/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_SLOTLIST_H
#define DM_UI_SLOTLIST_H
/* Prefixes: slotlist_ */

/* The slot list of the advanced mode (not in the original game): the save slots, and in the load list also the
   quick save, as rows in the dialog graphic with CANCEL. It reads the slot infos and waits for a click; it saves and
   loads nothing. */

#include "savegame.h"
#include <stdbool.h>
#include <stdint.h>

/* The quick save row of the load list, after the rows of the slots 0-7. */
#define SLOTLIST_QUICK_ROW SAVEGAME_SLOT_COUNT

/* Shows the save list (the slots in directory) or the load list (the slots, then the quick save) and waits for a row or
   CANCEL. Returns the slot 0-7, SLOTLIST_QUICK_ROW, or -1 for CANCEL, Escape, and a wait without frames. */
int16_t slotlist_choose(const char *directory, bool load);
/* A mouse button while the list is open: a left press on CANCEL or on a row that can be selected closes the list.
   Returns true when the list is open: it uses up every click. */
bool slotlist_process_click(int16_t x, int16_t y, int16_t kind);
/* A key while the list is open: Escape closes the list, the other keys do nothing. Returns true when the list is
   open: it uses up every key. */
bool slotlist_process_key(uint16_t key);

#endif
