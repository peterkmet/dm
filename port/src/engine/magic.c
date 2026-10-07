#include <string.h>
#include "magic.h"
#include "spells.h"
#include "champion.h"
#include "dungeon.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "projectile.h"
#include "random.h"
#include "timeline_process.h"

#define JUNK_ZOKATHRA 51
#define POWER_SYMBOL_FIRST 0x60          /* symbol of power 1 is 0x60, power n is 0x5f + n */
#define SPELL_PROJECTILE_ATTACK 90
#define MESSAGE_COLOR_SPELL 4
#define SHIELD_STRONG 50                 /* above this a new shield adds a quarter */
#define SHIELD_MANA_COST 4
#define LIGHT_MAXIMUM_TICKS_DARKNESS 98
#define LIGHT_SPELL_BASE_TICKS 10000
#define MAGIC_TORCH_BASE_TICKS 2000
#define FIRE_SHIELD_BASE_TICKS 100
#define POTION_POWER_PER_LEVEL 40
#define LIGHT_POWER_MAXIMUM 15
#define MAGIC_MAP_TEXT "MAGICMAP"        /* the text of a magic map scroll */

static void add_party_event(uint8_t type, uint16_t ticks, uint16_t b)
{
    TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + ticks), .type = type, .b = b};
    timeline_add_event(&event);
}

/* Sets redraw flags on all champions. */
static void set_attributes_of_all(uint16_t attributes)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        g_champions[i].attributes |= attributes;
}

/* Light comes now; event 70 takes it back step by step after the ticks. */
void menus_create_event70_light(int16_t light_power, int16_t ticks)
{
    TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + ticks), .type = EVENT_LIGHT, .b = (uint16_t)light_power};
    timeline_add_event(&event);
    /* set_dungeon_view_palette: the view step recomputes the palette from the light amount. */
}

/* The action hand is disabled for the ticks; an earlier disable is merged into one event 11. */
void magic_disable_action(uint16_t champion_index, uint16_t ticks)
{
    Champion *c = &g_champions[champion_index];
    uint32_t time = g_game_time + ticks;
    if (c->enable_action_event_index < 0) {
        c->attributes |= CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_ACTION_DISABLED;
    } else {
        uint32_t old = g_timeline_events[c->enable_action_event_index].map_time & EVENT_TIME_MASK;
        if ((int32_t)time < (int32_t)old)
            time = old + (ticks >> 1);
        else
            time += (old - g_game_time) >> 1;
        timeline_delete_event((uint16_t)c->enable_action_event_index);
    }
    TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_party_map_index, time), .type = EVENT_ENABLE_CHAMPION_ACTION,
                           .priority = (uint8_t)champion_index};
    c->enable_action_event_index = timeline_add_event(&event);
}

/* Prints "<text> <champion name> <text>" for a failed cast. */
void menus_print_spell_failure_message(uint16_t champion_index, int16_t failure_type, uint16_t skill_index)
{
    int16_t first;
    if (failure_type == SPELL_FAILURE_NEEDS_MORE_PRACTICE) {
        if (skill_index >= SKILL_BASE_COUNT)
            skill_index = (skill_index - SKILL_BASE_COUNT) >> 2;
        first = skill_index == SKILL_WIZARD ? TEXT_SPELL_FAILURE_WIZARD : TEXT_SPELL_FAILURE_PRIEST;
    } else if (failure_type == SPELL_FAILURE_NEEDS_FLASK_IN_HAND) {
        first = TEXT_SPELL_FAILURE_FLASK;
    } else if (failure_type == SPELL_FAILURE_MEANINGLESS_SPELL) {
        first = TEXT_SPELL_FAILURE_MEANINGLESS;
    } else if (failure_type == SPELL_FAILURE_NEEDS_MAGIC_MAP) {
        first = TEXT_SPELL_FAILURE_MAGIC_MAP;
    } else {
        return;
    }
    message_new_line(g_game_time);
    message_print(MESSAGE_COLOR_SPELL, gfx_string(first), g_game_time);
    message_print(MESSAGE_COLOR_SPELL, g_champions[champion_index].name, g_game_time);
    message_print(MESSAGE_COLOR_SPELL, gfx_string(first + 1), g_game_time);
}

