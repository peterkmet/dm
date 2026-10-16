/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/action.h"
#include "data/adventure.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/champion.h"
#include "engine/dungeon.h"
#include "engine/game.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/magic.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/move.h"
#include "engine/projectile.h"
#include "engine/random.h"
#include "engine/sound.h"
#include "engine/timeline_process.h"
#include <string.h>

#define MESSAGE_SIZE 100

/* Weapon classes (WeaponInfo.weapon_class). */
#define WEAPON_CLASS_BOW_AMMUNITION      10
#define WEAPON_CLASS_SLING_AMMUNITION    11
#define WEAPON_CLASS_KINETIC_THROW_LIMIT 13
#define WEAPON_CLASS_FIRST_BOW           16
#define WEAPON_CLASS_LAST_BOW            31
#define WEAPON_CLASS_FIRST_SLING         32
#define WEAPON_CLASS_LAST_SLING          47

#define FREEZE_LIFE_TICKS_BLUE_BOX  30
#define FREEZE_LIFE_TICKS_GREEN_BOX 125
#define FREEZE_LIFE_TICKS_CHARGE    70
#define FREEZE_LIFE_TICKS_MAXIMUM   200

#define SPELL_ACTION_MAXIMUM_LEVEL    6 /* the mana cost of a spell action is 7 - min(6, skill level) */
#define FIREBALL_ACTION_ATTACK        150
#define DISPELL_ACTION_ATTACK         150
#define LIGHTNING_ACTION_ATTACK       180
#define SPIT_ACTION_ATTACK            250
#define INVOKE_ACTION_BASE_ATTACK     100
#define INVOKE_SPELL_COUNT            6
#define SHIELD_ACTION_TICKS           280
#define LIGHT_ACTION_POWER            2
#define LIGHT_ACTION_TICKS            2500
#define WINDOW_ACTION_BASE_TICKS      5
#define HEAL_ACTION_MAXIMUM_LEVEL     10
#define HEAL_ACTION_MANA_PER_STEP     2
#define DOOR_BASH_DISABLED_TICKS      6
#define DOOR_BASH_DESTRUCTION_TICKS   2
#define THROW_DISABLED_TICKS          4
#define THROW_EXPERIENCE              8
#define THROW_WEAPON_EXPERIENCE       12
#define THROW_STAMINA_MAXIMUM         10
#define THROW_ATTACK_MINIMUM          40
#define THROW_ATTACK_MAXIMUM          200
#define THROW_STEP_ENERGY_BASE        11
#define THROW_STEP_ENERGY_MINIMUM     5
#define WEAPON_CLASS_SKILL_LEVEL_STEP 8
#define WEAPON_SHOOT_ATTACK_MASK      0xff /* weapon info attributes bits 0-7: the attack of a launcher */
#define SPELL_ACTION_ATTACK_MINIMUM   2    /* with too little mana: max(2, mana * attack / cost) */
#define INVOKE_ACTION_RANDOM_MASK     0x7f /* Invoke attack: 100 + random(128) */
#define HEAL_ACTION_EXPERIENCE_STEP   2    /* Heal experience: 2, plus 2 for each step */
#define WINDOW_ACTION_LEVEL_BONUS     8    /* Window ticks: random % (level + 8) + 5 */
#define THROW_ENERGY_RANDOM_MASK      0xf
#define THROW_ATTACK_RANDOM_MASK      0x1f

/* Fear actions: the bonus to the influence roll and the experience. */
#define FEAR_BONUS_CONFUSE        12
#define FEAR_EXPERIENCE_CONFUSE   45
#define FEAR_BONUS_WAR_CRY        3
#define FEAR_EXPERIENCE_WAR_CRY   12
#define FEAR_BONUS_BLOW_HORN      6
#define FEAR_EXPERIENCE_BLOW_HORN 20
#define FEAR_BONUS_BRANDISH       6
#define FEAR_EXPERIENCE_BRANDISH  30
#define FEAR_BONUS_CALM           7
#define FEAR_EXPERIENCE_CALM      35
#define FLEE_DELAY_FEAR_LIMIT     16 /* flee delay: (16 - fear) * 4 / movement ticks */
#define FLEE_DELAY_FACTOR         4

/* Spells of Invoke: random % INVOKE_SPELL_COUNT; the other values give a fireball. */
#define INVOKE_POISON_BOLT       0
#define INVOKE_POISON_CLOUD      1
#define INVOKE_HARM_NON_MATERIAL 2

