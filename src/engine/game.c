/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/game.h"
#include "data/os.h"
#include "engine/champion.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/party.h"
#include "engine/random.h"
#include "engine/timeline.h"
#include <stdlib.h>
#include <string.h>

/* Bit masks of the tick intervals of the torches and the time effects. */
#define TORCH_BURN_INTERVAL_MASK   0x1ff /* torches lose one charge every 512 ticks */
#define TIME_EFFECTS_MASK          0x03f /* champion time effects every 64 ticks */
#define TIME_EFFECTS_SLEEPING_MASK 0x00f /* every 16 ticks while the party sleeps */

/* Light and shield amounts of the Fuse action. */
#define FUSE_MAGICAL_LIGHT 200
#define FUSE_SHIELD        100
#define FUSE_LORD_HEALTH   10000

uint32_t g_game_time, g_game_id;
uint16_t g_redraw;
Session g_session = {.mode = GAME_MODE_VANILLA,
                     .candidate_champion_index = -1,
                     .inventory_champion_index = -1,
                     .acting_champion_index = -1,
                     .new_party_map = -1};

void game_fatal(int16_t code)
{
    os_fatal("System Error %d", code);
}

void game_start_new(void)
{
    memset(&g_party, 0, sizeof g_party);
    memset(g_champions, 0, sizeof g_champions);
    g_party_champion_count = 0;
    g_leader_index = -1;
    g_magic_caster_champion_index = -1;
    g_leader_hand_object = THING_NONE;
    g_session.party_sleeping = false;
    g_session.party_dead = false;
    g_game_time = 0;
    g_last_creature_attack_time = 0;
    g_last_party_movement_time = 0;
    g_disabled_movement_ticks = 0;
    g_projectile_disabled_movement_ticks = 0;
    g_last_projectile_disabled_movement_direction = 0;
    g_redraw = 0;
    g_dead_champions = 0;
    g_active_group_count = 0;
    uint32_t r = random16();
    g_game_id = r * random16();
    timeline_init();
    group_init_active_groups();
}

void game_reset_session(void)
{
    g_session.candidate_champion_index = -1;
    g_session.party_sleeping = false;
    g_session.won = false;
    g_session.cheat_light = false;
    g_session.cheat_immortal = false;
    g_session.cheat_food = false;
    g_redraw = 0;
    for (uint16_t i = 0; i < CHAMPION_MAX_COUNT; i++)
        memset(g_champions[i].runtime.skill_level_up, 0, sizeof g_champions[i].runtime.skill_level_up);
}

void game_set_mode(int16_t mode)
{
    g_session.mode = mode;
}

void game_toggle_cheat(int16_t cheat)
{
    bool *flag = cheat == CHEAT_LIGHT      ? &g_session.cheat_light
                 : cheat == CHEAT_IMMORTAL ? &g_session.cheat_immortal
                                           : &g_session.cheat_food;
    *flag = !*flag;
}

void game_start_fuse_sequence(void)
{
    g_session.won = true;
    g_party.magical_light_amount = FUSE_MAGICAL_LIGHT;
    g_party.shield_defense = FUSE_SHIELD;
    g_party.spell_shield_defense = FUSE_SHIELD;
    g_party.fire_shield_defense = FUSE_SHIELD;
}

void game_prepare_fuse_lord(uint16_t lord, Location at)
{
    rec_group_set_health(lord, 0, FUSE_LORD_HEALTH);
    group_set_cells(lord, CELL_CENTERED, g_party_map_index);
    group_set_directions(lord, (uint8_t)direction_back(g_party_direction), g_party_map_index); /* faces the party */
    map_remove_fluxcages((Location){g_party_map_x, g_party_map_y});
    map_remove_fluxcages(at);
}

void game_increment_time(void)
{
    g_game_time++;
}

void game_advance_time(void)
{
    game_increment_time();
    if (!(g_game_time & TORCH_BURN_INTERVAL_MASK))
        party_burn_torches();
    if (g_party.freeze_life_ticks)
        g_party.freeze_life_ticks--;
}

void game_apply_tick_effects(void)
{
    if (!(g_game_time & (g_session.party_sleeping ? TIME_EFFECTS_SLEEPING_MASK : TIME_EFFECTS_MASK)))
        champion_apply_time_effects();
    if (g_disabled_movement_ticks)
        g_disabled_movement_ticks--;
    if (g_projectile_disabled_movement_ticks)
        g_projectile_disabled_movement_ticks--;
}
