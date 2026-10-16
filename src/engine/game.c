/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/game.h"
#include "data/adventure.h"
#include "data/os.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/champion.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/random.h"
#include "engine/timeline.h"
#include <stdlib.h>
#include <string.h>

#define STAT_HIGH                  120 /* statistic gains above this value are halved */
#define STAT_VERY_HIGH             150 /* statistic gains above this value are quartered */
#define ATTACK_FACTOR_MIN          16  /* below this, the statistic factor divides the attack by 8 */
#define ATTACK_FACTOR_SHIFT        7
#define LUCK_STEP                  2     /* luck change after each luck test */
#define PERCENT_RANGE              100   /* a luck test draws random16() % 100 */
#define ATTACK_LOW_FACTOR_SHIFT    3     /* a statistic factor below ATTACK_FACTOR_MIN divides the attack by 8 */
#define TORCH_BURN_INTERVAL_MASK   0x1ff /* torches lose one charge every 512 ticks */
#define TIME_EFFECTS_MASK          0x3f  /* champion time effects every 64 ticks */
#define TIME_EFFECTS_SLEEPING_MASK 0xf   /* every 16 ticks while the party sleeps */
#define FUSE_MAGICAL_LIGHT         200
#define FUSE_SHIELD                100

Party g_party;
Champion g_champions[CHAMPION_MAX_COUNT];
uint16_t g_party_champion_count;
int16_t g_leader_index = -1;
int16_t g_magic_caster_champion_index = -1;
uint16_t g_leader_hand_object = THING_NONE;
bool g_party_is_sleeping, g_party_dead;
bool g_game_won;
uint32_t g_game_time, g_game_id;
int32_t g_last_creature_attack_time, g_last_party_movement_time;
int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks;
int16_t g_last_projectile_disabled_movement_direction;
uint16_t g_redraw;
int16_t g_game_mode = GAME_MODE_VANILLA;
bool g_cheat_light, g_cheat_immortal, g_cheat_food;

ActiveGroup *g_active_groups;
uint16_t g_active_group_count, g_max_active_group_count;

void game_fatal(int16_t code)
{
    os_fatal("System Error %d", code);
}

void game_start_new(void)
{
    memset(&g_party, 0, sizeof g_party);
    memset(g_champions, 0, sizeof g_champions);
    memset(g_champion_pending_damage, 0, sizeof g_champion_pending_damage);
    memset(g_champion_pending_wounds, 0, sizeof g_champion_pending_wounds);
    g_party_champion_count = 0;
    g_leader_index = -1;
    g_magic_caster_champion_index = -1;
    g_leader_hand_object = THING_NONE;
    g_party_is_sleeping = false;
    g_party_dead = false;
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
    game_init_active_groups();
}

void game_reset_session(void)
{
    g_candidate_champion_ordinal = 0;
    g_party_is_sleeping = false;
    g_game_won = false;
    g_cheat_light = false;
    g_cheat_immortal = false;
    g_cheat_food = false;
    memset(g_champion_skill_level_up, 0, sizeof g_champion_skill_level_up);
}

void game_set_mode(int16_t mode)
{
    g_game_mode = mode;
}

void game_toggle_cheat(int16_t cheat)
{
    bool *flag = cheat == CHEAT_LIGHT ? &g_cheat_light : cheat == CHEAT_IMMORTAL ? &g_cheat_immortal : &g_cheat_food;
    *flag = !*flag;
}

void game_start_fuse_sequence(void)
{
    g_game_won = true;
    g_party.magical_light_amount = FUSE_MAGICAL_LIGHT;
    g_party.shield_defense = FUSE_SHIELD;
    g_party.spell_shield_defense = FUSE_SHIELD;
    g_party.fire_shield_defense = FUSE_SHIELD;
}

void game_increment_time(void)
{
    g_game_time++;
}

void game_advance_time(void)
{
    game_increment_time();
    if (!(g_game_time & TORCH_BURN_INTERVAL_MASK))
        inventory_decrease_torches_light_power();
    if (g_party.freeze_life_ticks)
        g_party.freeze_life_ticks--;
}

void game_apply_tick_effects(void)
{
    if (!(g_game_time & (g_party_is_sleeping ? TIME_EFFECTS_SLEEPING_MASK : TIME_EFFECTS_MASK)))
        champion_apply_time_effects();
    if (g_disabled_movement_ticks)
        g_disabled_movement_ticks--;
    if (g_projectile_disabled_movement_ticks)
        g_projectile_disabled_movement_ticks--;
}

void game_update_map_coordinates_after_relative_movement(int16_t direction, int16_t steps_forward, int16_t steps_right,
                                                         int16_t *map_x, int16_t *map_y)
{
    int16_t right = turn_right(direction);
    *map_x = *map_x + steps_forward * tables_get_step_east(direction) + steps_right * tables_get_step_east(right);
    *map_y = *map_y + steps_forward * tables_get_step_north(direction) + steps_right * tables_get_step_north(right);
}

void game_init_active_groups(void)
{
    g_max_active_group_count = g_adventure == ADVENTURE_CSB ? CSB_ACTIVE_GROUP_MAX_COUNT : ACTIVE_GROUP_MAX_COUNT;
    free(g_active_groups);
    g_active_groups = malloc(g_max_active_group_count * sizeof *g_active_groups);
    if (!g_active_groups)
        game_fatal(SYSTEM_ERROR_MEMORY);
    for (uint16_t i = 0; i < g_max_active_group_count; i++)
        g_active_groups[i].group_index = -1;
}

int16_t game_get_stamina_adjusted_value(const Champion *champion, int16_t value)
{
    int16_t half = champion->maximum_stamina >> 1;
    if (champion->current_stamina < half)
        value = (value >> 1) + (int16_t)((int32_t)value * champion->current_stamina / half); /* 16-bit quotient */
    return value;
}

uint16_t game_get_statistic_adjusted_attack(const Champion *champion, uint16_t statistic, uint16_t attack)
{
    int16_t factor = STAT_LIMIT - champion->statistics[statistic][STAT_CURRENT];
    return factor < ATTACK_FACTOR_MIN ? attack >> ATTACK_LOW_FACTOR_SHIFT
                                      : scale_value(attack, ATTACK_FACTOR_SHIFT, factor);
}

void game_adjust_statistic_current_value(Champion *champion, uint16_t statistic, int16_t delta)
{
    uint8_t *s = champion->statistics[statistic];
    if (delta < 0) {
        delta = max16(delta, s[STAT_MINIMUM] - s[STAT_CURRENT]);
    } else {
        if (s[STAT_CURRENT] > STAT_HIGH)
            delta = (s[STAT_CURRENT] > STAT_VERY_HIGH ? delta >> 2 : delta >> 1) + 1;
        delta = min16(delta, STAT_LIMIT - s[STAT_CURRENT]);
    }
    s[STAT_CURRENT] += delta;
}

bool game_is_lucky(Champion *champion, uint16_t percentage)
{
    uint8_t *luck = champion->statistics[STAT_LUCK];
    if (random_bit() && percentage < random16() % PERCENT_RANGE)
        return true;
    bool lucky = (int8_t)luck[STAT_CURRENT] > 0 && percentage < random16() % (luck[STAT_CURRENT] * 2);
    int16_t change = lucky ? -LUCK_STEP : LUCK_STEP;
    luck[STAT_CURRENT] =
        bounded_value((int8_t)luck[STAT_MINIMUM], (int8_t)luck[STAT_CURRENT] + change, (int8_t)luck[STAT_MAXIMUM]);
    return lucky;
}
