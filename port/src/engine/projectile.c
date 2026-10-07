#include "projectile.h"
#include "champion.h"
#include "group.h"
#include "inventory.h"
#include "move.h"
#include "random.h"
#include "sensor.h"
#include "sound.h"
#include "tables.h"
#include "timeline_process.h"

/* Combined square argument: a value from 0x100 holds (x + 1) << 8 | target x (and y << 8 | target y). */
#define SQUARE_COMBO_MIN 0x100
#define SQUARE_COMBO_TARGET_MASK 0xff
#define SQUARE_COMBO_SHIFT 8

/* Potion types that explode when a flask hits something. */
enum { POTION_FUL_BOMB = 19, POTION_VEN_BOMB = 3 };
/* Weapons that a creature with CREATURE_KEEPS_THROWN_SHARP_WEAPONS keeps after a hit. */
enum { WEAPON_DAGGER = 8, WEAPON_ARROW = 27, WEAPON_SLAYER = 28, WEAPON_POISON_DART = 31, WEAPON_THROWING_STAR = 32 };
enum { CREATURE_KEEPS_THROWN_SHARP_WEAPONS = 0x0400 };
/* Door attributes (high byte of g_current_map_door_info): projectiles pass through the grate. */
enum { DOOR_PROJECTILES_PASS = 2 };
/* Object info allowed slots bit: the object passes through a door grate. */
enum { OBJECT_PASSES_DOOR_GRATE = 0x0100 };
/* Junk icons that never pass through a door grate. */
enum { ICON_JUNK_SOLID_FIRST = 0xb0, ICON_JUNK_SOLID_LAST = 0xbf };
enum { PROJECTILE_DOOR_PASS_RANDOM_MASK = 0x7f };
enum { LAUNCHER_EXPLOSION_DATA_BASE = 128, LAUNCHER_ATTACK = 100 };
/* Object info indices that a launcher or a wall generator creates. */
enum {
    LAUNCHER_INFO_TORCH_FIRST = 4, LAUNCHER_INFO_TORCH_LAST = 7, LAUNCHER_INFO_DAGGER = 32, LAUNCHER_INFO_ARROW = 51,
    LAUNCHER_INFO_SLAYER = 52, LAUNCHER_INFO_NONE = 53, LAUNCHER_INFO_ROCK = 54, LAUNCHER_INFO_POISON_DART = 55,
    LAUNCHER_INFO_THROWING_STAR = 56, LAUNCHER_INFO_BOULDER = 128
};
enum { WEAPON_TORCH = 2, WEAPON_ROCK = 30, JUNK_BOULDER = 25 };
enum { WOUND_HEAD_AND_TORSO = WOUND_HEAD | WOUND_TORSO, WOUND_ALL = 0x3f };
enum { MAX_CREATURE_HEALTH = 1000, FIRE_IMMUNE = 15 };
enum { FLUXCAGE_LIFETIME = 100, REBIRTH_STEP_1_DELAY = 5 };
enum { POISON_CLOUD_FADE = 3, POISON_CLOUD_MIN_ATTACK = 5, SMOKE_FADE = 40, SMOKE_MIN_ATTACK = 56 };
enum { WEAK_EXPLOSION_MAX_ATTACK = 80, POISON_CLOUD_MAX_DAMAGE = 4, POISON_REACTION_MIN_DAMAGE = 2 };
enum { PROJECTILE_STEP_TICKS_PARTY_MAP = 1, PROJECTILE_STEP_TICKS_OTHER_MAP = 3 };

int16_t g_create_launcher_projectile;
int16_t g_projectile_poison_attack, g_projectile_attack_type;
int16_t g_fuse_sequence_requested;

static int16_t is_party_on(int16_t x, int16_t y)
{
    return g_current_map_index == g_party_map_index && x == g_party_map_x && y == g_party_map_y;
}

static uint16_t creature_fire_resistance(const CreatureInfo *info) { return info->resistances >> 4 & 15; }
static uint16_t door_attributes(uint16_t door) { return g_current_map_door_info[door_type(door)] >> 8; }

static void add_event_one_tick_later(const TimelineEvent *event)
{
    TimelineEvent next = *event;
    next.map_time++;
    timeline_add_event(&next);
}

/* A flying thing enters a square. Without a free projectile record, the object drops to the square. */
void projectile_create(uint16_t thing, int16_t x, int16_t y, int16_t cell, int16_t direction, uint8_t kinetic_energy, uint8_t attack, uint8_t step_energy)
{
    uint16_t projectile = get_unused_thing(THING_PROJECTILE);
    if (projectile == THING_NONE) {
        if (THING_TYPE(thing) != THING_EXPLOSION)
            move_get_move_result(THING_WITH_CELL(thing, cell), -1, 0, x, y);
        return;
    }
    projectile = THING_WITH_CELL(projectile, cell);
    projectile_set_slot(projectile, thing);
    projectile_set_kinetic_energy(projectile, kinetic_energy);
    projectile_set_attack(projectile, attack);
    link_thing_to_list(projectile, 0, x, y);
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + 1),
        .type = g_create_launcher_projectile ? EVENT_MOVE_PROJECTILE : EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS,
        .b = projectile,
        .c = projectile_event_c(x, y, direction, step_energy),
    };
    projectile_set_event_index(projectile, timeline_add_event(&event));
}