/* Cells relative to the champion direction. */
#define RELATIVE_CELL_FRONT_LEFT  0
#define RELATIVE_CELL_FRONT_RIGHT 1
#define RELATIVE_CELL_BACK_RIGHT  2
#define RELATIVE_CELL_BACK_LEFT   3

uint16_t g_action_target_group_thing;
int16_t g_acting_champion_ordinal;
uint8_t g_action_list_indices[ACTION_LIST_COUNT] = {ACTION_NONE, ACTION_NONE, ACTION_NONE};
int16_t g_action_damage;
uint8_t g_action_list_minimum_skill_levels[ACTION_LIST_COUNT];
uint16_t g_action_count;

/* The charges of the object in the action hand of the acting champion (1 when it has none). */
static uint16_t action_hand_charge_count(void)
{
    uint16_t thing = g_champions[g_acting_champion_ordinal - 1].slots[SLOT_HAND_ACTION];
    switch (THING_TYPE(thing)) {
    case THING_WEAPON:
        return weapon_charges(thing);
    case THING_ARMOUR:
        return armour_charges(thing);
    case THING_JUNK:
        return junk_charges(thing);
    }
    return 1;
}

void action_set_action_list(int16_t action_set_index)
{
    const ActionSet *set = tables_get_action_set(action_set_index);
    uint16_t n = 1;
    if (!set)
        return;
    g_action_list_indices[0] = set->action_indices[0];
    g_action_list_minimum_skill_levels[0] = 1;
    for (int k = 1; k < ACTION_LIST_COUNT; k++) {
        uint8_t action = set->action_indices[k];
        uint8_t level = set->minimum_skill_levels[k - 1];
        const ActionInfo *info = tables_get_action_info(action);
        if (action == ACTION_NONE || !info || ((level & ACTION_LEVEL_NEEDS_CHARGES) && !action_hand_charge_count()))
            continue;
        level &= ~ACTION_LEVEL_NEEDS_CHARGES;
        if (level <= champion_get_skill_level(g_acting_champion_ordinal - 1, info->skill_index)) {
            g_action_list_indices[n] = action;
            g_action_list_minimum_skill_levels[n++] = level;
        }
    }
    g_action_count = n;
    for (; n < ACTION_LIST_COUNT; n++)
        g_action_list_indices[n] = ACTION_NONE;
}

/* A living champion that may act becomes the acting champion: the actions of the object in its action
   hand (or of the empty hand) form the action list, and its action hand is drawn again. */
bool action_set_acting_champion(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    int16_t set = ACTION_SET_EMPTY_HAND;
    if (c->attributes & CHAMPION_ATTR_ACTION_DISABLED || !c->current_health)
        return false;
    if (c->slots[SLOT_HAND_ACTION] != THING_NONE) {
        const ObjectInfo *info = tables_get_object_info(inventory_get_object_info_index(c->slots[SLOT_HAND_ACTION]));
        if (!info || !(set = info->action_set_index))
            return false;
    }
    g_acting_champion_ordinal = champion_index + 1;
    action_set_action_list(set);
    c->attributes |= CHAMPION_ATTR_ACTION_HAND;
    return true;
}

int16_t action_take_damage(void)
{
    int16_t damage = g_action_damage;
    g_action_damage = 0;
    return damage;
}

void action_reset(void)
{
    g_acting_champion_ordinal = 0;
    memset(g_action_list_indices, ACTION_NONE, sizeof g_action_list_indices);
    memset(g_action_list_minimum_skill_levels, 0, sizeof g_action_list_minimum_skill_levels);
    g_action_count = 0;
    g_action_damage = 0;
    g_redraw = 0;
}

void action_clear_acting_champion(void)
{
    if (!g_acting_champion_ordinal)
        return;
    g_champions[g_acting_champion_ordinal - 1].attributes |= CHAMPION_ATTR_ACTION_HAND;
    g_acting_champion_ordinal = 0;
    g_redraw |= REDRAW_ACTION_AREA;
}

/* The square in front of (x, y) in the direction. */
static void step_forward(int16_t direction, int16_t *x, int16_t *y)
{
    *x = g_party_map_x;
    *y = g_party_map_y;
    game_update_map_coordinates_after_relative_movement(direction, 1, 0, x, y);
}

