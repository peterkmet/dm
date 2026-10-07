#include "action.h"
#include "adventure.h"
#include "champion.h"
#include "dungeon.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "group.h"
#include "inventory.h"
#include "magic.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "projectile.h"
#include "random.h"
#include "sound.h"
#include "tables.h"
#include "timeline_process.h"

#define MESSAGE_COLOR_ACTION 4
#define MESSAGE_SIZE 100

#define SOUND_WAR_CRY 17
#define SOUND_BLOW_HORN 18

/* Weapon classes (WeaponInfo.weapon_class). */
enum {
    WEAPON_CLASS_BOW_AMMUNITION = 10, WEAPON_CLASS_SLING_AMMUNITION = 11, WEAPON_CLASS_KINETIC_THROW_LIMIT = 13,
    WEAPON_CLASS_FIRST_BOW = 16, WEAPON_CLASS_LAST_BOW = 31, WEAPON_CLASS_FIRST_SLING = 32, WEAPON_CLASS_LAST_SLING = 47
};

/* The weapon with this icon hits non-material creatures. */
#define ICON_WEAPON_HITS_NON_MATERIAL 40
#define HIT_PROBABILITY_NON_MATERIAL 0x8000   /* group_get_melee_action_damage: the hit can reach non-material creatures */

#define JUNK_MAGICAL_BOX_BLUE 42
#define JUNK_MAGICAL_BOX_GREEN 43
#define FREEZE_LIFE_TICKS_BLUE_BOX 30
#define FREEZE_LIFE_TICKS_GREEN_BOX 125
#define FREEZE_LIFE_TICKS_CHARGE 70
#define FREEZE_LIFE_TICKS_MAXIMUM 200

#define SPELL_ACTION_MAXIMUM_LEVEL 6     /* the mana cost of a spell action is 7 - min(6, skill level) */
#define FIREBALL_ACTION_ATTACK 150
#define DISPELL_ACTION_ATTACK 150
#define LIGHTNING_ACTION_ATTACK 180
#define SPIT_ACTION_ATTACK 250
#define INVOKE_ACTION_BASE_ATTACK 100
#define INVOKE_SPELL_COUNT 6
#define SHIELD_ACTION_TICKS 280
#define LIGHT_ACTION_POWER 2
#define LIGHT_ACTION_TICKS 2500
#define WINDOW_ACTION_BASE_TICKS 5
#define HEAL_ACTION_MAXIMUM_LEVEL 10
#define HEAL_ACTION_MANA_PER_STEP 2
#define DOOR_BASH_DISABLED_TICKS 6
#define DOOR_BASH_DESTRUCTION_TICKS 2
#define THROW_DISABLED_TICKS 4
#define THROW_EXPERIENCE 8
#define THROW_WEAPON_EXPERIENCE 12
#define THROW_STAMINA_MAXIMUM 10
#define PROJECTILE_DISABLED_MOVEMENT_TICKS 4
#define THROW_ATTACK_MINIMUM 40
#define THROW_ATTACK_MAXIMUM 200
#define THROW_STEP_ENERGY_BASE 11
#define THROW_STEP_ENERGY_MINIMUM 5
#define WEAPON_CLASS_SKILL_LEVEL_STEP 8
#define WEAPON_SHOOT_ATTACK_MASK 0xff     /* weapon info attributes bits 0-7: the attack of a launcher */
#define QUIVER_SLOT_SHIFT 8               /* event 11 b: slot + 1 for the next quiver weapon in the high byte */
#define EVENT_B_LOW_BYTE 0xff

uint16_t g_action_target_group_thing;
int16_t g_acting_champion_ordinal;
uint8_t g_action_list_indices[ACTION_LIST_COUNT] = { ACTION_NONE, ACTION_NONE, ACTION_NONE };
int16_t g_action_damage;

void action_clear_acting_champion(void)
{
    if (!g_acting_champion_ordinal) return;
    g_champions[g_acting_champion_ordinal - 1].attributes |= CHAMPION_ATTR_ACTION_HAND;
    g_acting_champion_ordinal = 0;
    g_redraw |= REDRAW_ACTION_AREA;
}

