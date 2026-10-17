/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_UI_PANEL_H
#define DM_UI_PANEL_H
/* Prefixes: panel_ */

/* The inventory panel: slots, the eye and the mouth, food and water, skills and statistics, the object
   description, the scroll, the chest. */

#include <stdbool.h>
#include <stdint.h>

/* Inventory panel content (g_panel_content). */
#define PANEL_CONTENT_FOOD_WATER 1
#define PANEL_CONTENT_SCROLL     5
#define PANEL_CONTENT_CHEST      6
#define PANEL_CONTENT_RESURRECT  7
#define PANEL_CONTENT_NAME_ENTRY 8
#define PANEL_LINE_COUNT         3
#define PANEL_LINE_SIZE          40
#define STAMINA_DISPLAY_UNIT     10 /* the inventory shows stamina in tenths of the stored value */
#define WEIGHT_TENTHS            10 /* loads and object weights are in tenths of a kilogram */
extern int16_t g_panel_content;

/* Draws the inventory statistics: health, stamina, mana values, and the mouth and eye borders
   (red when hungry, thirsty, or poisoned, and when a statistic is below its maximum). */
void panel_draw_statistics(int16_t champion_index);
/* Draws the load line "LOAD nn.n/ mm KG": red above the maximum load, yellow above 5/8 of it. */
void panel_draw_load(int16_t champion_index);
/* The rank name that the eye panel shows for a skill level (NEOPHYTE ...), or NULL for level 1. */
const char *panel_get_rank_name(int16_t level);
/* Draws the eye panel: base skill levels above 1 (green after a level up), then the six
   statistics with current value (red below maximum, green above) and maximum value. */
void panel_draw_skills_statistics(void); /* exported for the tests */
/* Opens the inventory of a champion on the screen: dims the movement arrows and draws the
   inventory background, labels, all slots, and the champion state. Sets g_session.inventory_champion_index. */
void panel_open(int16_t champion_index);
/* Closes the inventory on the screen: the status box returns and the movement arrows come back.
   The caller redraws the dungeon view. */
void panel_close(int16_t champion_index);
/* The object panel text: the name (bones with the champion name, a potion with its power rune
   for a priest above level 1) and the description lines (burnt out, compass direction, waterskin
   level, attributes, weight). Returns the line count. */
int16_t panel_format_object_text(uint16_t thing, char *name, char lines[][PANEL_LINE_SIZE]);
/* The ATTACK (weapon) or DEFENSE (armour) meter of the advanced object panel: true with its label and value, else false. */
bool panel_get_object_meter(uint16_t thing, const char **label, uint16_t *value);
/* Draws the inventory panel: resurrect/reincarnate for a candidate, the open scroll or chest in
   the action hand, else food and water. */
void panel_draw(void);
/* Draws the name entry of a reincarnated champion from the engine state: the keyboard, the name and the title
   with underscores for the free characters, and the cursor at the input. */
void panel_draw_name_entry(void);
/* The panel while the eye is pressed: the leader hand object, or the skills and statistics.
   Deviation: the leader hand object name stays; the original clears it until the eye is released. */
void panel_draw_eye(void);
/* CSB, each tick with the inventory open: a scroll with a creature name or the magic map draws again. */
void panel_redraw_scroll(void);
/* A click in the open chest panel swaps the leader hand object with the clicked chest slot. */
void panel_click_in_chest(int16_t x, int16_t y);

#endif