void action_print_action_message(uint16_t champion_index, const char *text)
{
    char out[MESSAGE_SIZE];
    size_t n = 0;
    out[n++] = '\n';
    for (; *text && n < sizeof out - 1; text++) {
        if (*text != '@') {
            out[n++] = *text;
            continue;
        }
        if (out[n - 1] != '\n')
            out[n++] = ' ';
        for (const char *name = g_champions[champion_index].name; *name && n < sizeof out - 2; name++)
            out[n++] = *name;
        out[n++] = ' ';
        if (text[1])
            text++;
    }
    out[n] = 0;
    if (out[1])
        message_print(MESSAGE_COLOR_CYAN, out, g_game_time);
}

void action_decrement_charges(uint16_t champion_index)
{
    uint16_t thing = g_champions[champion_index].slots[SLOT_HAND_ACTION];
    switch (THING_TYPE(thing)) {
    case THING_WEAPON:
        if (weapon_charges(thing))
            weapon_set_charges(thing, weapon_charges(thing) - 1);
        break;
    case THING_ARMOUR:
        if (armour_charges(thing))
            armour_set_charges(thing, armour_charges(thing) - 1);
        break;
    case THING_JUNK:
        if (junk_charges(thing))
            junk_set_charges(thing, junk_charges(thing) - 1);
        break;
    }
    /* The interface redraws the changed object icons from the slots. */
}

bool action_is_group_frightened_by_action(uint16_t champion_index, int16_t action, int16_t x, int16_t y)
{
    uint16_t group = g_action_target_group_thing;
    if (group == THING_ENDOFLIST)
        return false;
    uint16_t bonus = 0;
    uint16_t experience = 0;
    switch (action) {
    case ACTION_CONFUSE:
        bonus = FEAR_BONUS_CONFUSE;
        experience = FEAR_EXPERIENCE_CONFUSE;
        break;
    case ACTION_WAR_CRY:
        bonus = FEAR_BONUS_WAR_CRY;
        experience = FEAR_EXPERIENCE_WAR_CRY;
        break;
    case ACTION_BLOW_HORN:
        bonus = FEAR_BONUS_BLOW_HORN;
        experience = FEAR_EXPERIENCE_BLOW_HORN;
        break;
    case ACTION_BRANDISH:
        bonus = FEAR_BONUS_BRANDISH;
        experience = FEAR_EXPERIENCE_BRANDISH;
        break;
    case ACTION_CALM:
        bonus = FEAR_BONUS_CALM;
        experience = FEAR_EXPERIENCE_CALM;
        break;
    }
    uint16_t level = champion_get_skill_level(champion_index, SKILL_INFLUENCE);
    const CreatureInfo *info = group_info(group);
    uint16_t roll = random16() % (level + bonus);
    uint16_t fear = creature_fear_resistance(info);
    bool frightened = false;
    if (roll < fear || fear == FEAR_RESISTANCE_NONE_FEARLESS) {
        experience >>= 1;
    } else {
        ActiveGroup *active = group_active(group);
        if (group_behavior(group) == BEHAVIOR_ATTACK) {
            group_stop_attacking(active, x, y);
            group_start_wandering(x, y);
        }
        group_set_behavior(group, BEHAVIOR_FLEE);
        active->delay_fleeing = (FLEE_DELAY_FEAR_LIMIT - fear) * FLEE_DELAY_FACTOR / info->movement_ticks;
        frightened = true;
    }
    champion_add_skill_experience(champion_index, SKILL_INFLUENCE, experience);
    return frightened;
}

bool action_is_melee_action_performed(uint16_t champion_index, int16_t action, int16_t x, int16_t y,
                                      int16_t skill_index)
{
    const Champion *c = &g_champions[champion_index];
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    if (g_action_target_group_thing == THING_ENDOFLIST)
        return false;
    int16_t ordinal = group_get_melee_target_creature_ordinal(x, y, g_party_map_x, g_party_map_y, c->cell);
    if (!ordinal)
        return false;
    uint16_t relative_cell = NORMALIZE(c->cell - c->direction);
    if (relative_cell >= RELATIVE_CELL_BACK_RIGHT) {
        int16_t front_cell = NORMALIZE(c->cell + (relative_cell == RELATIVE_CELL_BACK_LEFT ? 1 : -1));
        if (champion_get_index_in_cell(front_cell) != -1) {
            g_action_damage = ACTION_DAMAGE_CANT_REACH;
            return false;
        }
    }
    if (action == ACTION_DISRUPT && !(map_get_creature_attributes(g_action_target_group_thing) & CREATURE_NON_MATERIAL))
        return false;
    const ActionInfo *info = tables_get_action_info(action);
    uint16_t hit_probability = info->hit_probability;
    if (object_get_icon_index(c->slots[SLOT_HAND_ACTION]) == ICON_VORPAL_BLADE || action == ACTION_DISRUPT)
        hit_probability |= HIT_PROBABILITY_ALWAYS_HITS;
    g_action_damage = group_get_melee_action_damage(champion_index, g_action_target_group_thing, ordinal - 1, x, y,
                                                    hit_probability, info->damage_factor, skill_index);
    return true;
}