/* The square in front of (x, y) in the direction. */
static void step_forward(int16_t direction, int16_t *x, int16_t *y)
{
    *x = g_party_map_x;
    *y = g_party_map_y;
    update_map_coordinates_after_relative_movement(direction, 1, 0, x, y);
}

/* The champion turns to the party direction. */
static void champion_face_party_direction(Champion *c)
{
    if (c->direction != g_party_direction) {
        c->direction = (uint8_t)g_party_direction;
        c->attributes |= CHAMPION_ATTR_ICON;
    }
}

/* Prints "<text>" in the action color; "@p" in the text becomes the champion name. */
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
        message_print(MESSAGE_COLOR_ACTION, out, g_game_time);
}

/* The object in the action hand loses one charge: weapons, armour, and junk keep their charges in different bits. */
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
    /* champion_draw_changed_object_icons: the interface redraws the icons from the slots. */
}

/* War cry, blow horn, confuse, calm, brandish: the group in front flees when the influence roll beats its fear
   resistance. The champion gains influence experience, half of it when the group stays. */
int16_t action_is_group_frightened_by_action(uint16_t champion_index, int16_t action, int16_t x, int16_t y)
{
    uint16_t group = g_action_target_group_thing;
    if (group == THING_ENDOFLIST)
        return 0;
    uint16_t bonus = 0, experience = 0;
    switch (action) {
    case ACTION_CONFUSE: bonus = 12; experience = 45; break;
    case ACTION_WAR_CRY: bonus = 3; experience = 12; break;
    case ACTION_BLOW_HORN: bonus = 6; experience = 20; break;
    case ACTION_BRANDISH: bonus = 6; experience = 30; break;
    case ACTION_CALM: bonus = 7; experience = 35; break;
    }
    uint16_t level = (uint16_t)champion_get_skill_level((int16_t)champion_index, SKILL_INFLUENCE);
    const CreatureInfo *info = group_info(group);
    uint16_t roll = random16() % (uint16_t)(level + bonus);
    uint16_t fear = creature_fear_resistance(info);
    int16_t frightened = 0;
    if (roll < fear || fear == FEAR_RESISTANCE_NONE_FEARLESS) {
        experience >>= 1;
    } else {
        ActiveGroup *active = group_active(group);
        if (group_behavior(group) == BEHAVIOR_ATTACK) {
            group_stop_attacking(active, x, y);
            group_start_wandering(x, y);
        }
        group_set_behavior(group, BEHAVIOR_FLEE);
        active->delay_fleeing = (uint8_t)((uint16_t)((16 - fear) * 4) / info->movement_ticks);
        frightened = 1;
    }
    champion_add_skill_experience(champion_index, SKILL_INFLUENCE, experience);
    return frightened;
}

/* A melee hit on the group in front. A champion in a back cell reaches only past an empty front cell ("can't
   reach"). Disrupt hits only non-material creatures. Returns 0 when the champion hits nothing. */
int16_t action_is_melee_action_performed(uint16_t champion_index, int16_t action, int16_t x, int16_t y, int16_t skill_index)
{
    const Champion *c = &g_champions[champion_index];
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    if (g_action_target_group_thing == THING_ENDOFLIST)
        return 0;
    int16_t ordinal = group_get_melee_target_creature_ordinal(x, y, g_party_map_x, g_party_map_y, c->cell);
    if (!ordinal)
        return 0;
    uint16_t relative_cell = NORMALIZE(c->cell + 4 - c->direction);
    if (relative_cell >= 2) {
        int16_t front_cell = NORMALIZE(c->cell + (relative_cell == 3 ? 1 : 3));
        if (champion_get_index_in_cell(front_cell) != -1) {
            g_action_damage = ACTION_DAMAGE_CANT_REACH;
            return 0;
        }
    }
    if (action == ACTION_DISRUPT && !(get_creature_attributes(g_action_target_group_thing) & CREATURE_NON_MATERIAL))
        return 0;
    const ActionInfo *info = tables_action_info(action);
    uint16_t hit_probability = info->hit_probability;
    if (object_get_icon_index(c->slots[SLOT_HAND_ACTION]) == ICON_WEAPON_HITS_NON_MATERIAL || action == ACTION_DISRUPT)
        hit_probability |= HIT_PROBABILITY_NON_MATERIAL;
    g_action_damage = group_get_melee_action_damage((int16_t)champion_index, g_action_target_group_thing, ordinal - 1, x, y,
                                                    hit_probability, info->damage_factor, skill_index);
    return 1;
}