/* Fireball and lightning burn the party or the creatures on the target square at once. */
static void explosion_damage_square(uint16_t explosion_thing, uint16_t attack, int16_t x, int16_t y)
{
    uint16_t half = (attack >> 1) + 1;
    uint16_t damage = random16() % half + 1 + half;
    if (explosion_thing == EXPLOSION_THING(EXPLOSION_LIGHTNING_BOLT)) {
        damage >>= 1;
        if (!damage)
            return;
    }
    if (is_party_on(x, y)) {
        champion_damage_all(damage, WOUND_ALL, ATTACK_FIRE);
        return;
    }
    uint16_t group = get_square_group(x, y);
    if (group == THING_END)
        return;
    const CreatureInfo *info = group_info(group);
    uint16_t resistance = creature_fire_resistance(info);
    if (resistance == FIRE_IMMUNE)
        return;
    if (info->attributes & CREATURE_NON_MATERIAL)
        damage >>= 2;
    int16_t resisted = damage - random16() % (resistance * 2 + 1);
    if (resisted)
        g_creature_damage_outcome = group_get_damage_all_creatures_outcome(group, x, y, resisted, 1);
}

/* An explosion appears on a square for one event 25. A combined square puts the explosion on one square and
   aims the fireball or lightning damage at the target square. */
void explosion_create(uint16_t explosion_thing, uint16_t attack, uint16_t x_combo, uint16_t y_combo, int16_t cell)
{
    uint16_t explosion = get_unused_thing(THING_EXPLOSION);
    if (explosion == THING_NONE)
        return;
    int16_t x = x_combo, y = y_combo, target_x = x, target_y = y;
    if (x_combo >= SQUARE_COMBO_MIN) {
        target_x = x_combo & SQUARE_COMBO_TARGET_MASK;
        target_y = y_combo & SQUARE_COMBO_TARGET_MASK;
        x = (x_combo >> SQUARE_COMBO_SHIFT) - 1;
        y = y_combo >> SQUARE_COMBO_SHIFT;
    }
    explosion_set_centered(explosion, cell == CELL_CENTERED);
    if (cell != CELL_CENTERED)
        explosion = THING_WITH_CELL(explosion, cell);
    explosion_set_type(explosion, explosion_thing - EXPLOSION_THING_BASE);
    explosion_set_attack(explosion, attack);
    if (explosion_thing < EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL))
        sound_request_play(attack > WEAK_EXPLOSION_MAX_ATTACK ? SOUND_STRONG_EXPLOSION : SOUND_WEAK_EXPLOSION, x, y, SOUND_MODE_AFTER_TICK);
    else if (explosion_thing != EXPLOSION_THING(EXPLOSION_SMOKE))
        sound_request_play(SOUND_SPELL, x, y, SOUND_MODE_AFTER_TICK);
    link_thing_to_list(explosion, 0, x, y);
    uint16_t delay = explosion_thing == EXPLOSION_THING(EXPLOSION_REBIRTH_STEP_1) ? REBIRTH_STEP_1_DELAY : 1;
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + delay),
        .type = EVENT_EXPLOSION,
        .b = EVENT_SQUARE(x, y),
        .c = explosion,
    };
    timeline_add_event(&event);
    if (explosion_thing == EXPLOSION_THING(EXPLOSION_LIGHTNING_BOLT) || explosion_thing == EXPLOSION_THING(EXPLOSION_FIREBALL))
        explosion_damage_square(explosion_thing, attack, target_x, target_y);
}

void projectile_delete_event(uint16_t projectile)
{
    timeline_delete_event(rec_projectile_event_index(projectile));
}

/* The flying object drops to the square, or joins the possessions of the group that caught it. */
void projectile_delete(uint16_t projectile, uint16_t possessing_group, int16_t x, int16_t y)
{
    uint16_t thing = rec_projectile_slot(projectile);
    if (THING_TYPE(thing) != THING_EXPLOSION) {
        if (possessing_group == THING_NONE) {
            move_get_move_result(THING_WITH_CELL(thing, THING_CELL(projectile)), -2, 0, x, y);
        } else if (group_possessions(possessing_group) == THING_END) {
            thing_set_next(thing, THING_END);
            group_set_possessions(possessing_group, thing);
        } else {
            link_thing_to_list(thing, group_possessions(possessing_group), -1, 0);
        }
    }
    thing_set_free(projectile);
}