/* A spell action shoots the explosion. With less mana than the cost, the attack shrinks and all mana is used. */
static bool is_spell_action_cast(uint16_t champion_index, uint16_t explosion_type, int16_t attack, int16_t mana_cost)
{
    Champion *c = &g_champions[champion_index];
    champion_face_party_direction(c);
    if (c->current_mana < mana_cost) {
        attack =
            max16(SPELL_ACTION_ATTACK_MINIMUM, (int16_t)(c->current_mana * attack) / mana_cost); /* 16-bit product */
        mana_cost = c->current_mana;
    }
    bool cast = magic_is_projectile_spell_cast(champion_index, EXPLOSION_THING(explosion_type), attack, mana_cost);
    action_decrement_charges(champion_index);
    return cast;
}

/* The bow or sling in the action hand shoots the ammunition of the ready hand. Returns false without matching
   ammunition. */
static bool is_shot(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    uint16_t ammunition = c->slots[SLOT_HAND_READY];
    if (THING_TYPE(ammunition) != THING_WEAPON)
        return false;
    const WeaponInfo *launcher = tables_get_weapon_info(weapon_type(c->slots[SLOT_HAND_ACTION]));
    const WeaponInfo *missile = tables_get_weapon_info(weapon_type(ammunition));
    int16_t step_energy = launcher->weapon_class;
    if (step_energy >= WEAPON_CLASS_FIRST_BOW && step_energy <= WEAPON_CLASS_LAST_BOW) {
        if (missile->weapon_class != WEAPON_CLASS_BOW_AMMUNITION)
            return false;
        step_energy -= WEAPON_CLASS_FIRST_BOW;
    } else if (step_energy >= WEAPON_CLASS_FIRST_SLING && step_energy <= WEAPON_CLASS_LAST_SLING) {
        if (missile->weapon_class != WEAPON_CLASS_SLING_AMMUNITION)
            return false;
        step_energy -= WEAPON_CLASS_FIRST_SLING;
    }
    champion_face_party_direction(c);
    uint16_t thing = inventory_get_object_removed_from_slot(champion_index, SLOT_HAND_READY);
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    int16_t level = champion_get_skill_level(champion_index, SKILL_SHOOT);
    /* The attack is a byte: the launcher shoot attack plus the level, doubled. */
    projectile_shoot_from_champion(champion_index, thing, (uint8_t)(missile->kinetic_energy + launcher->kinetic_energy),
                                   (uint8_t)((level + (launcher->attributes & WEAPON_SHOOT_ATTACK_MASK)) << 1),
                                   (uint8_t)step_energy);
    return true;
}

/* Freeze Life stops the creatures. A magical box is used up; other objects use a charge. */
static void cast_freeze_life(uint16_t champion_index)
{
    uint16_t thing = g_champions[champion_index].slots[SLOT_HAND_ACTION];
    int16_t ticks;
    /* Weapons, armour, and junk keep their type in the same bits. */
    uint16_t type = junk_type(thing);
    if (type == JUNK_MAGICAL_BOX_BLUE || type == JUNK_MAGICAL_BOX_GREEN) {
        ticks = type == JUNK_MAGICAL_BOX_BLUE ? FREEZE_LIFE_TICKS_BLUE_BOX : FREEZE_LIFE_TICKS_GREEN_BOX;
        inventory_get_object_removed_from_slot(champion_index, SLOT_HAND_ACTION);
        thing_set_free(thing);
    } else {
        ticks = FREEZE_LIFE_TICKS_CHARGE;
        action_decrement_charges(champion_index);
    }
    g_party.freeze_life_ticks = min16(FREEZE_LIFE_TICKS_MAXIMUM, ticks + g_party.freeze_life_ticks);
}