/* A spell action shoots the explosion. With less mana than the cost, the attack shrinks and all mana is used. */
static int16_t is_spell_action_cast(uint16_t champion_index, uint16_t explosion_type, int16_t attack, int16_t mana_cost)
{
    Champion *c = &g_champions[champion_index];
    champion_face_party_direction(c);
    if (c->current_mana < mana_cost) {
        attack = max16(2, (int16_t)(c->current_mana * attack) / mana_cost);
        mana_cost = c->current_mana;
    }
    int16_t cast = magic_is_projectile_spell_cast(champion_index, EXPLOSION_THING(explosion_type), attack, (uint16_t)mana_cost);
    action_decrement_charges(champion_index);
    return cast;
}

/* The bow or sling in the action hand shoots the ammunition of the ready hand. Returns 0 without matching
   ammunition. */
static int16_t is_shot(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    uint16_t ammunition = c->slots[SLOT_HAND_READY];
    if (THING_TYPE(ammunition) != THING_WEAPON)
        return 0;
    const WeaponInfo *launcher = tables_weapon_info(weapon_type(c->slots[SLOT_HAND_ACTION]));
    const WeaponInfo *missile = tables_weapon_info(weapon_type(ammunition));
    int16_t step_energy = launcher->weapon_class;
    if (step_energy >= WEAPON_CLASS_FIRST_BOW && step_energy <= WEAPON_CLASS_LAST_BOW) {
        if (missile->weapon_class != WEAPON_CLASS_BOW_AMMUNITION)
            return 0;
        step_energy -= WEAPON_CLASS_FIRST_BOW;
    } else if (step_energy >= WEAPON_CLASS_FIRST_SLING && step_energy <= WEAPON_CLASS_LAST_SLING) {
        if (missile->weapon_class != WEAPON_CLASS_SLING_AMMUNITION)
            return 0;
        step_energy -= WEAPON_CLASS_FIRST_SLING;
    }
    champion_face_party_direction(c);
    uint16_t thing = inventory_get_object_removed_from_slot(champion_index, SLOT_HAND_READY);
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    int16_t level = champion_get_skill_level((int16_t)champion_index, SKILL_SHOOT);
    /* The attack is a byte: the launcher shoot attack plus the level, doubled. */
    projectile_shoot_from_champion((int16_t)champion_index, thing, (uint8_t)(missile->kinetic_energy + launcher->kinetic_energy),
                              (uint8_t)((level + (launcher->attributes & WEAPON_SHOOT_ATTACK_MASK)) << 1), (uint8_t)step_energy);
    return 1;
}

/* Freeze Life stops the creatures. A magical box is used up; other objects use a charge. */
static void freeze_life(uint16_t champion_index)
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
    g_party.freeze_life_ticks = (uint8_t)min16(FREEZE_LIFE_TICKS_MAXIMUM, ticks + g_party.freeze_life_ticks);
}