/* Attack of a projectile hit. Sets the attack type and the poison attack of the hit. */
uint16_t projectile_get_impact_attack(uint16_t projectile, uint16_t thing)
{
    uint16_t kinetic_energy = rec_projectile_kinetic_energy(projectile);
    int16_t attack;
    g_projectile_poison_attack = 0;
    g_projectile_attack_type = ATTACK_BLUNT;
    if (THING_TYPE(thing) == THING_EXPLOSION) {
        if (thing == EXPLOSION_THING(EXPLOSION_SLIME)) {
            uint16_t poison = random16() & 15;
            g_projectile_poison_attack = poison + 10;
            attack = (random16() & 31) + poison;
        } else if (thing >= EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL)) {
            g_projectile_attack_type = ATTACK_MAGIC;
            if (thing == EXPLOSION_THING(EXPLOSION_POISON_BOLT)) {
                g_projectile_poison_attack = kinetic_energy;
                return (kinetic_energy >> 4) + 1;
            }
            return 0;
        } else {
            g_projectile_attack_type = ATTACK_FIRE;
            uint16_t first = random16();
            uint16_t second = random16();
            attack = (second & 15) + (first & 15) + 10;
            if (thing == EXPLOSION_THING(EXPLOSION_LIGHTNING_BOLT)) {
                g_projectile_attack_type = ATTACK_LIGHTNING;
                attack = kinetic_energy + attack * 16;
            }
        }
    } else {
        uint16_t base;
        if (THING_TYPE(thing) == THING_WEAPON) {
            base = tables_weapon_info(weapon_type(thing))->kinetic_energy;
            g_projectile_attack_type = ATTACK_BLUNT;
        } else {
            base = random_2bits();
        }
        attack = inventory_get_object_weight(thing) + base;
    }
    uint16_t value = ((uint16_t)(kinetic_energy + attack) >> 4) + 1;
    uint16_t spread = random16();
    uint16_t extra = random_2bits();
    value = extra + spread % ((value >> 1) + 1) + value;
    return max16(value >> 1, value - (32 - (projectile_attack(projectile) >> 3)));
}

/* A creature hit: damage scaled by the creature defense, plus poison. A black flame eats fireballs. A creature that
   keeps sharp weapons catches the dagger, arrow, star, or dart. Returns 0 when no creature stands in the cell, 1 when
   the projectile hit, 2 when the black flame ate the fireball. */
static int16_t hit_creature(uint16_t projectile, uint16_t thing, int16_t is_explosion, int16_t x, int16_t y, int16_t cell,
                            uint16_t *possessing_group)
{
    uint16_t group = get_square_group(x, y);
    int16_t ordinal = group_get_creature_ordinal_in_cell(group, cell);
    if (!ordinal)
        return 0;
    uint16_t creature_type = rec_group_creature_type(group);
    const CreatureInfo *info = group_info(group);
    if (thing == EXPLOSION_THING(EXPLOSION_FIREBALL) && creature_type == CREATURE_BLACK_FLAME) {
        uint16_t health = group_health(group, ordinal - 1);
        uint16_t gain = projectile_get_impact_attack(projectile, EXPLOSION_THING(EXPLOSION_FIREBALL));
        group_set_health(group, ordinal - 1, min16(MAX_CREATURE_HEALTH, health + gain));
        return 2;
    }
    if ((info->attributes & CREATURE_NON_MATERIAL) && thing != EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL))
        return 0;
    uint16_t attack = (uint16_t)(projectile_get_impact_attack(projectile, thing) << 6) / info->defense;
    if (!attack)
        return 1;
    uint16_t poison = group_get_resistance_adjusted_poison_attack(creature_type, g_projectile_poison_attack);
    int16_t outcome = group_get_damage_creature_outcome(group, ordinal - 1, x, y, attack + poison, 1);
    if (outcome != OUTCOME_ALL_CREATURES_KILLED)
        group_process_reaction_event(x, y, REACTION_HIT_BY_PROJECTILE, 0);
    g_creature_damage_outcome = outcome;
    if (!is_explosion && outcome == OUTCOME_NO_CREATURE_KILLED && THING_TYPE(thing) == THING_WEAPON &&
        (info->attributes & CREATURE_KEEPS_THROWN_SHARP_WEAPONS)) {
        uint16_t type = weapon_type(thing);
        if (type == WEAPON_DAGGER || type == WEAPON_ARROW || type == WEAPON_SLAYER || type == WEAPON_POISON_DART ||
            type == WEAPON_THROWING_STAR)
            *possessing_group = group;
    }
    return 1;
}

/* A door hit: an open-door spell opens a door with a button. Other projectiles may pass a grate, else they bash
   the door. Returns 0 when the projectile does not hit. */
static int16_t hit_door(uint16_t projectile, uint16_t thing, int16_t x, int16_t y)
{
    int16_t state = door_state(g_current_map_data[x][y]);
    uint16_t door = get_square_first_thing(x, y);
    if (state != DOOR_DESTROYED && thing == EXPLOSION_THING(EXPLOSION_OPEN_DOOR)) {
        if (door_button_present(door))
            sensor_add_event(EVENT_DOOR, x, y, 0, EFFECT_TOGGLE, g_game_time + 1);
        return 1;
    }
    if (state == DOOR_DESTROYED || state <= DOOR_ALMOST_OPEN)
        return 0;
    if (door_attributes(door) & DOOR_PROJECTILES_PASS) {
        int16_t passes;
        if (THING_TYPE(thing) == THING_EXPLOSION) {
            passes = thing >= EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL);
        } else {
            passes = (random16() & PROJECTILE_DOOR_PASS_RANDOM_MASK) < projectile_attack(projectile) &&
                     (tables_object_info(inventory_get_object_info_index(thing))->allowed_slots & OBJECT_PASSES_DOOR_GRATE);
            if (passes && THING_TYPE(thing) == THING_JUNK) {
                int16_t icon = object_get_icon_index(thing);
                passes = icon < ICON_JUNK_SOLID_FIRST || icon > ICON_JUNK_SOLID_LAST;
            }
        }
        if (passes)
            return 0;
    }
    uint16_t attack = projectile_get_impact_attack(projectile, thing) + 1;
    uint16_t bonus = random16() % attack;
    event_is_door_destroyed_by_attack(x, y, attack + bonus, 0, 0);
    return 1;
}