/* Heal changes mana into health: up to min(10, heal level) health for each 2 mana. Returns the experience. */
static uint16_t heal_champion(uint16_t champion_index, uint16_t experience)
{
    Champion *c = &g_champions[champion_index];
    int16_t missing = c->maximum_health - c->current_health;
    if (missing <= 0 || !c->current_mana)
        return experience;
    int16_t step = min16(HEAL_ACTION_MAXIMUM_LEVEL, champion_get_skill_level(champion_index, SKILL_HEAL));
    experience = HEAL_ACTION_EXPERIENCE_STEP;
    for (;;) {
        int16_t amount = min16(missing, step);
        c->current_health += amount;
        experience += HEAL_ACTION_EXPERIENCE_STEP;
        c->current_mana -= HEAL_ACTION_MANA_PER_STEP;
        if (c->current_mana <= 0)
            break;
        missing -= amount;
        if (!missing)
            break;
    }
    if (c->current_mana < 0)
        c->current_mana = 0;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    return experience;
}

/* Window: thieves eye for a random time by the skill level. */
static void open_window(uint16_t champion_index, uint16_t skill_index)
{
    uint16_t random = random16();
    int16_t level = champion_get_skill_level(champion_index, skill_index);
    uint16_t ticks = random % (level + WINDOW_ACTION_LEVEL_BONUS) + WINDOW_ACTION_BASE_TICKS;
    TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + ticks),
                           .type = EVENT_THIEVES_EYE_END};
    timeline_add_event(&event);
    g_party.thieves_eye_count++;
}

/* Climb down: the party goes down the pit in front with the rope, without fall damage. Returns false without a pit. */
static bool is_climbed_down(void)
{
    int16_t x, y;
    step_forward(g_party_direction, &x, &y);
    if (SQUARE_TYPE(map_get_square(x, y)) != SQUARE_PIT ||
        map_find_thing((Location){x, y}, THING_GROUP) != THING_ENDOFLIST)
        return false;
    g_use_rope_to_climb_down_pit = true;
    move_get_move_result(THING_NONE, g_party_map_x, g_party_map_y, x, y);
    g_use_rope_to_climb_down_pit = false;
    return true;
}

/* Fireball, Dispell, Lightning, Spit, and Invoke shoot their explosion. Invoke takes a random attack and spell. */
static bool is_explosion_action_cast(uint16_t champion_index, int16_t action, int16_t mana_cost)
{
    int16_t attack = 0;
    uint16_t type = EXPLOSION_FIREBALL;
    switch (action) {
    case ACTION_FIREBALL:
        attack = FIREBALL_ACTION_ATTACK;
        break;
    case ACTION_DISPELL:
        attack = DISPELL_ACTION_ATTACK;
        type = EXPLOSION_HARM_NON_MATERIAL;
        break;
    case ACTION_LIGHTNING:
        attack = LIGHTNING_ACTION_ATTACK;
        type = EXPLOSION_LIGHTNING_BOLT;
        break;
    case ACTION_SPIT:
        attack = SPIT_ACTION_ATTACK;
        break;
    case ACTION_INVOKE: {
        attack = (random16() & INVOKE_ACTION_RANDOM_MASK) + INVOKE_ACTION_BASE_ATTACK;
        uint16_t spell = random16() % INVOKE_SPELL_COUNT;
        type = spell == INVOKE_POISON_BOLT         ? EXPLOSION_POISON_BOLT
               : spell == INVOKE_POISON_CLOUD      ? EXPLOSION_POISON_CLOUD
               : spell == INVOKE_HARM_NON_MATERIAL ? EXPLOSION_HARM_NON_MATERIAL
                                                   : EXPLOSION_FIREBALL;
        break;
    }
    }
    return is_spell_action_cast(champion_index, type, attack, mana_cost);
}

/* Chop, Kick, Swing, Hack, Berzerk, and Bash on a closed door: the champion bashes the door with the action hand
   strength. Returns false without a closed door in front. */
static bool is_door_bashed(uint16_t champion_index, int16_t x, int16_t y, uint8_t square)
{
    if (SQUARE_TYPE(square) != SQUARE_DOOR || door_state(square) != DOOR_CLOSED)
        return false;
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    event_is_door_destroyed_by_attack(x, y, champion_get_strength(champion_index, SLOT_HAND_ACTION), 0,
                                      DOOR_BASH_DESTRUCTION_TICKS);
    sound_request_play(SOUND_WOODEN_THUD, g_party_map_x, g_party_map_y, SOUND_MODE_ONE_TICK_LATER);
    return true;
}