/* Heal changes mana into health: up to min(10, heal level) health for each 2 mana. Returns the experience. */
static uint16_t heal(uint16_t champion_index, uint16_t experience)
{
    Champion *c = &g_champions[champion_index];
    int16_t missing = c->maximum_health - c->current_health;
    if (missing <= 0 || !c->current_mana)
        return experience;
    int16_t step = min16(HEAL_ACTION_MAXIMUM_LEVEL, champion_get_skill_level((int16_t)champion_index, SKILL_HEAL));
    experience = 2;
    for (;;) {
        int16_t amount = min16(missing, step);
        c->current_health += amount;
        experience += 2;
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
static void window(uint16_t champion_index, uint16_t skill_index)
{
    uint16_t random = random16();
    int16_t level = champion_get_skill_level((int16_t)champion_index, skill_index);
    uint16_t ticks = random % (uint16_t)(level + 8) + WINDOW_ACTION_BASE_TICKS;
    TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + ticks), .type = EVENT_THIEVES_EYE_END};
    timeline_add_event(&event);
    g_party.event73_count_thieves_eye++;
}

/* Climb down: the party goes down the pit in front with the rope, without fall damage. Returns 0 without a pit. */
static int16_t is_climbed_down(void)
{
    int16_t x, y;
    step_forward(g_party_direction, &x, &y);
    if (SQUARE_TYPE(get_square(x, y)) != SQUARE_PIT || group_get_thing(x, y) != THING_ENDOFLIST)
        return 0;
    g_use_rope_to_climb_down_pit = 1;
    move_get_move_result(THING_NONE, g_party_map_x, g_party_map_y, x, y);
    g_use_rope_to_climb_down_pit = 0;
    return 1;
}

/* CSB has no code for invoke, fluxcage, and fuse: they only disable the hand, take stamina, and give experience. */
static int16_t is_removed_in_csb(int16_t action)
{
    return g_adventure == ADVENTURE_CSB && (action == ACTION_INVOKE || action == ACTION_FLUXCAGE || action == ACTION_FUSE);
}

/* Runs one action of a champion against the square in front of the champion. After the action the action hand is
   disabled for the action ticks, the champion loses stamina, and the action skill gains experience. A failed
   melee action halves the ticks and the experience. Returns 0 when the action fails. */