/* A projectile hits the party (impact -2), a creature (-1), or a wall, stairs, door, or fake wall (square type).
   The flying object drops or explodes and the projectile disappears. Returns 0 when nothing was hit. */
int16_t projectile_has_impact_occurred(int16_t impact_type, int16_t x_combo, int16_t y_combo, int16_t cell, uint16_t projectile)
{
    uint16_t thing = rec_projectile_slot(projectile);
    int16_t thing_type = THING_TYPE(thing);
    int16_t flask_explodes = 0;
    uint16_t flask_explosion = 0, explosion_attack = 0;
    g_creature_damage_outcome = OUTCOME_NO_CREATURE_KILLED;
    if (thing_type == THING_POTION) {
        uint16_t type = potion_type(thing);
        if (type == POTION_FUL_BOMB || type == POTION_VEN_BOMB) {
            flask_explosion = EXPLOSION_THING(type == POTION_FUL_BOMB ? EXPLOSION_FIREBALL : EXPLOSION_POISON_CLOUD);
            flask_explodes = 1;
            explosion_attack = potion_power(thing);
        }
    }
    int16_t is_explosion = thing_type == THING_EXPLOSION && thing != EXPLOSION_THING(EXPLOSION_SLIME) &&
                           thing != EXPLOSION_THING(EXPLOSION_POISON_BOLT);
    uint16_t possessing_group = THING_NONE, champion_attack = 0;
    int16_t x = x_combo, y = y_combo, target_x = x_combo, target_y = y_combo;
    if (x_combo >= SQUARE_COMBO_MIN) {
        x = (x_combo >> SQUARE_COMBO_SHIFT) - 1;
        y = y_combo >> SQUARE_COMBO_SHIFT;
        target_x = x_combo & SQUARE_COMBO_TARGET_MASK;
        target_y = y_combo & SQUARE_COMBO_TARGET_MASK;
    }
    int16_t effect_happens = 1;
    if (impact_type == IMPACT_PARTY) {
        cell = champion_get_index_in_cell(cell);
        if (cell < 0)
            return 0;
        champion_attack = projectile_get_impact_attack(projectile, thing);
    } else if (impact_type == IMPACT_CREATURE) {
        int16_t hit = hit_creature(projectile, thing, is_explosion, target_x, target_y, cell, &possessing_group);
        if (!hit)
            return 0;
        effect_happens = hit == 1;
    } else if (impact_type == SQUARE_DOOR) {
        if (!hit_door(projectile, thing, target_x, target_y))
            return 0;
    }
    if (effect_happens) {
        if (champion_attack && champion_add_pending_damage_and_wounds(cell, champion_attack, WOUND_HEAD_AND_TORSO, g_projectile_attack_type) &&
            g_projectile_poison_attack && random_bit())
            champion_poison(cell, g_projectile_poison_attack);
        if (!is_explosion && !flask_explodes && thing != EXPLOSION_THING(EXPLOSION_POISON_BOLT)) {
            sound_request_play(thing_type == THING_WEAPON ? SOUND_METALLIC_THUD : SOUND_WOODEN_THUD, x, y, SOUND_MODE_AFTER_TICK);
        } else {
            uint16_t explosion = flask_explosion;
            if (!flask_explodes) {
                explosion = thing;
                explosion_attack = rec_projectile_kinetic_energy(projectile);
                if (thing == EXPLOSION_THING(EXPLOSION_POISON_BOLT))
                    explosion_attack >>= 2;
                else if (thing == EXPLOSION_THING(EXPLOSION_LIGHTNING_BOLT))
                    explosion_attack >>= 1;
            }
            int16_t weakened_to_nothing = !flask_explodes && !explosion_attack &&
                                          (thing == EXPLOSION_THING(EXPLOSION_POISON_BOLT) || thing == EXPLOSION_THING(EXPLOSION_LIGHTNING_BOLT));
            /* After a party hit, cell holds the champion index, as in the original. */
            if (!weakened_to_nothing)
                explosion_create(explosion, explosion_attack, x_combo, y_combo,
                                 explosion == EXPLOSION_THING(EXPLOSION_POISON_CLOUD) ? CELL_CENTERED : cell);
        }
    }
    if (flask_explodes) {
        thing_set_free(thing);
        projectile_set_slot(projectile, flask_explosion);
    }
    unlink_thing_from_list(projectile, 0, x, y);
    projectile_delete(projectile, possessing_group, x, y);
    return 1;
}

/* Every projectile in the cell hits. A creature impact stops when the whole group died. Returns the hit count. */
int16_t projectile_get_impact_count(int16_t impact_type, int16_t x, int16_t y, int16_t cell)
{
    int16_t count = 0;
    g_creature_damage_outcome = OUTCOME_NO_CREATURE_KILLED;
    do {
        uint16_t t = get_square_first_thing(x, y);
        while (t != THING_END && !(THING_TYPE(t) == THING_PROJECTILE && THING_CELL(t) == (uint16_t)cell &&
                                   projectile_has_impact_occurred(impact_type, x, y, cell, t)))
            t = get_next_thing(t);
        if (t == THING_END)
            return count;
        projectile_delete_event(t);
        count++;
    } while (impact_type != IMPACT_CREATURE || g_creature_damage_outcome != OUTCOME_ALL_CREATURES_KILLED);
    return count;
}