/* Throw: the champion throws the action hand object from its side of the party. Event 11 then moves the next
   weapon from the quiver to the action hand. */
static bool is_action_hand_object_thrown(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    champion_face_party_direction(c);
    int16_t side = turn_right(g_party_direction) == c->cell || turn_back(g_party_direction) == c->cell;
    if (!action_is_object_thrown(champion_index, SLOT_HAND_ACTION, side))
        return false;
    event_set_quiver_slot(&g_timeline_events[c->enable_action_event_index], SLOT_HAND_ACTION);
    return true;
}

/* CSB has no code for invoke, fluxcage, and fuse: they only disable the hand, take stamina, and give experience. */
static bool is_removed_in_csb(int16_t action)
{
    return g_adventure == ADVENTURE_CSB &&
           (action == ACTION_INVOKE || action == ACTION_FLUXCAGE || action == ACTION_FUSE);
}

bool action_is_action_performed(uint16_t champion_index, int16_t action)
{
    if (champion_index >= g_party_champion_count)
        return false;
    Champion *c = &g_champions[champion_index];
    if (!c->current_health)
        return false;
    int16_t x, y;
    step_forward(c->direction, &x, &y);
    g_action_target_group_thing = map_find_thing((Location){x, y}, THING_GROUP);
    const ActionInfo *info = tables_get_action_info(action);
    uint16_t disabled_ticks = info->disabled_ticks;
    uint16_t skill_index = info->skill_index;
    int16_t stamina = random_bit() + info->stamina;
    uint16_t experience = info->experience_gain;
    uint8_t square = map_get_square(x, y);
    bool performed = true;
    int16_t mana_cost = 0;
    if ((skill_index >= SKILL_COUNT - SKILL_BASE_COUNT && skill_index < SKILL_COUNT) || skill_index == SKILL_WIZARD)
        mana_cost = SPELL_ACTION_MAXIMUM_LEVEL + 1 -
                    min16(SPELL_ACTION_MAXIMUM_LEVEL, champion_get_skill_level(champion_index, skill_index));
    switch (is_removed_in_csb(action) ? ACTION_NONE : action) {
    case ACTION_CHOP:
    case ACTION_KICK:
    case ACTION_SWING:
    case ACTION_HACK:
    case ACTION_BERZERK:
    case ACTION_BASH:
        if (is_door_bashed(champion_index, x, y, square)) {
            disabled_ticks = DOOR_BASH_DISABLED_TICKS;
            break;
        }
        /* no closed door: a melee attack */
        /* fall through */
        __attribute__((fallthrough));
    case ACTION_PUNCH:
    case ACTION_STAB_1:
    case ACTION_STAB_2:
    case ACTION_THRUST:
    case ACTION_JAB:
    case ACTION_PARRY:
    case ACTION_DISRUPT:
    case ACTION_MELEE:
    case ACTION_SLASH:
    case ACTION_CLEAVE:
    case ACTION_STUN:
        performed = action_is_melee_action_performed(champion_index, action, x, y, skill_index);
        if (!performed) {
            experience = (int16_t)experience >> 1; /* signed shift as in the original */
            disabled_ticks >>= 1;
        }
        break;
    case ACTION_CONFUSE:
        action_decrement_charges(champion_index);
        /* fall through */
        __attribute__((fallthrough));
    case ACTION_BLOW_HORN:
    case ACTION_WAR_CRY:
    case ACTION_CALM:
    case ACTION_BRANDISH:
        if (action == ACTION_WAR_CRY)
            sound_request_play(SOUND_WAR_CRY, x, y, SOUND_MODE_NOW);
        if (action == ACTION_BLOW_HORN)
            sound_request_play(SOUND_BLOW_HORN, x, y, SOUND_MODE_NOW);
        performed = action_is_group_frightened_by_action(champion_index, action, x, y);
        break;
    case ACTION_FLIP:
        action_print_action_message(champion_index, gfx_get_string(random_bit() ? TEXT_FLIP_HEADS : TEXT_FLIP_TAILS));
        break;
    case ACTION_CLIMB_DOWN:
        if (!is_climbed_down())
            disabled_ticks = 0;
        break;
    case ACTION_FREEZE_LIFE:
        cast_freeze_life(champion_index);
        break;
    case ACTION_FIREBALL:
    case ACTION_DISPELL:
    case ACTION_LIGHTNING:
    case ACTION_SPIT:
    case ACTION_INVOKE:
        performed = is_explosion_action_cast(champion_index, action, mana_cost);
        if (!performed)
            experience = (int16_t)experience >> 1; /* signed shift as in the original */
        break;
    case ACTION_SHOOT:
        if (!is_shot(champion_index)) {
            g_action_damage = ACTION_DAMAGE_NO_AMMUNITION;
            experience = 0;
            performed = false;
        }
        break;
    case ACTION_SPELLSHIELD:
    case ACTION_FIRESHIELD:
        if (magic_is_party_spell_or_fire_shield_successful(champion_index, action == ACTION_SPELLSHIELD,
                                                           SHIELD_ACTION_TICKS, true)) {
            action_decrement_charges(champion_index);
        } else {
            experience = (int16_t)experience >> 2; /* signed shift as in the original */
            disabled_ticks >>= 1;
        }
        break;
    case ACTION_FLUXCAGE:
        champion_face_party_direction(c);
        projectile_fluxcage_action(x, y);
        break;
    case ACTION_HEAL:
        experience = heal_champion(champion_index, experience);
        break;
    case ACTION_LIGHT:
        g_party.magical_light_amount += tables_get_light_amount(LIGHT_ACTION_POWER);
        magic_create_light_event(-LIGHT_ACTION_POWER, LIGHT_ACTION_TICKS);
        action_decrement_charges(champion_index);
        break;
    case ACTION_WINDOW:
        open_window(champion_index, skill_index);
        action_decrement_charges(champion_index);
        break;
    case ACTION_THROW:
        performed = is_action_hand_object_thrown(champion_index);
        break;
    case ACTION_FUSE: {
        champion_face_party_direction(c);
        int16_t fuse_x, fuse_y;
        step_forward(g_party_direction, &fuse_x, &fuse_y);
        projectile_fuse_action(fuse_x, fuse_y);
        break;
    }
    }
    if (disabled_ticks)
        magic_disable_action(champion_index, disabled_ticks);
    if (stamina)
        champion_decrement_stamina(champion_index, stamina);
    if (experience)
        champion_add_skill_experience(champion_index, skill_index, experience);
    return performed;
}