/* Spends the mana and shoots the spell; a weak cast with little maximum mana gets a slower step energy. */
int16_t magic_is_projectile_spell_cast(uint16_t champion_index, uint16_t thing, int16_t kinetic_energy, uint16_t required_mana)
{
    Champion *c = &g_champions[champion_index];
    if ((uint16_t)c->current_mana < required_mana)
        return 0;
    c->current_mana -= required_mana;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    int16_t mana_bonus = min16(8, c->maximum_mana >> 3);
    int16_t step_energy = 10 - mana_bonus;
    if (kinetic_energy < step_energy * 4) {
        kinetic_energy += 3;
        step_energy = 9 - mana_bonus;
    }
    projectile_shoot_from_champion((int16_t)champion_index, thing, (uint8_t)kinetic_energy, SPELL_PROJECTILE_ATTACK, (uint8_t)step_energy);
    return 1;
}

/* Spell shield (event 77) or fire shield (event 78) for the ticks. With mana, it costs 4 and half the ticks when short. */
int16_t menus_is_party_spell_or_fire_shield_successful(uint16_t champion_index, int16_t spell_shield, uint16_t ticks, int16_t use_mana)
{
    Champion *c = &g_champions[champion_index];
    int16_t success = 1;
    if (use_mana) {
        if (!c->current_mana)
            return 0;
        if (c->current_mana < SHIELD_MANA_COST) {
            ticks >>= 1;
            c->current_mana = 0;
            success = 0;
        } else {
            c->current_mana -= SHIELD_MANA_COST;
        }
    }
    int16_t defense = (int16_t)(ticks >> 5);
    int16_t *shield = spell_shield ? &g_party.spell_shield_defense : &g_party.fire_shield_defense;
    if (*shield > SHIELD_STRONG)
        defense >>= 2;
    *shield += defense;
    add_party_event(spell_shield ? EVENT_SPELL_SHIELD_END : EVENT_FIRE_SHIELD_END, ticks, (uint16_t)defense);
    set_attributes_of_all(CHAMPION_ATTR_STATUS_BOX);
    return success;
}

/* The empty flask in the action hand, else in the ready hand, or THING_NONE. */
static uint16_t empty_flask_in_hand(const Champion *c)
{
    for (int16_t slot = SLOT_HAND_ACTION; slot >= SLOT_HAND_READY; slot--)
        if (c->slots[slot] != THING_NONE && object_get_icon_index(c->slots[slot]) == ICON_EMPTY_FLASK)
            return c->slots[slot];
    return THING_NONE;
}

/* Fills an empty flask in a hand with the potion. Returns 0 without a flask. */
static int16_t cast_potion(uint16_t champion_index, uint16_t potion_type, int16_t power)
{
    Champion *c = &g_champions[champion_index];
    uint16_t flask = empty_flask_in_hand(c);
    if (flask == THING_NONE) {
        menus_print_spell_failure_message(champion_index, SPELL_FAILURE_NEEDS_FLASK_IN_HAND, 0);
        return 0;
    }
    uint16_t empty_weight = get_object_weight(flask);
    potion_set_type(flask, potion_type);
    potion_set_power(flask, power * POTION_POWER_PER_LEVEL + (random16() & 15));
    c->load += get_object_weight(flask) - empty_weight;
    if (champion_index + 1 == g_inventory_champion_ordinal)
        c->attributes |= CHAMPION_ATTR_LOAD;
    return 1;
}

/* The champion turns to the party direction and shoots the explosion of the spell. */
static void cast_projectile(uint16_t champion_index, uint16_t explosion_type, int16_t power, uint16_t skill_level)
{
    Champion *c = &g_champions[champion_index];
    if (c->direction != g_party_direction) {
        c->direction = (uint8_t)g_party_direction;
        c->attributes |= CHAMPION_ATTR_ICON;
    }
    if (explosion_type == EXPLOSION_OPEN_DOOR)
        skill_level *= 2;
    int16_t kinetic_energy = bounded_value(21, (int16_t)((power + 2) * (skill_level * 2 + 4)), 255);
    magic_is_projectile_spell_cast(champion_index, EXPLOSION_THING(explosion_type), kinetic_energy, 0);
}

/* Light now, taken back by event 70 after the ticks. */
static void cast_light(uint16_t light_power, int16_t ticks)
{
    g_party.magical_light_amount += *tables_light_power_to_light_amount(light_power);
    menus_create_event70_light(-(int16_t)light_power, ticks);
}

/* Zokathra: a new junk object in an empty hand, else on the party square. */
static void cast_zokathra(uint16_t champion_index)
{
    uint16_t thing = get_unused_thing(THING_JUNK);
    if (thing == THING_NONE)
        return;
    junk_set_type(thing, JUNK_ZOKATHRA);
    const Champion *c = &g_champions[champion_index];
    int16_t slot = c->slots[SLOT_HAND_READY] == THING_NONE ? SLOT_HAND_READY : c->slots[SLOT_HAND_ACTION] == THING_NONE ? SLOT_HAND_ACTION : -1;
    if (slot >= 0) {
        inventory_add_object_in_slot(champion_index, thing, (uint16_t)slot);
    } else {
        move_get_move_result(thing, -1, 0, g_party_map_x, g_party_map_y);
    }
}