/* Moves a projectile one cell forward: within the square, or into the next square. Each step outside the first one
   tests the party and the creatures on the square and costs kinetic energy and attack. */
void projectile_process_move_event(TimelineEvent *event)
{
    TimelineEvent next = *event;
    uint16_t projectile = next.b;
    int16_t x = projectile_event_x(&next), y = projectile_event_y(&next);
    uint16_t step_energy = projectile_event_step_energy(&next);
    uint16_t cell = THING_CELL(projectile);
    if (next.type == EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS) {
        next.type = EVENT_MOVE_PROJECTILE;
    } else {
        if (is_party_on(x, y) && projectile_has_impact_occurred(IMPACT_PARTY, x, y, cell, projectile))
            return;
        if (get_square_group(x, y) != THING_END && projectile_has_impact_occurred(IMPACT_CREATURE, x, y, cell, projectile))
            return;
        if (rec_projectile_kinetic_energy(projectile) <= step_energy) {
            unlink_thing_from_list(projectile, 0, x, y);
            projectile_delete(projectile, THING_NONE, x, y);
            return;
        }
        projectile_set_kinetic_energy(projectile, rec_projectile_kinetic_energy(projectile) - step_energy);
        projectile_set_attack(projectile, projectile_attack(projectile) < step_energy ? 0 : projectile_attack(projectile) - step_energy);
    }
    uint16_t direction = projectile_event_direction(&next);
    int16_t leaves_square = cell == direction || cell == ((direction + 1) & 3);
    int16_t dst_x = x, dst_y = y;
    if (leaves_square) {
        update_map_coordinates_after_relative_movement(direction, 1, 0, &dst_x, &dst_y);
        uint8_t square = get_square(dst_x, dst_y);
        int16_t type = SQUARE_TYPE(square);
        int16_t solid = type == SQUARE_WALL || (type == SQUARE_FAKE_WALL && !(square & (FAKE_WALL_OPEN | FAKE_WALL_IMAGINARY))) ||
                        (type == SQUARE_STAIRS && SQUARE_TYPE(g_current_map_data[x][y]) == SQUARE_STAIRS);
        if (solid && projectile_has_impact_occurred(type, x, y, cell, projectile))
            return;
    }
    uint16_t new_cell = ((cell & 1) == (direction & 1) ? cell - 1 : cell + 1) & 3;
    uint16_t moved = THING_WITH_CELL(projectile, new_cell);
    if (!leaves_square) {
        if (SQUARE_TYPE(get_square(x, y)) == SQUARE_DOOR && projectile_has_impact_occurred(SQUARE_DOOR, x, y, new_cell, projectile))
            return;
        unlink_thing_from_list(moved, 0, x, y);
        link_thing_to_list(moved, 0, x, y);
    } else {
        move_get_move_result(moved, x, y, dst_x, dst_y);
        next.c = projectile_event_c(g_move_result_map_x, g_move_result_map_y, g_move_result_direction, step_energy);
        moved = THING_WITH_CELL(projectile, g_move_result_cell);
        next.map_time = EVENT_MAP_TIME(g_move_result_map_index, next.map_time & EVENT_TIME_MASK);
    }
    next.map_time += g_current_map_index == g_party_map_index ? PROJECTILE_STEP_TICKS_PARTY_MAP : PROJECTILE_STEP_TICKS_OTHER_MAP;
    next.b = moved;
    projectile_set_event_index(projectile, timeline_add_event(&next));
}

/* Harm non-material: a materializer is hurt only while it attacks (material); other non-material creatures always. */
static void harm_non_material(uint16_t group, int16_t x, int16_t y, int16_t damage)
{
    if (rec_group_creature_type(group) != CREATURE_MATERIALIZER || g_current_map_index != g_party_map_index) {
        group_get_damage_all_creatures_outcome(group, x, y, damage, 1);
        return;
    }
    for (int16_t i = rec_group_last_creature_index(group); i >= 0; i--) {
        if (!(group_active(group)->aspect[i] & ASPECT_ATTACKING))
            continue;
        uint16_t spread = random16();
        uint16_t extra = random_2bits();
        int16_t eighth = damage >> 3;
        group_get_damage_creature_outcome(group, i, x, y, extra + (damage - eighth) + spread % (uint16_t)(eighth * 2 + 1), 1);
    }
}

/* One explosion step: poison clouds and smoke fade over several events, a rebirth explosion becomes its second
   step, harm non-material hurts non-material creatures, fireball and lightning break doors. Then it disappears. */
