/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_STATUS_H
#define DM_UI_STATUS_H
/* Prefixes: status_ */

/* The champion status boxes at the top of the screen: name, bars, hands, shields, damage, the party
   icons; while the inventory of a champion is open, its portrait, name and title and its bars in the panel. */

#include <stdint.h>

/* Draws one slot: a hand in the status box, or any slot (and the chest slots) in the inventory of
   the champion. Empty body slots show their empty icon; wounded body slots get a red border, the
   action hand of the acting champion the acting border. */
void status_draw_slot(int16_t champion_index, int16_t slot);
/* Shows the damage a champion took: a splat with the number on the status box, or on the
   portrait while the inventory of the champion is open. */
void status_draw_damage(int16_t champion_index, int16_t damage); /* exported for the tests */
/* Draws the parts of a champion that have a redraw flag in its attributes, then clears the flags. */
void status_draw_champion(int16_t champion_index);
/* Sets redraw flags on all champions and draws them. */
void status_draw_all(uint16_t attributes);

#endif
