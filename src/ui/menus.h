/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_MENUS_H
#define DM_UI_MENUS_H
/* Prefixes: menus_ */

/* The action area, the spell area and the movement arrows at the right of the screen. */

#include "engine/champion.h"
#include <stdbool.h>
#include <stdint.h>

/* MENU.C state. */
extern bool g_action_area_contains_icons;

/* Draws the movement arrows. */
void menus_draw_movement_arrows(void);
/* Opens the action menu of a champion: the actions of the object in the action hand. */
void menus_set_acting_champion(int16_t champion_index);
/* Closes the action menu and redraws the action hand of the champion. */
void menus_clear_acting_champion(void);
/* Draws the action hand object of a champion as a black icon on a cyan box (with the advanced
   features, in its own colors on the cyan box); dimmed while the action is disabled, a candidate is
   shown, or the party sleeps. */
void menus_draw_action_icon(int16_t champion_index);
/* Draws the action area: the action icons of all champions, or the action menu of the acting
   champion (its name and up to three action names). */
void menus_draw_action_area(void);
/* Shows the result of an action: "CAN'T REACH", "NEED AMMO", or the damage in a splat (smaller
   for small damage). */
void menus_draw_action_damage(int16_t damage);
/* Runs each tick: turns idle champions toward their last attacker, then redraws the action area
   when requested (menu, damage, or icons). */
void menus_refresh_action_area(void);
/* Draws the spell area of the caster again. A dead caster (the whole party died) is cleared and nothing is
   drawn. */
void menus_draw_spell_area(void);
/* Draws the six runes that the caster can choose at its symbol step. */
void menus_draw_available_symbols(int16_t symbol_step);
/* Draws the runes that the champion has chosen so far. */
void menus_draw_champion_symbols(int16_t champion_index);
/* Selects the spell caster and draws the spell area; -1 clears it. */
void menus_set_magic_caster(int16_t champion_index);
/* Draws the spell area parts that the rules changed, with caster as the record of the magic caster (NULL: its own). */
void menus_refresh_spell_area(const Champion *caster);
/* No acting champion, no action list, no action damage, and the action icons at the start of a game. */
void menus_reset(void);

#endif