/* Commands 113-115: the action defense applies while the action runs; the action menu closes. */
bool action_did_click_trigger_action(int16_t action_list_index)
{
    if (!g_acting_champion_ordinal || g_action_list_indices[action_list_index] == ACTION_NONE)
        return false;
    uint16_t champion_index = g_acting_champion_ordinal - 1;
    Champion *c = &g_champions[champion_index];
    uint8_t action = g_action_list_indices[action_list_index];
    c->action_defense += tables_get_action_info(action)->defense;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    bool performed = action_is_action_performed(champion_index, action);
    c->action_index = action;
    action_clear_acting_champion();
    return performed;
}

int16_t action_get_throwing_stamina_cost(uint16_t thing)
{
    uint16_t weight = inventory_get_object_weight(thing) >> 1;
    int16_t cost = bounded_value(1, weight, THROW_STAMINA_MAXIMUM);
    for (int16_t rest = weight - THROW_STAMINA_MAXIMUM; rest > 0; rest -= THROW_STAMINA_MAXIMUM)
        cost += rest >> 1;
    return cost;
}

/* The champion throws the object of the slot (or, with slot < 0, the leader hand object) from the left or right
   front cell. Kinetic energy comes from the strength, the weapon, and the throw level. */
bool action_is_object_thrown(uint16_t champion_index, int16_t slot, int16_t side)
{
    Champion *c = &g_champions[champion_index];
    uint16_t thing;
    int16_t strength;
    if (slot < 0) {
        if (g_leader_empty_handed)
            return false;
        thing = inventory_get_object_removed_from_leader_hand();
        /* The strength counts the thrown object as if it were in the action hand. */
        uint16_t action_hand = c->slots[SLOT_HAND_ACTION];
        c->slots[SLOT_HAND_ACTION] = thing;
        slot = SLOT_HAND_ACTION;
        strength = champion_get_strength(champion_index, slot);
        c->slots[SLOT_HAND_ACTION] = action_hand;
    } else {
        strength = champion_get_strength(champion_index, slot);
        thing = inventory_get_object_removed_from_slot(champion_index, slot);
        if (thing == THING_NONE)
            return false;
    }
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    champion_decrement_stamina(champion_index, action_get_throwing_stamina_cost(thing));
    magic_disable_action(champion_index, THROW_DISABLED_TICKS);
    int16_t experience = THROW_EXPERIENCE;
    int16_t weapon_energy = 1;
    if (THING_TYPE(thing) == THING_WEAPON) {
        experience = THROW_WEAPON_EXPERIENCE;
        const WeaponInfo *info = tables_get_weapon_info(weapon_type(thing));
        if (info->weapon_class < WEAPON_CLASS_KINETIC_THROW_LIMIT) {
            weapon_energy = info->kinetic_energy;
            experience += weapon_energy >> 2;
        }
    }
    champion_add_skill_experience(champion_index, SKILL_THROW, experience);
    int16_t level = champion_get_skill_level(champion_index, SKILL_THROW);
    uint16_t energy_random = random16() & THROW_ENERGY_RANDOM_MASK;
    uint16_t attack_random = random16() & THROW_ATTACK_RANDOM_MASK;
    int16_t energy = weapon_energy + strength;
    /* The kinetic energy is a byte. */
    uint8_t kinetic_energy = level + (energy >> 1) + energy_random + energy;
    uint8_t attack = bounded_value(THROW_ATTACK_MINIMUM, attack_random + level * WEAPON_CLASS_SKILL_LEVEL_STEP,
                                   THROW_ATTACK_MAXIMUM);
    uint8_t step_energy = max16(THROW_STEP_ENERGY_MINIMUM, THROW_STEP_ENERGY_BASE - level);
    projectile_create(thing, g_party_map_x, g_party_map_y, NORMALIZE(side + g_party_direction), g_party_direction,
                      kinetic_energy, attack, step_energy);
    g_projectile_disabled_movement_ticks = SHOT_DISABLED_MOVEMENT_TICKS;
    g_last_projectile_disabled_movement_direction = g_party_direction;
    return true;
}