void explosion_process_step_event(TimelineEvent *event)
{
    uint16_t explosion = event->c;
    int16_t x = EVENT_X(event), y = EVENT_Y(event);
    uint8_t square = g_current_map_data[x][y];
    uint16_t group = get_square_group(x, y);
    uint16_t type = rec_explosion_type(explosion), attack = rec_explosion_attack(explosion);
    int16_t damage;
    if (type == EXPLOSION_POISON_CLOUD) {
        damage = min16(attack >> 5, POISON_CLOUD_MAX_DAMAGE);
        damage = max16(1, damage + random_bit());
    } else {
        uint16_t half = (attack >> 1) + 1;
        damage = random16() % half + 1 + half;
    }
    int16_t breaks_door = 0;
    switch (type) {
    case EXPLOSION_POISON_CLOUD:
        if (is_party_on(x, y)) {
            champion_damage_all(damage, 0, ATTACK_RAW);
        } else if (group != THING_END) {
            damage = group_get_resistance_adjusted_poison_attack(rec_group_creature_type(group), damage);
            if (damage && group_get_damage_all_creatures_outcome(group, x, y, damage, 1) != OUTCOME_ALL_CREATURES_KILLED &&
                damage > POISON_REACTION_MIN_DAMAGE)
                group_process_reaction_event(x, y, REACTION_DANGER_ON_SQUARE, 0);
        }
        if (attack > POISON_CLOUD_MIN_ATTACK) {
            explosion_set_attack(explosion, attack - POISON_CLOUD_FADE);
            add_event_one_tick_later(event);
            return;
        }
        break;
    case EXPLOSION_REBIRTH_STEP_1:
        explosion_set_type(explosion, EXPLOSION_REBIRTH_STEP_2);
        sound_request_play(SOUND_STRONG_EXPLOSION, x, y, SOUND_MODE_AFTER_TICK);
        add_event_one_tick_later(event);
        return;
    case EXPLOSION_SMOKE:
        if (attack >= SMOKE_MIN_ATTACK) {
            explosion_set_attack(explosion, attack - SMOKE_FADE);
            add_event_one_tick_later(event);
            return;
        }
        break;
    case EXPLOSION_LIGHTNING_BOLT:
        damage >>= 1;
        breaks_door = damage != 0;
        break;
    case EXPLOSION_FIREBALL:
        breaks_door = 1;
        break;
    case EXPLOSION_HARM_NON_MATERIAL:
        if (group != THING_END && (group_info(group)->attributes & CREATURE_NON_MATERIAL))
            harm_non_material(group, x, y, damage);
        break;
    }
    if (breaks_door && SQUARE_TYPE(square) == SQUARE_DOOR)
        event_is_door_destroyed_by_attack(x, y, damage, 1, 0);
    unlink_thing_from_list(explosion, 0, x, y);
    thing_set_free(explosion);
}

/* A fluxcage explosion stands on the square (never on a wall or stairs). */
int16_t projectile_is_fluxcage_on_square(int16_t x, int16_t y)
{
    int16_t type = SQUARE_TYPE(get_square(x, y));
    if (type == SQUARE_WALL || type == SQUARE_STAIRS)
        return 0;
    for (uint16_t t = get_square_first_thing(x, y); t != THING_END; t = get_next_thing(t))
        if (THING_TYPE(t) == THING_EXPLOSION && rec_explosion_type(t) == EXPLOSION_FLUXCAGE)
            return 1;
    return 0;
}

/* The group thing of Lord Chaos on the square, or 0. */
uint16_t projectile_get_lord_chaos_on_square(int16_t x, int16_t y)
{
    uint16_t group = get_square_group(x, y);
    if (group == THING_END || rec_group_creature_type(group) != CREATURE_LORD_CHAOS)
        return 0;
    return group;
}

/* A fluxcage for 100 ticks. When it closes the third side around an adjacent Lord Chaos, he senses danger. */
void projectile_fluxcage_action(int16_t x, int16_t y)
{
    int16_t type = SQUARE_TYPE(get_square(x, y));
    if (type == SQUARE_WALL || type == SQUARE_STAIRS)
        return;
    uint16_t fluxcage = get_unused_thing(THING_EXPLOSION);
    if (fluxcage == THING_NONE)
        return;
    link_thing_to_list(fluxcage, 0, x, y);
    explosion_set_type(fluxcage, EXPLOSION_FLUXCAGE);
    TimelineEvent event = {
        .map_time = EVENT_MAP_TIME(g_current_map_index, g_game_time + FLUXCAGE_LIFETIME),
        .type = EVENT_REMOVE_FLUXCAGE,
        .b = EVENT_SQUARE(x, y),
        .c = fluxcage,
    };
    timeline_add_event(&event);
    int16_t lord_x = x, lord_y = y, cages = 0;
    if (projectile_get_lord_chaos_on_square(x, y - 1)) {
        lord_y = y - 1;
        cages = projectile_is_fluxcage_on_square(x + 1, lord_y) + projectile_is_fluxcage_on_square(x, lord_y - 1) + projectile_is_fluxcage_on_square(x - 1, lord_y);
    } else if (projectile_get_lord_chaos_on_square(x - 1, y)) {
        lord_x = x - 1;
        cages = projectile_is_fluxcage_on_square(lord_x, y + 1) + projectile_is_fluxcage_on_square(lord_x, y - 1) + projectile_is_fluxcage_on_square(lord_x - 1, y);
    } else if (projectile_get_lord_chaos_on_square(x + 1, y)) {
        lord_x = x + 1;
        cages = projectile_is_fluxcage_on_square(lord_x, y - 1) + projectile_is_fluxcage_on_square(lord_x, y + 1) + projectile_is_fluxcage_on_square(lord_x + 1, y);
    } else if (projectile_get_lord_chaos_on_square(x, y + 1)) {
        lord_y = y + 1;
        cages = projectile_is_fluxcage_on_square(x - 1, lord_y) + projectile_is_fluxcage_on_square(x, lord_y + 1) + projectile_is_fluxcage_on_square(x + 1, lord_y);
    }
    if (cages == 2)
        group_process_reaction_event(lord_x, lord_y, REACTION_DANGER_ON_SQUARE, 0);
}