int16_t action_is_action_performed(uint16_t champion_index, int16_t action)
{
    if (champion_index >= g_party_champion_count)
        return 0;
    Champion *c = &g_champions[champion_index];
    if (!c->current_health)
        return 0;
    int16_t x, y;
    step_forward(c->direction, &x, &y);
    g_action_target_group_thing = group_get_thing(x, y);
    const ActionInfo *info = tables_action_info(action);
    uint16_t disabled_ticks = info->disabled_ticks, skill_index = info->skill_index;
    int16_t stamina = (int16_t)(random_bit() + info->stamina);
    uint16_t experience = info->experience_gain;
    uint8_t square = get_square(x, y);
    int16_t performed = 1, mana_cost = 0;
    if ((skill_index >= SKILL_COUNT - SKILL_BASE_COUNT && skill_index < SKILL_COUNT) || skill_index == SKILL_WIZARD)
        mana_cost = SPELL_ACTION_MAXIMUM_LEVEL + 1 - min16(SPELL_ACTION_MAXIMUM_LEVEL, champion_get_skill_level((int16_t)champion_index, skill_index));
    switch (is_removed_in_csb(action) ? ACTION_NONE : action) {
    case ACTION_CHOP: case ACTION_KICK: case ACTION_SWING: case ACTION_HACK: case ACTION_BERZERK: case ACTION_BASH:
        if (SQUARE_TYPE(square) == SQUARE_DOOR && door_state(square) == DOOR_CLOSED) {
            sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
            disabled_ticks = DOOR_BASH_DISABLED_TICKS;
            event_is_door_destroyed_by_attack(x, y, champion_get_strength((int16_t)champion_index, SLOT_HAND_ACTION), 0,
                                              DOOR_BASH_DESTRUCTION_TICKS);
            sound_request_play(SOUND_WOODEN_THUD, g_party_map_x, g_party_map_y, SOUND_MODE_ONE_TICK_LATER);
            break;
        }
        /* fall through: no closed door, a melee attack */
    case ACTION_PUNCH: case ACTION_STAB_1: case ACTION_STAB_2: case ACTION_THRUST: case ACTION_JAB: case ACTION_PARRY:
    case ACTION_DISRUPT: case ACTION_MELEE: case ACTION_SLASH: case ACTION_CLEAVE: case ACTION_STUN:
        performed = action_is_melee_action_performed(champion_index, action, x, y, (int16_t)skill_index);
        if (!performed) {
            experience = (uint16_t)((int16_t)experience >> 1);
            disabled_ticks >>= 1;
        }
        break;
    case ACTION_CONFUSE:
        action_decrement_charges(champion_index);
        /* fall through */
    case ACTION_BLOW_HORN: case ACTION_WAR_CRY: case ACTION_CALM: case ACTION_BRANDISH:
        if (action == ACTION_WAR_CRY)
            sound_request_play(SOUND_WAR_CRY, x, y, SOUND_MODE_NOW);
        if (action == ACTION_BLOW_HORN)
            sound_request_play(SOUND_BLOW_HORN, x, y, SOUND_MODE_NOW);
        performed = action_is_group_frightened_by_action(champion_index, action, x, y);
        break;
    case ACTION_FLIP:
        action_print_action_message(champion_index, gfx_string(random_bit() ? TEXT_FLIP_HEADS : TEXT_FLIP_TAILS));
        break;
    case ACTION_CLIMB_DOWN:
        if (!is_climbed_down())
            disabled_ticks = 0;
        break;
    case ACTION_FREEZE_LIFE:
        freeze_life(champion_index);
        break;
    case ACTION_FIREBALL:
    case ACTION_DISPELL:
    case ACTION_LIGHTNING:
    case ACTION_SPIT:
    case ACTION_INVOKE: {
        int16_t attack = 0;
        uint16_t type = EXPLOSION_FIREBALL;
        switch (action) {
        case ACTION_FIREBALL: attack = FIREBALL_ACTION_ATTACK; break;
        case ACTION_DISPELL: attack = DISPELL_ACTION_ATTACK; type = EXPLOSION_HARM_NON_MATERIAL; break;
        case ACTION_LIGHTNING: attack = LIGHTNING_ACTION_ATTACK; type = EXPLOSION_LIGHTNING_BOLT; break;
        case ACTION_SPIT: attack = SPIT_ACTION_ATTACK; break;
        case ACTION_INVOKE: {
            attack = (int16_t)((random16() & 127) + INVOKE_ACTION_BASE_ATTACK);
            uint16_t spell = random16() % INVOKE_SPELL_COUNT;
            type = spell == 0 ? EXPLOSION_POISON_BOLT : spell == 1 ? EXPLOSION_POISON_CLOUD
                 : spell == 2 ? EXPLOSION_HARM_NON_MATERIAL : EXPLOSION_FIREBALL;
            break;
        }
        }
        performed = is_spell_action_cast(champion_index, type, attack, mana_cost);
        if (!performed)
            experience = (uint16_t)((int16_t)experience >> 1);
        break;
    }
    case ACTION_SHOOT:
        if (!is_shot(champion_index)) {
            g_action_damage = ACTION_DAMAGE_NO_AMMUNITION;
            experience = 0;
            performed = 0;
        }
        break;
    case ACTION_SPELLSHIELD:
    case ACTION_FIRESHIELD:
        if (magic_is_party_spell_or_fire_shield_successful(champion_index, action == ACTION_SPELLSHIELD, SHIELD_ACTION_TICKS, 1)) {
            action_decrement_charges(champion_index);
        } else {
            experience = (uint16_t)((int16_t)experience >> 2);
            disabled_ticks >>= 1;
        }
        break;
    case ACTION_FLUXCAGE:
        champion_face_party_direction(c);
        projectile_fluxcage_action(x, y);
        break;
    case ACTION_HEAL:
        experience = heal(champion_index, experience);
        break;
    case ACTION_LIGHT:
        g_party.magical_light_amount += *tables_light_power_to_light_amount(LIGHT_ACTION_POWER);
        magic_create_light_event(-LIGHT_ACTION_POWER, LIGHT_ACTION_TICKS);
        action_decrement_charges(champion_index);
        break;
    case ACTION_WINDOW:
        window(champion_index, skill_index);
        action_decrement_charges(champion_index);
        break;
    case ACTION_THROW: {
        champion_face_party_direction(c);
        int16_t side = NORMALIZE(g_party_direction + 1) == c->cell || NORMALIZE(g_party_direction + 2) == c->cell;
        performed = action_is_object_thrown(champion_index, SLOT_HAND_ACTION, side);
        if (performed) /* event 11 moves the next weapon from the quiver to the action hand */
            g_timeline_events[c->enable_action_event_index].b = (uint16_t)((SLOT_HAND_ACTION + 1) << QUIVER_SLOT_SHIFT |
                                                                           (g_timeline_events[c->enable_action_event_index].b & EVENT_B_LOW_BYTE));
        break;
    }
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
        champion_decrement_stamina((int16_t)champion_index, stamina);
    if (experience)
        champion_add_skill_experience(champion_index, skill_index, experience);
    return performed;
}

/* Commands 113-115: the action defense applies while the action runs; the action menu closes. */
int16_t action_did_click_trigger_action(int16_t action_list_index)
{
    if (!g_acting_champion_ordinal || g_action_list_indices[action_list_index] == ACTION_NONE)
        return 0;
    uint16_t champion_index = (uint16_t)(g_acting_champion_ordinal - 1);
    Champion *c = &g_champions[champion_index];
    uint8_t action = g_action_list_indices[action_list_index];
    c->action_defense += tables_action_info(action)->defense;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    int16_t performed = action_is_action_performed(champion_index, action);
    c->action_index = action;
    action_clear_acting_champion();
    return performed;
}

/* Stamina cost of a throw: half the weight, 1 to 10, plus half of each 10 above. */
int16_t action_get_throwing_stamina_cost(uint16_t thing)
{
    uint16_t weight = inventory_get_object_weight(thing) >> 1;
    int16_t cost = bounded_value(1, (int16_t)weight, THROW_STAMINA_MAXIMUM);
    for (int16_t rest = (int16_t)weight - 10; rest > 0; rest -= 10)
        cost += rest >> 1;
    return cost;
}

/* The champion throws the object of the slot (or, with slot < 0, the leader hand object) from the left or right
   front cell. Kinetic energy comes from the strength, the weapon, and the throw level. */
int16_t action_is_object_thrown(uint16_t champion_index, int16_t slot, int16_t side)
{
    Champion *c = &g_champions[champion_index];
    uint16_t thing;
    int16_t strength;
    if (slot < 0) {
        if (g_leader_empty_handed)
            return 0;
        thing = inventory_get_object_removed_from_leader_hand();
        /* The strength counts the thrown object as if it were in the action hand. */
        uint16_t action_hand = c->slots[SLOT_HAND_ACTION];
        c->slots[SLOT_HAND_ACTION] = thing;
        slot = SLOT_HAND_ACTION;
        strength = champion_get_strength((int16_t)champion_index, slot);
        c->slots[SLOT_HAND_ACTION] = action_hand;
    } else {
        strength = champion_get_strength((int16_t)champion_index, slot);
        thing = inventory_get_object_removed_from_slot(champion_index, (uint16_t)slot);
        if (thing == THING_NONE)
            return 0;
    }
    sound_request_play(SOUND_SWING, g_party_map_x, g_party_map_y, SOUND_MODE_AFTER_TICK);
    champion_decrement_stamina((int16_t)champion_index, action_get_throwing_stamina_cost(thing));
    magic_disable_action(champion_index, THROW_DISABLED_TICKS);
    int16_t experience = THROW_EXPERIENCE, weapon_energy = 1;
    if (THING_TYPE(thing) == THING_WEAPON) {
        experience = THROW_WEAPON_EXPERIENCE;
        const WeaponInfo *info = tables_weapon_info(weapon_type(thing));
        if (info->weapon_class < WEAPON_CLASS_KINETIC_THROW_LIMIT) {
            weapon_energy = info->kinetic_energy;
            experience += weapon_energy >> 2;
        }
    }
    champion_add_skill_experience(champion_index, SKILL_THROW, (uint16_t)experience);
    int16_t level = champion_get_skill_level((int16_t)champion_index, SKILL_THROW);
    uint16_t energy_random = random16() & 15;
    uint16_t attack_random = random16() & 31;
    int16_t energy = weapon_energy + strength;
    /* The kinetic energy is a byte. */
    uint8_t kinetic_energy = (uint8_t)(level + (energy >> 1) + energy_random + energy);
    uint8_t attack = (uint8_t)bounded_value(THROW_ATTACK_MINIMUM, (int16_t)(attack_random + level * WEAPON_CLASS_SKILL_LEVEL_STEP),
                                            THROW_ATTACK_MAXIMUM);
    uint8_t step_energy = (uint8_t)max16(THROW_STEP_ENERGY_MINIMUM, THROW_STEP_ENERGY_BASE - level);
    projectile_create(thing, g_party_map_x, g_party_map_y, NORMALIZE(side + g_party_direction), g_party_direction,
                      kinetic_energy, attack, step_energy);
    g_projectile_disabled_movement_ticks = PROJECTILE_DISABLED_MOVEMENT_TICKS;
    g_last_projectile_disabled_movement_direction = g_party_direction;
    return 1;
}

/* The leader throws the object in the leader hand (a click in the upper dungeon view). */
int16_t action_is_leader_hand_object_thrown(int16_t side)
{
    if (g_leader_index == -1)
        return 0;
    return action_is_object_thrown((uint16_t)g_leader_index, -1, side);
}

/* 1 when the weapon slot holds a bow and the ammunition slot an arrow, or a sling and a sling stone. */
int16_t action_is_ammunition_compatible_with_weapon(uint16_t champion_index, uint16_t weapon_slot, uint16_t ammunition_slot)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t weapon = c->slots[weapon_slot], ammunition = c->slots[ammunition_slot];
    if (THING_TYPE(weapon) != THING_WEAPON)
        return 0;
    uint8_t weapon_class = tables_weapon_info(weapon_type(weapon))->weapon_class, needed;
    if (weapon_class >= WEAPON_CLASS_FIRST_BOW && weapon_class <= WEAPON_CLASS_LAST_BOW)
        needed = WEAPON_CLASS_BOW_AMMUNITION;
    else if (weapon_class >= WEAPON_CLASS_FIRST_SLING && weapon_class <= WEAPON_CLASS_LAST_SLING)
        needed = WEAPON_CLASS_SLING_AMMUNITION;
    else
        return 0;
    return THING_TYPE(ammunition) == THING_WEAPON && tables_weapon_info(weapon_type(ammunition))->weapon_class == needed;
}

/* Moves a weapon from the quiver to the slot. */
static int16_t has_weapon_moved_to_slot(uint16_t champion_index, uint16_t quiver_slot, uint16_t slot)
{
    if (THING_TYPE(g_champions[champion_index].slots[quiver_slot]) != THING_WEAPON)
        return 0;
    inventory_add_object_in_slot(champion_index, inventory_get_object_removed_from_slot(champion_index, quiver_slot), slot);
    return 1;
}

/* After a throw: the next weapon of the quiver goes to the empty slot (quiver line 1 first, then line 2). */
void action_move_weapon_from_quiver_to_slot(uint16_t champion_index, uint16_t slot)
{
    if (g_champions[champion_index].slots[slot] != THING_NONE || has_weapon_moved_to_slot(champion_index, SLOT_QUIVER_LINE1_1, slot))
        return;
    for (uint16_t quiver = SLOT_QUIVER_LINE2_1; quiver <= SLOT_QUIVER_LINE2_2; quiver++)   /* slots 7-9 */
        if (has_weapon_moved_to_slot(champion_index, quiver, slot))
            return;
}