/* Spells of kind "other": light, darkness, thieves eye, invisibility, shields, magic torch, footprints, zokathra. */
static void cast_other(uint16_t champion_index, uint16_t type, int16_t power)
{
    uint16_t ticks = (uint16_t)(power * 4 + 4);
    switch (type) {
    case SPELL_OTHER_LIGHT:
        cast_light((ticks >> 1) - 1, (int16_t)((power * 4 - 4) * 512 + LIGHT_SPELL_BASE_TICKS));
        break;
    case SPELL_OTHER_DARKNESS:
        g_party.magical_light_amount -= *tables_light_power_to_light_amount(ticks >> 2);
        menus_create_event70_light((int16_t)(ticks >> 2), LIGHT_MAXIMUM_TICKS_DARKNESS);
        break;
    case SPELL_OTHER_THIEVES_EYE:
        g_party.event73_count_thieves_eye++;
        ticks >>= 1;
        add_party_event(EVENT_THIEVES_EYE_END, (uint16_t)(ticks * ticks), 0);
        break;
    case SPELL_OTHER_INVISIBILITY:
        if (!g_party.event71_count_invisibility++) {
            if (g_inventory_champion_ordinal)
                g_champions[g_inventory_champion_ordinal - 1].attributes |= CHAMPION_ATTR_STATUS_BOX;
            set_attributes_of_all(CHAMPION_ATTR_ICON);
        }
        add_party_event(EVENT_INVISIBILITY_END, (uint16_t)(ticks * 8), 0);
        break;
    case SPELL_OTHER_PARTY_SHIELD: {
        int16_t defense = (int16_t)ticks;
        if (g_party.shield_defense > SHIELD_STRONG)
            defense >>= 2;
        g_party.shield_defense += defense;
        set_attributes_of_all(CHAMPION_ATTR_STATUS_BOX);
        add_party_event(EVENT_PARTY_SHIELD_END, (uint16_t)(ticks * ticks), (uint16_t)defense);
        break;
    }
    case SPELL_OTHER_MAGIC_TORCH:
        cast_light((ticks >> 2) + 1, (int16_t)((power * 4 + 1) * 128 + MAGIC_TORCH_BASE_TICKS));
        break;
    case SPELL_OTHER_FOOTPRINTS:
        g_party.event79_count_footprints++;
        g_party.first_scent_index = g_party.scent_count;
        g_party.last_scent_index = power < 3 ? g_party.scent_count : 0;
        add_party_event(EVENT_FOOTPRINTS_END, (uint16_t)(ticks * ticks), 0);
        break;
    case SPELL_OTHER_ZOKATHRA:
        cast_zokathra(champion_index);
        break;
    case SPELL_OTHER_FIRE_SHIELD:
        menus_is_party_spell_or_fire_shield_successful(champion_index, 0, (uint16_t)(ticks * ticks + FIRE_SHIELD_BASE_TICKS), 0);
        break;
    }
}

int magic_map_is_text(const char *text) { return !strcmp(text, MAGIC_MAP_TEXT); }

/* A magic map is a scroll with the text MAGICMAP. */
static int is_magic_map(uint16_t thing)
{
    char text[SCROLL_TEXT_SIZE];
    if (THING_TYPE(thing) != THING_SCROLL) return 0;
    decode_text(text, sizeof text, (uint16_t)(THING_TEXT << 10 | scroll_text_index(thing)), TEXT_MODE_SCROLL | TEXT_MODE_ANY_VISIBILITY);
    return magic_map_is_text(text);
}

/* CSB map spells need a magic map in the action hand. DAIN, ROS, and KU add a map layer for a time;
   NETA keeps the map of the party square and direction. Returns 0 without the map. */
