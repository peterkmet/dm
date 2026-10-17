/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/party.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/champion.h"
#include "engine/game.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/record.h"
#include <string.h>

/* Party light and scent limits. */
#define LIGHT_SOURCE_COUNT     (CHAMPION_MAX_COUNT * 2)
#define LIGHT_SOURCES_USED     5 /* the brightest torches that add light */
#define LIGHT_FIRST_SHIFT      6
#define LIGHT_DARKEST          (PALETTE_LIGHT_THRESHOLD_COUNT - 1) /* palette index of the darkest light */
#define SCENT_STRENGTH_MAXIMUM 80

Party g_party;
int16_t g_party_map_x, g_party_map_y, g_party_direction, g_party_map_index;
int32_t g_last_party_movement_time;
int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks;
int16_t g_last_projectile_disabled_movement_direction;
int16_t g_party_light_level;

int16_t party_get_light(void)
{
    int16_t power[LIGHT_SOURCE_COUNT] = {0};
    int16_t n = 0;
    int16_t total = 0;
    int16_t shift = LIGHT_FIRST_SHIFT;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        for (int hand = SLOT_HAND_ACTION; hand >= SLOT_HAND_READY; hand--, n++) {
            uint16_t thing = g_champions[i].slots[hand];
            int16_t type = object_get_type(thing);
            if (type >= ICON_TORCH_UNLIT && type <= ICON_TORCH_LAST)
                power[n] = rec_weapon_charges(thing);
        }
    /* the original sorts only the 4 brightest: the fifth light is any of the rest */
    for (int i = 0; i < LIGHT_SOURCES_USED - 1; i++)
        for (int k = i + 1; k < LIGHT_SOURCE_COUNT; k++)
            if (power[i] < power[k]) {
                int16_t t = power[i];
                power[i] = power[k];
                power[k] = t;
            }
    for (int i = 0; i < LIGHT_SOURCES_USED; i++)
        if (power[i]) {
            /* the 16-bit cast keeps the overflow of the original word arithmetic */
            total += (int16_t)(tables_get_light_amount(power[i]) << shift) >> LIGHT_FIRST_SHIFT;
            shift = max16(0, shift - 1);
        }
    return g_party.magical_light_amount + total;
}

void party_set_light_level(int16_t map_index, int16_t light)
{
    g_party_light_level = 0;
    if (!rec_map_difficulty(map_index))
        return;
    if (light < 1) {
        g_party_light_level = LIGHT_DARKEST;
        return;
    }
    while (g_party_light_level < LIGHT_DARKEST && light < tables_get_light_threshold(g_party_light_level))
        g_party_light_level++;
}

void party_burn_torches(void)
{
    uint16_t count = g_party_champion_count - (g_session.candidate_champion_index >= 0 ? 1 : 0);
    for (uint16_t i = 0; i < count; i++)
        for (int hand = SLOT_HAND_ACTION; hand >= SLOT_HAND_READY; hand--) {
            uint16_t thing = g_champions[i].slots[hand];
            int16_t icon = object_get_icon_index(thing);
            if (icon < ICON_TORCH_UNLIT || icon > ICON_TORCH_LAST || !rec_weapon_charges(thing))
                continue;
            rec_weapon_set_charges(thing, rec_weapon_charges(thing) - 1);
            if (!rec_weapon_charges(thing))
                rec_object_set_important(thing, false);
        }
}

void party_delete_scent(uint16_t scent_index)
{
    uint16_t n = --g_party.scent_count - scent_index;
    if (n) {
        memmove(&g_party.scents[scent_index], &g_party.scents[scent_index + 1], n * sizeof *g_party.scents);
        memmove(&g_party.scent_strengths[scent_index], &g_party.scent_strengths[scent_index + 1], n);
    }
    if (scent_index < g_party.first_scent_index)
        g_party.first_scent_index--;
    if (scent_index < g_party.last_scent_index)
        g_party.last_scent_index--;
}

void party_add_scent_strength(int16_t x, int16_t y, int16_t strength_and_flags)
{
    if (!g_party.scent_count)
        return;
    int16_t keep_larger = strength_and_flags & SCENT_KEEP_LARGER;
    int16_t found = 0;
    int16_t value = keep_larger ? strength_and_flags & ~SCENT_KEEP_LARGER : strength_and_flags;
    uint16_t scent = party_pack_position(g_current_map_index, x, y);
    for (uint16_t i = 0; i < g_party.scent_count; i++) {
        uint16_t s = g_party.scent_count - 1 - i;
        if (g_party.scents[i] != scent)
            continue;
        if (!found) {
            found = 1;
            value = keep_larger ? max16(g_party.scent_strengths[s], value)
                                : min16(SCENT_STRENGTH_MAXIMUM, value + g_party.scent_strengths[s]);
        }
        g_party.scent_strengths[s] = value;
    }
}