/* Fuse: a harm non-material explosion. Lord Chaos in 4 fluxcages starts the fuse sequence; otherwise he escapes
   through a free side to an open square. */
void projectile_fuse_action(int16_t x, int16_t y)
{
    if ((uint16_t)x >= (uint16_t)g_current_map_width || (uint16_t)y >= (uint16_t)g_current_map_height)
        return;
    explosion_create(EXPLOSION_THING(EXPLOSION_HARM_NON_MATERIAL), 255, x, y, CELL_CENTERED);
    uint16_t lord_chaos = projectile_get_lord_chaos_on_square(x, y);
    if (!lord_chaos)
        return;
    /* Sides: 0 west, 1 east, 2 north, 3 south. */
    int16_t cages[4] = {
        projectile_is_fluxcage_on_square(x - 1, y), projectile_is_fluxcage_on_square(x + 1, y),
        projectile_is_fluxcage_on_square(x, y - 1), projectile_is_fluxcage_on_square(x, y + 1),
    };
    uint16_t count = cages[0] + cages[1] + cages[2] + cages[3];
    int16_t escape_x, escape_y;
    do {
        if (count > 3) {
            g_fuse_sequence_requested = 1;
            return;
        }
        count++;
        uint16_t side = random_2bits();
        escape_x = x;
        escape_y = y;
        for (int16_t tries = 4; tries; tries--, side = (side + 1) & 3) {
            if (cages[side])
                continue;
            cages[side] = 1;
            if (side == 0)
                escape_x = x - 1;
            else if (side == 1)
                escape_x = x + 1;
            else if (side == 2)
                escape_y = y - 1;
            else
                escape_y = y + 1;
            break;
        }
    } while (!is_square_open(escape_x, escape_y));
    if (!move_get_move_result(lord_chaos, x, y, escape_x, escape_y))
        group_start_wandering(escape_x, escape_y);
}

/* Projectiles in the cells of the party (thing 0xffff) or of a group hit before it moves. A move to an adjacent
   square also tests the projectiles on the destination that the movers cross. Returns 1 when the group died. */
int16_t projectile_is_killed_by_impact(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, uint16_t thing)
{
    uint8_t occupied[4] = {0}, crossed[4] = {0};
    int16_t impact_type;
    if (thing == THING_NONE) {
        impact_type = IMPACT_PARTY;
        for (uint16_t cell = 0; cell < 4; cell++)
            if (champion_get_index_in_cell(cell) >= 0)
                occupied[cell] = cell + 1;
    } else {
        impact_type = IMPACT_CREATURE;
        uint16_t health = 0;
        for (uint16_t cell = 0; cell < 4; cell++) {
            health |= group_health(thing, cell);
            if (group_get_creature_ordinal_in_cell(thing, cell))
                occupied[cell] = cell + 1;
        }
        if (!health)
            return 0;
    }
    int16_t test_destination = 0;
    if (dst_x >= 0 && group_get_square_distance(src_x, src_y, dst_x, dst_y) == 1) {
        uint16_t primary = group_get_directions_to_destination(src_x, src_y, dst_x, dst_y);
        uint16_t next = (primary + 1) & 3;
        crossed[(primary + 3) & 3] = occupied[primary];
        crossed[(next + 1) & 3] = occupied[next];
        test_destination = occupied[primary] || occupied[next];
        if (!occupied[primary])
            occupied[primary] = occupied[(primary + 3) & 3];
        if (!occupied[next])
            occupied[next] = occupied[(next + 1) & 3];
    }
    int16_t x = src_x, y = src_y, x_combo = src_x, y_combo = src_y;
    for (;;) {
        int16_t hit;
        do {
            hit = 0;
            for (uint16_t t = get_square_first_thing(x, y); t != THING_END; t = get_next_thing(t)) {
                uint8_t target = occupied[THING_CELL(t)];
                if (THING_TYPE(t) != THING_PROJECTILE || g_timeline_events[rec_projectile_event_index(t)].type == EVENT_MOVE_PROJECTILE_IGNORE_IMPACTS ||
                    !target || !projectile_has_impact_occurred(impact_type, x_combo, y_combo, target - 1, t))
                    continue;
                projectile_delete_event(t);
                if (g_creature_damage_outcome == OUTCOME_ALL_CREATURES_KILLED)
                    return 1;
                hit = 1;
                break;
            }
        } while (hit);
        if (!test_destination)
            return 0;
        x = dst_x;
        y = dst_y;
        x_combo = (dst_x + 1) << SQUARE_COMBO_SHIFT | src_x;
        y_combo = dst_y << SQUARE_COMBO_SHIFT | src_y;
        for (int16_t i = 0; i < 4; i++)
            occupied[i] = crossed[i];
        test_destination = 0;
    }
}

/* The first object (or group) in the launch cell or the cell beside it. */
static uint16_t square_object_in_cells(int16_t x, int16_t y, uint16_t cell)
{
    /* Bug fix: the original scans until 0xffff and reads past the end of the list. */
    for (uint16_t t = get_square_first_thing(x, y); t != THING_END; t = get_next_thing(t))
        if (THING_TYPE(t) >= THING_GROUP && (THING_CELL(t) == cell || THING_CELL(t) == ((cell + 1) & 3)))
            return t;
    return THING_END;
}