static int16_t cast_magic_map(uint16_t champion_index, uint16_t type, int16_t power, uint16_t *disabled_ticks)
{
    Champion *c = &g_champions[champion_index];
    if (!is_magic_map(c->slots[SLOT_HAND_ACTION])) {
        menus_print_spell_failure_message(champion_index, SPELL_FAILURE_NEEDS_MAGIC_MAP, 0);
        return 0;
    }
    uint16_t d = (uint16_t)((power + 1) * 4), ticks;
    if (type == MAGIC_MAP_NETA) {
        for (uint16_t i = 0; c->magic_map[MAGIC_MAP_NETA] && i < g_event_high_water; i++)
            if (g_timeline_events[i].type == EVENT_MAGIC_MAP_NETA_END && g_timeline_events[i].priority == champion_index) {
                timeline_delete_event(i);
                break;
            }
        c->magic_map[MAGIC_MAP_NETA] = (uint8_t)(g_party_direction + 1);
        magic_map_set_position(c->magic_map, g_party_map_index, g_party_map_x, g_party_map_y);
        *disabled_ticks = (uint16_t)(d * d) >> 1;
        ticks = (uint16_t)(*disabled_ticks * 4);
    } else {
        if (type == MAGIC_MAP_ROS) d -= 2;
        else if (type == MAGIC_MAP_KU) d -= (d >> 2) - 1;
        c->magic_map[type]++;
        ticks = (uint16_t)(d * d * 2);
    }
    TimelineEvent event = {.map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + ticks), .type = (uint8_t)(EVENT_MAGIC_MAP_DAIN_END + type),
                           .priority = (uint8_t)champion_index};
    timeline_add_event(&event);
    return 1;
}

/* Casts the spell of the champion symbols. Each missing skill level is a chance to fail against wisdom. */
int16_t menus_get_champion_spell_cast_result(uint16_t champion_index)
{
    if (champion_index >= g_party_champion_count)
        return SPELL_CAST_FAILURE;
    Champion *c = &g_champions[champion_index];
    if (!c->current_health)
        return SPELL_CAST_FAILURE;
    int16_t index = spell_find((const uint8_t *)c->symbols);
    if (index < 0) {
        menus_print_spell_failure_message(champion_index, SPELL_FAILURE_MEANINGLESS_SPELL, 0);
        return SPELL_CAST_FAILURE;
    }
    const Spell *spell = tables_spell(index);
    uint8_t power_symbol = (uint8_t)c->symbols[0];
    int16_t power = (int8_t)power_symbol - (POWER_SYMBOL_FIRST - 1);
    uint16_t required_level = (uint16_t)(power + spell->base_required_skill_level);
    /* The original squares a stale register for the first term; the port uses required_level squared (spell_experience). */
    uint16_t experience = (uint16_t)(required_level * required_level + (power - 1) * spell->base_required_skill_level * 8 +
                                     required_level * 16 + (random16() & 7));
    uint16_t skill_level = (uint16_t)champion_get_skill_level((int16_t)champion_index, spell->skill_index);
    for (uint16_t missing = skill_level < required_level ? required_level - skill_level : 0; missing; missing--) {
        if (spell_draw_fails(c->statistics[STAT_WISDOM][STAT_CURRENT], random16())) {
            champion_add_skill_experience(champion_index, spell->skill_index, spell_failure_experience(experience, required_level, skill_level));
            menus_print_spell_failure_message(champion_index, SPELL_FAILURE_NEEDS_MORE_PRACTICE, spell->skill_index);
            return SPELL_CAST_FAILURE;
        }
    }
    uint16_t disabled_ticks = spell->disabled_ticks;
    switch (spell->kind) {
    case SPELL_KIND_POTION:
        if (!cast_potion(champion_index, spell->type, power))
            return SPELL_CAST_FAILURE_NEEDS_FLASK;
        break;
    case SPELL_KIND_PROJECTILE:
        cast_projectile(champion_index, spell->type, power, skill_level);
        break;
    case SPELL_KIND_OTHER:
        cast_other(champion_index, spell->type, power);
        break;
    case SPELL_KIND_MAGIC_MAP:
        if (!cast_magic_map(champion_index, spell->type, power, &disabled_ticks))
            return SPELL_CAST_FAILURE_NEEDS_FLASK; /* the symbols stay, as without a flask */
        break;
    }
    champion_add_skill_experience(champion_index, spell->skill_index, experience);
    magic_disable_action(champion_index, disabled_ticks);
    return SPELL_CAST_SUCCESS;
}

/* Cast button: casts for the magic caster; the symbols clear unless a flask is missing. */
int16_t menus_get_click_on_spell_cast_result(void)
{
    int16_t result = menus_get_champion_spell_cast_result((uint16_t)g_magic_caster_champion_index);
    if (result == SPELL_CAST_FAILURE_NEEDS_FLASK)
        return SPELL_CAST_FAILURE;
    Champion *c = &g_champions[g_magic_caster_champion_index];
    c->symbols[0] = 0;
    c->symbol_step = 0;
    g_redraw |= REDRAW_SPELL_SYMBOLS;
    return result;
}