bool action_is_leader_hand_object_thrown(int16_t side)
{
    if (g_leader_index == -1)
        return false;
    return action_is_object_thrown(g_leader_index, -1, side);
}

/* True when the weapon slot holds a bow and the ammunition slot an arrow, or a sling and a sling stone. */
bool action_is_ammunition_compatible_with_weapon(uint16_t champion_index, uint16_t weapon_slot,
                                                 uint16_t ammunition_slot)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t weapon = c->slots[weapon_slot];
    uint16_t ammunition = c->slots[ammunition_slot];
    if (THING_TYPE(weapon) != THING_WEAPON)
        return false;
    uint8_t weapon_class = tables_get_weapon_info(weapon_type(weapon))->weapon_class, needed;
    if (weapon_class >= WEAPON_CLASS_FIRST_BOW && weapon_class <= WEAPON_CLASS_LAST_BOW)
        needed = WEAPON_CLASS_BOW_AMMUNITION;
    else if (weapon_class >= WEAPON_CLASS_FIRST_SLING && weapon_class <= WEAPON_CLASS_LAST_SLING)
        needed = WEAPON_CLASS_SLING_AMMUNITION;
    else
        return false;
    return THING_TYPE(ammunition) == THING_WEAPON &&
           tables_get_weapon_info(weapon_type(ammunition))->weapon_class == needed;
}

/* Moves a weapon from the quiver to the slot. */
static bool has_weapon_moved_to_slot(uint16_t champion_index, uint16_t quiver_slot, uint16_t slot)
{
    if (THING_TYPE(g_champions[champion_index].slots[quiver_slot]) != THING_WEAPON)
        return false;
    inventory_add_object_in_slot(champion_index, inventory_get_object_removed_from_slot(champion_index, quiver_slot),
                                 slot);
    return true;
}

void action_move_weapon_from_quiver_to_slot(uint16_t champion_index, uint16_t slot)
{
    if (g_champions[champion_index].slots[slot] != THING_NONE ||
        has_weapon_moved_to_slot(champion_index, SLOT_QUIVER_LINE1_1, slot))
        return;
    for (uint16_t quiver = SLOT_QUIVER_LINE2_1; quiver <= SLOT_QUIVER_LINE2_2; quiver++) /* slots 7-9 */
        if (has_weapon_moved_to_slot(champion_index, quiver, slot))
            return;
}