/* A wall launcher (sensor 7-10, 14, 15) shoots one or two objects or explosions from the square in front of it. */
void projectile_trigger_launcher(uint16_t sensor, const TimelineEvent *event)
{
    int16_t x = EVENT_X(event), y = EVENT_Y(event);
    uint16_t direction = EVENT_CELL(event), cell = (direction + 2) & 3;
    uint16_t type = rec_sensor_type(sensor), data = rec_sensor_data(sensor);
    int16_t single = type == SENSOR_WALL_LAUNCHER_OBJECT || type == SENSOR_WALL_LAUNCHER_EXPLOSION || type == SENSOR_WALL_LAUNCHER_SQUARE_OBJECT;
    uint16_t first, second = THING_NONE;
    if (type == SENSOR_WALL_LAUNCHER_EXPLOSION || type == SENSOR_WALL_LAUNCHER_TWO_EXPLOSIONS) {
        first = second = data - LAUNCHER_EXPLOSION_DATA_BASE;
    } else if (type == SENSOR_WALL_LAUNCHER_SQUARE_OBJECT || type == SENSOR_WALL_LAUNCHER_TWO_SQUARE_OBJECTS) {
        first = square_object_in_cells(x, y, direction);
        if (first == THING_END)
            return;
        unlink_thing_from_list(first, 0, x, y);
        if (!single) {
            second = square_object_in_cells(x, y, direction);
            if (second == THING_END)
                single = 1;
            else
                unlink_thing_from_list(second, 0, x, y);
        }
    } else {
        first = projectile_get_object_for_launcher_or_generator(data);
        if (first == THING_NONE)
            return;
        if (!single) {
            second = projectile_get_object_for_launcher_or_generator(data);
            if (second == THING_NONE)
                single = 1;
        }
    }
    if (single)
        cell = (cell + random_bit()) & 3;
    int16_t target_x = x, target_y = y;
    update_map_coordinates_after_relative_movement(direction, 1, 0, &target_x, &target_y);
    uint8_t kinetic_energy = rec_sensor_launcher_kinetic_energy(sensor), step_energy = rec_sensor_launcher_step_energy(sensor);
    g_create_launcher_projectile = 1;
    projectile_create(first, target_x, target_y, cell, direction, kinetic_energy, LAUNCHER_ATTACK, step_energy);
    if (!single)
        projectile_create(second, target_x, target_y, (cell + 1) & 3, direction, kinetic_energy, LAUNCHER_ATTACK, step_energy);
    g_create_launcher_projectile = 0;
}

/* A new weapon or junk object for an object info index: torches, daggers, arrows, slayers, rocks, poison darts,
   throwing stars, boulders. THING_NONE for other indices. */
uint16_t projectile_get_object_for_launcher_or_generator(int16_t object_info_index)
{
    if ((uint16_t)object_info_index >= LAUNCHER_INFO_TORCH_FIRST && (uint16_t)object_info_index <= LAUNCHER_INFO_TORCH_LAST)
        object_info_index = LAUNCHER_INFO_TORCH_FIRST;
    uint16_t thing_type = THING_WEAPON, object_type;
    switch (object_info_index) {
    case LAUNCHER_INFO_TORCH_FIRST: object_type = WEAPON_TORCH; break;
    case LAUNCHER_INFO_DAGGER: object_type = WEAPON_DAGGER; break;
    case LAUNCHER_INFO_ARROW: object_type = WEAPON_ARROW; break;
    case LAUNCHER_INFO_SLAYER: object_type = WEAPON_SLAYER; break;
    case LAUNCHER_INFO_ROCK: object_type = WEAPON_ROCK; break;
    case LAUNCHER_INFO_POISON_DART: object_type = WEAPON_POISON_DART; break;
    case LAUNCHER_INFO_THROWING_STAR: object_type = WEAPON_THROWING_STAR; break;
    case LAUNCHER_INFO_BOULDER: object_type = JUNK_BOULDER; thing_type = THING_JUNK; break;
    default: return THING_NONE;
    }
    uint16_t thing = get_unused_thing(thing_type);
    if (thing == THING_NONE)
        return THING_NONE;
    /* The original also fills the charges of a lit torch; a new record is never lit. */
    rec_object_set_type(thing, object_type);
    return thing;
}

/* A champion throws or shoots from the front cell of his side; the party cannot move into it for 4 ticks. */
void projectile_shoot_from_champion(int16_t champion_index, uint16_t thing, uint8_t kinetic_energy, uint8_t attack, uint8_t step_energy)
{
    const Champion *champion = &g_champions[champion_index];
    uint16_t direction = champion->direction;
    uint16_t cell = (direction + (((champion->cell - direction + 1) & 2) >> 1)) & 3;
    projectile_create(thing, g_party_map_x, g_party_map_y, cell, direction, kinetic_energy, attack, step_energy);
    g_projectile_disabled_movement_ticks = 4;
    g_last_projectile_disabled_movement_direction = direction;
}

/* No launcher, poison attack, or fuse request carries into a new game. */
void projectile_reset(void)
{
    g_create_launcher_projectile = 0;
    g_projectile_poison_attack = g_projectile_attack_type = 0;
    g_fuse_sequence_requested = 0;
}
