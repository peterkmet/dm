/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/inventory.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/arith.h"
#include "engine/champion.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/magic.h"
#include "engine/party.h"
#include "engine/random.h"
#include "engine/record.h"
#include "engine/sound.h"
#include "engine/timeline.h"
#include <stdbool.h>

#define WATERSKIN_WATER            800
#define FILLED_WATERSKIN_CHARGES   3
#define POTION_WATER_WATER         1600
#define FOOD_WATER_MAXIMUM         2048
#define CHEST_WEIGHT               50
#define SCROLL_WEIGHT              1
#define EMPTY_FLASK_WEIGHT         1
#define POTION_WEIGHT              3
#define LIGHT_POWER_ILLUMULET      2
#define CURSED_SLOT_LAST           12 /* cursed objects lower luck in slots up to here */
#define CURSED_LUCK                (-3)
#define HEAL_WOUND_TRIES           10
#define ARMOUR_SHARP_DEFENSE_BASE  4 /* sharp defense: defense * (sharp resistance + 4) / 8 */
#define ARMOUR_SHARP_DEFENSE_SHIFT 3
/* Object modifiers: the bonus of each object. */
#define RABBITS_FOOT_LUCK          10
#define MACE_OF_ORDER_STRENGTH     5
#define STAFF_OF_CLAWS_MANA        4
#define VORPAL_BLADE_MANA          4
#define DELTA_MANA                 1
#define INQUISITOR_MANA            2
#define POWERTOWERS_STRENGTH       10
#define CROWN_OF_NERRA_WISDOM      10
#define DEXHELM_DEXTERITY          10
#define FLAMEBAIN_ANTIFIRE         12
#define CLOAK_OF_NIGHT_DEXTERITY   8
#define JEWEL_SYMAL_ANTIMAGIC      15
#define MOONSTONE_MANA             3
/* Potions of power p: amount = p / 25 + 8, divisor = (511 - p) / ((p + 1) / 8 + 32) / 2. */
#define POTION_AMOUNT_DIVISOR      25
#define POTION_AMOUNT_BASE         8
#define POTION_DIVISOR_BASE        511
#define POTION_DIVISOR_POWER_SHIFT 3
#define POTION_DIVISOR_OFFSET      32
#define KU_STRENGTH_DIVISOR        35 /* Ku: strength + p / 35 + 5 */
#define KU_STRENGTH_BASE           5
#define VI_WOUND_ROLL_DIVISOR      42 /* Vi: max(1, p / 42) wound rolls */

uint16_t g_leader_hand_object = THING_NONE;

int16_t g_inventory_champion_index = -1;
bool g_leader_empty_handed = true;
uint16_t g_open_chest_slots[SLOT_CHEST_COUNT] = {THING_NONE, THING_NONE, THING_NONE, THING_NONE,
                                                 THING_NONE, THING_NONE, THING_NONE, THING_NONE};
uint16_t g_open_chest = THING_NONE;

int16_t inventory_get_object_info_index(uint16_t thing)
{
    switch (THING_TYPE(thing)) {
    case THING_WEAPON:
        return OBJECT_INFO_FIRST_WEAPON + rec_weapon_type(thing);
    case THING_ARMOUR:
        return OBJECT_INFO_FIRST_ARMOUR + rec_armour_type(thing);
    case THING_SCROLL:
        return OBJECT_INFO_FIRST_SCROLL;
    case THING_POTION:
        return OBJECT_INFO_FIRST_POTION + rec_potion_type(thing);
    case THING_CONTAINER:
        return OBJECT_INFO_FIRST_CONTAINER + rec_container_type(thing);
    case THING_JUNK:
        return OBJECT_INFO_FIRST_JUNK + rec_junk_type(thing);
    }
    return -1;
}

int16_t object_get_type(uint16_t thing)
{
    if (thing == THING_NONE)
        return -1;
    int16_t i = inventory_get_object_info_index(thing);
    return i == -1 ? -1 : tables_get_object_info(i)->type;
}

int16_t object_get_icon_index(uint16_t thing)
{
    int16_t type = object_get_type(thing);
    if (type < 0 || type > ICON_LAST_WITH_STATE)
        return type;
    switch (type) {
    case ICON_COMPASS_NORTH:
        return type + g_party_direction;
    case ICON_TORCH_UNLIT:
        return rec_weapon_is_lit(thing) ? type + tables_get_torch_icon_offset(rec_weapon_charges(thing)) : type;
    case ICON_WATERSKIN_EMPTY:
    case ICON_JEWEL_SYMAL:
    case ICON_ILLUMULET:
        return type + (rec_junk_charges(thing) != 0);
    case ICON_FLAMITT:
    case ICON_EYE_OF_TIME:
    case ICON_STORMRING:
    case ICON_STAFF_OF_CLAWS:
    case ICON_BOLT_BLADE:
    case ICON_FURY:
        return type + (rec_weapon_charges(thing) != 0);
    case ICON_SCROLL_OPEN:
        return type + rec_scroll_is_closed(thing);
    }
    return type;
}

uint16_t inventory_get_object_weight(uint16_t thing)
{
    if (thing == THING_NONE)
        return 0;
    uint16_t weight = 0;
    switch (THING_TYPE(thing)) {
    case THING_WEAPON:
        return tables_get_weapon_info(rec_weapon_type(thing))->weight;
    case THING_ARMOUR:
        return tables_get_armour_info(rec_armour_type(thing))->weight;
    case THING_SCROLL:
        return SCROLL_WEIGHT;
    case THING_POTION:
        return rec_potion_type(thing) == POTION_EMPTY_FLASK ? EMPTY_FLASK_WEIGHT : POTION_WEIGHT;
    case THING_CONTAINER:
        weight = CHEST_WEIGHT;
        for (uint16_t t = rec_container_first_thing(thing); t != THING_ENDOFLIST; t = rec_thing_next(t))
            weight += inventory_get_object_weight(t);
        return weight;
    case THING_JUNK:
        weight = tables_get_junk_weight(rec_junk_type(thing));
        return rec_junk_type(thing) == JUNK_WATERSKIN ? weight + rec_junk_charges(thing) * 2 : weight;
    }
    return weight;
}

uint16_t inventory_get_armour_defense(const ArmourInfo *info, bool use_sharp_defense)
{
    uint16_t defense = info->defense;
    return use_sharp_defense ? scale_value(defense, ARMOUR_SHARP_DEFENSE_SHIFT,
                                           (info->attributes & ARMOUR_SHARP_RESISTANCE) + ARMOUR_SHARP_DEFENSE_BASE)
                             : defense;
}

void inventory_put_object_in_leader_hand(uint16_t thing)
{
    if (thing == THING_NONE)
        return;
    g_leader_empty_handed = false;
    g_leader_hand_object = thing;
    if (g_leader_index != -1) {
        g_champions[g_leader_index].load += inventory_get_object_weight(thing);
        g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
    }
}

uint16_t inventory_get_object_removed_from_leader_hand(void)
{
    uint16_t thing = g_leader_hand_object;
    g_leader_empty_handed = true;
    if (thing != THING_NONE) {
        g_leader_hand_object = THING_NONE;
        if (g_leader_index != -1) {
            g_champions[g_leader_index].load -= inventory_get_object_weight(thing);
            g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
        }
    }
    return thing;
}

/* Mana bonus of staffs and wands in the action hand, from the Staff to the Sceptre of Lyf (icons 58-66). */
#define STAFF_MANA          2
#define WAND_MANA           1
#define TEOWAND_MANA        6
#define YEW_STAFF_MANA      4
#define STAFF_OF_MANAR_MANA 10
#define SNAKE_STAFF_MANA    8
#define CONDUIT_MANA        16
#define DRAGON_SPIT_MANA    7
#define SCEPTRE_OF_LYF_MANA 5
static const int8_t staff_mana[] = {STAFF_MANA,     WAND_MANA,           TEOWAND_MANA,
                                    YEW_STAFF_MANA, STAFF_OF_MANAR_MANA, SNAKE_STAFF_MANA,
                                    CONDUIT_MANA,   DRAGON_SPIT_MANA,    SCEPTRE_OF_LYF_MANA};

typedef struct {
    int16_t bonus;
    uint16_t statistic;
} ObjectModifier;

/* The statistic that an object raises and the bonus. */
static ObjectModifier stat_bonus(uint16_t statistic, int16_t bonus)
{
    return (ObjectModifier){bonus, statistic};
}

/* The statistic (or STAT_MANA) and the bonus of an object in a slot. The bonus is 0 for no bonus. */
static ObjectModifier get_object_modifier(uint16_t slot, int16_t icon, uint16_t thing)
{
    uint16_t type = THING_TYPE(thing);
    ObjectModifier none = {0, STAT_LUCK};
    if ((type == THING_WEAPON || type == THING_ARMOUR) && slot <= CURSED_SLOT_LAST && rec_object_is_cursed(thing))
        return stat_bonus(STAT_LUCK, CURSED_LUCK);
    if (icon == ICON_RABBITS_FOOT && slot < SLOT_COUNT)
        return stat_bonus(STAT_LUCK, RABBITS_FOOT_LUCK);
    switch (slot) {
    case SLOT_HAND_ACTION:
        if (icon == ICON_MACE_OF_ORDER)
            return stat_bonus(STAT_STRENGTH, MACE_OF_ORDER_STRENGTH);
        if (icon >= ICON_STAFF_OF_CLAWS && icon <= ICON_STAFF_OF_CLAWS_LAST)
            return stat_bonus(STAT_MANA, STAFF_OF_CLAWS_MANA);
        if (icon >= ICON_STAFF && icon <= ICON_SCEPTRE_OF_LYF)
            return stat_bonus(STAT_MANA, staff_mana[icon - ICON_STAFF]);
        return stat_bonus(STAT_MANA, icon == ICON_VORPAL_BLADE ? VORPAL_BLADE_MANA
                                     : icon == ICON_DELTA      ? DELTA_MANA
                                     : icon == ICON_INQUISITOR ? INQUISITOR_MANA
                                                               : 0);
    case SLOT_LEGS:
        return icon == ICON_POWERTOWERS ? stat_bonus(STAT_STRENGTH, POWERTOWERS_STRENGTH) : none;
    case SLOT_HEAD:
        if (icon == ICON_CROWN_OF_NERRA)
            return stat_bonus(STAT_WISDOM, CROWN_OF_NERRA_WISDOM);
        return icon == ICON_DEXHELM ? stat_bonus(STAT_DEXTERITY, DEXHELM_DEXTERITY) : none;
    case SLOT_TORSO:
        if (icon == ICON_FLAMEBAIN)
            return stat_bonus(STAT_ANTIFIRE, FLAMEBAIN_ANTIFIRE);
        return icon == ICON_CLOAK_OF_NIGHT ? stat_bonus(STAT_DEXTERITY, CLOAK_OF_NIGHT_DEXTERITY) : none;
    case SLOT_NECK:
        if (icon == ICON_JEWEL_SYMAL || icon == ICON_JEWEL_SYMAL_WORN)
            return stat_bonus(STAT_ANTIMAGIC, JEWEL_SYMAL_ANTIMAGIC);
        if (icon == ICON_CLOAK_OF_NIGHT)
            return stat_bonus(STAT_DEXTERITY, CLOAK_OF_NIGHT_DEXTERITY);
        return icon == ICON_MOONSTONE ? stat_bonus(STAT_MANA, MOONSTONE_MANA) : none;
    }
    return none;
}

/* Statistic, luck, and mana changes of worn objects; sign is 1 to add, -1 to remove. */
static void apply_object_modifiers(Champion *c, uint16_t slot, int16_t icon, int16_t sign, uint16_t thing)
{
    ObjectModifier modifier = get_object_modifier(slot, icon, thing);
    int16_t delta = modifier.bonus * sign;
    uint16_t stat = modifier.statistic;
    if (!delta)
        return;
    if (stat == STAT_MANA)
        c->maximum_mana += delta;
    else
        for (int k = STAT_MAXIMUM; k <= STAT_MINIMUM; k++)
            c->statistics[stat][k] += delta;
    c->attributes |= CHAMPION_ATTR_STATISTICS | CHAMPION_ATTR_PANEL;
}

static uint16_t *get_slot_pointer(Champion *c, uint16_t slot)
{
    return slot < SLOT_COUNT ? &c->slots[slot] : &g_open_chest_slots[slot - SLOT_CHEST_FIRST];
}

static bool is_illumulet(int16_t icon)
{
    return icon == ICON_ILLUMULET || icon == ICON_ILLUMULET_WORN;
}
static bool is_neck_jewel(int16_t icon)
{
    return icon >= ICON_JEWEL_SYMAL && icon <= ICON_ILLUMULET_WORN;
}
static bool is_scroll(int16_t icon)
{
    return icon == ICON_SCROLL_OPEN || icon == ICON_SCROLL_CLOSED;
}
static bool is_torch(int16_t icon)
{
    return icon >= ICON_TORCH_UNLIT && icon <= ICON_TORCH_LAST;
}

uint16_t inventory_get_object_removed_from_slot(uint16_t champion_index, uint16_t slot)
{
    Champion *c = &g_champions[champion_index];
    uint16_t *p = get_slot_pointer(c, slot);
    uint16_t thing = *p;
    *p = THING_NONE;
    if (thing == THING_NONE)
        return THING_NONE;
    bool inventory_open = champion_index == g_inventory_champion_index;
    int16_t icon = object_get_icon_index(thing);
    apply_object_modifiers(c, slot, icon, -1, thing);
    if (slot == SLOT_NECK && is_neck_jewel(icon)) {
        rec_junk_set_charges(thing, 0);
        if (is_illumulet(icon))
            g_party.magical_light_amount -= tables_get_light_amount(LIGHT_POWER_ILLUMULET);
    }
    if (inventory_open)
        c->attributes |= CHAMPION_ATTR_VIEWPORT;
    if (slot <= SLOT_HAND_ACTION) {
        if (slot == SLOT_HAND_ACTION) {
            c->attributes |= CHAMPION_ATTR_ACTION_HAND;
            if (is_scroll(icon))
                rec_scroll_set_closed(thing, true);
        }
        if (is_torch(icon))
            rec_weapon_set_lit(thing, false);
        if (inventory_open && slot == SLOT_HAND_ACTION && icon == ICON_CHEST_CLOSED && thing == g_open_chest)
            inventory_close_chest();
        if (inventory_open && slot == SLOT_HAND_ACTION && (icon == ICON_CHEST_CLOSED || is_scroll(icon)))
            c->attributes |= CHAMPION_ATTR_PANEL;
    }
    c->load -= inventory_get_object_weight(thing);
    c->attributes |= CHAMPION_ATTR_LOAD;
    return thing;
}

void inventory_add_object_in_slot(uint16_t champion_index, uint16_t thing, uint16_t slot)
{
    if (thing == THING_NONE)
        return;
    Champion *c = &g_champions[champion_index];
    *get_slot_pointer(c, slot) = thing;
    c->load += inventory_get_object_weight(thing);
    c->attributes |= CHAMPION_ATTR_LOAD;
    int16_t icon = object_get_icon_index(thing);
    bool inventory_open = champion_index == g_inventory_champion_index;
    apply_object_modifiers(c, slot, icon, 1, thing);
    if (slot <= SLOT_HAND_ACTION) {
        if (slot == SLOT_HAND_ACTION) {
            c->attributes |= CHAMPION_ATTR_ACTION_HAND;
            if (is_scroll(icon))
                rec_scroll_set_closed(thing, false);
        }
        if (icon == ICON_TORCH_UNLIT)
            rec_weapon_set_lit(thing, true);
        else if (inventory_open && slot == SLOT_HAND_ACTION && (icon == ICON_CHEST_CLOSED || is_scroll(icon)))
            c->attributes |= CHAMPION_ATTR_PANEL;
    } else if (slot == SLOT_NECK && is_neck_jewel(icon)) {
        rec_junk_set_charges(thing, 1);
        if (is_illumulet(icon))
            g_party.magical_light_amount += tables_get_light_amount(LIGHT_POWER_ILLUMULET);
    }
    if (inventory_open)
        c->attributes |= CHAMPION_ATTR_VIEWPORT;
}

void inventory_click_on_slot_box(uint16_t slot_box)
{
    enum { HAND_BOX_COUNT = 8 };
    uint16_t champion_index, slot, hand = g_leader_hand_object;
    if (slot_box < HAND_BOX_COUNT) {
        champion_index = slot_box >> 1;
        if (g_candidate_champion_index >= 0 || champion_index >= g_party_champion_count ||
            champion_index == g_inventory_champion_index || !g_champions[champion_index].current_health)
            return;
        slot = slot_box & 1;
    } else {
        if (g_inventory_champion_index < 0)
            return;
        champion_index = g_inventory_champion_index;
        slot = slot_box - HAND_BOX_COUNT;
    }
    uint16_t thing = *get_slot_pointer(&g_champions[champion_index], slot);
    if (thing == THING_NONE && hand == THING_NONE)
        return;
    if (hand != THING_NONE &&
        !(tables_get_slot_mask(slot) & tables_get_object_info(inventory_get_object_info_index(hand))->allowed_slots))
        return;
    if (hand != THING_NONE)
        inventory_get_object_removed_from_leader_hand();
    if (thing != THING_NONE) {
        inventory_get_object_removed_from_slot(champion_index, slot);
        inventory_put_object_in_leader_hand(thing);
    }
    if (hand != THING_NONE)
        inventory_add_object_in_slot(champion_index, hand, slot);
}

void inventory_open_chest(uint16_t chest)
{
    uint16_t thing = rec_container_first_thing(chest);
    g_open_chest = chest;
    for (int i = 0; i < SLOT_CHEST_COUNT; i++) {
        g_open_chest_slots[i] = thing == THING_ENDOFLIST ? THING_NONE : thing;
        if (thing != THING_ENDOFLIST)
            thing = rec_thing_next(thing);
    }
}

void inventory_close_chest(void)
{
    if (g_open_chest == THING_NONE)
        return;
    uint16_t chest = g_open_chest;
    uint16_t last = THING_NONE;
    g_open_chest = THING_NONE;
    rec_container_set_first_thing(chest, THING_ENDOFLIST);
    for (int i = 0; i < SLOT_CHEST_COUNT; i++) {
        uint16_t thing = g_open_chest_slots[i];
        if (thing == THING_NONE)
            continue;
        g_open_chest_slots[i] = THING_NONE;
        if (last == THING_NONE) {
            rec_thing_set_next(thing, THING_ENDOFLIST);
            rec_container_set_first_thing(chest, thing);
        } else {
            map_link_thing_to_list(thing, last, -1, 0);
        }
        last = thing;
    }
}

/* Heals wounds at random; tries again (up to 10 times) until at least one wound heals. */
static void heal_wounds(Champion *c, uint16_t rolls)
{
    uint16_t old_wounds = c->wounds;
    if (!old_wounds)
        return;
    int16_t tries = HEAL_WOUND_TRIES;
    do {
        for (uint16_t i = 0; i < rolls; i++)
            c->wounds &= random16();
        rolls = 1;
    } while (old_wounds == c->wounds && --tries);
}

/* The effect of a drunk potion. Stronger potions (power 0-255) have larger effects. */
static void drink_potion(uint16_t champion_index, uint16_t potion)
{
    Champion *c = &g_champions[champion_index];
    uint16_t power = rec_potion_power(potion);
    uint16_t divisor =
        (POTION_DIVISOR_BASE - power) / (((power + 1) >> POTION_DIVISOR_POWER_SHIFT) + POTION_DIVISOR_OFFSET) >> 1;
    uint16_t amount = power / POTION_AMOUNT_DIVISOR + POTION_AMOUNT_BASE;
    switch (rec_potion_type(potion)) {
    case POTION_ROS:
        champion_adjust_statistic(champion_index, STAT_DEXTERITY, amount);
        break;
    case POTION_KU:
        champion_adjust_statistic(champion_index, STAT_STRENGTH, power / KU_STRENGTH_DIVISOR + KU_STRENGTH_BASE);
        break;
    case POTION_DANE:
        champion_adjust_statistic(champion_index, STAT_WISDOM, amount);
        break;
    case POTION_NETA:
        champion_adjust_statistic(champion_index, STAT_VITALITY, amount);
        break;
    case POTION_ANTIVENIN:
        champion_unpoison(champion_index);
        break;
    case POTION_MON:
        /* unsigned division as in the original */
        c->current_stamina += min16(c->maximum_stamina - c->current_stamina, (uint16_t)c->maximum_stamina / divisor);
        break;
    case POTION_YA: {
        amount += amount >> 1;
        if (c->shield_defense > SHIELD_STRONG)
            amount >>= 2;
        c->shield_defense += amount;
        TimelineEvent e = {
            EVENT_MAP_TIME(g_party_map_index, g_game_time + (uint16_t)(amount * amount)), /* 16-bit product */
            EVENT_CHAMPION_SHIELD_END, champion_index, amount, 0};
        timeline_add_event(&e);
        c->attributes |= CHAMPION_ATTR_STATUS_BOX;
        break;
    }
    case POTION_EE: {
        uint16_t mana = min16(MAXIMUM_MANA, c->current_mana + amount + (amount - POTION_AMOUNT_BASE));
        /* unsigned compare and shift as in the original */
        if (mana > (uint16_t)c->maximum_mana)
            mana -= (uint16_t)(mana - max16(c->current_mana, c->maximum_mana)) >> 1;
        c->current_mana = mana;
        break;
    }
    case POTION_VI:
        c->current_health += (uint16_t)c->maximum_health / divisor; /* unsigned division as in the original */
        heal_wounds(c, max16(1, power / VI_WOUND_ROLL_DIVISOR));
        c->attributes |= CHAMPION_ATTR_WOUNDS | CHAMPION_ATTR_LOAD;
        break;
    case POTION_WATER:
        c->water = min16(c->water + POTION_WATER_WATER, FOOD_WATER_MAXIMUM);
        break;
    }
    rec_potion_set_type(potion, POTION_EMPTY_FLASK);
}

int16_t inventory_process_click_on_mouth(void)
{
    uint16_t thing = g_leader_hand_object;
    if (g_leader_empty_handed || g_candidate_champion_index >= 0 ||
        !(tables_get_object_info(inventory_get_object_info_index(thing))->allowed_slots & OBJECT_EDIBLE))
        return MOUTH_REFUSED;
    int16_t icon = object_get_icon_index(thing);
    uint16_t type = THING_TYPE(thing);
    uint16_t weight = inventory_get_object_weight(thing);
    uint16_t champion_index = g_inventory_champion_index;
    Champion *c = &g_champions[champion_index];
    bool consumed = false;
    if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL) {
        if (!rec_junk_charges(thing))
            return MOUTH_REFUSED;
        c->water = min16(c->water + WATERSKIN_WATER, FOOD_WATER_MAXIMUM);
        rec_junk_set_charges(thing, rec_junk_charges(thing) - 1);
    } else if (type != THING_POTION) {
        rec_thing_set_free(thing);
        consumed = true;
    }
    if (consumed)
        inventory_get_object_removed_from_leader_hand();
    if (type == THING_POTION)
        drink_potion(champion_index, thing);
    else if (icon >= ICON_FOOD_FIRST && icon <= ICON_FOOD_LAST)
        c->food = min16(c->food + tables_get_food_value(icon - ICON_FOOD_FIRST), FOOD_WATER_MAXIMUM);
    if (c->current_stamina > c->maximum_stamina)
        c->current_stamina = c->maximum_stamina;
    if (c->current_health > c->maximum_health)
        c->current_health = c->maximum_health;
    if (!consumed) {
        /* The leader carries the hand object, also when another champion drinks. */
        g_champions[g_leader_index].load += inventory_get_object_weight(thing) - weight;
        g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
    }
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    return consumed ? MOUTH_USED_UP : MOUTH_SWALLOWED;
}

void inventory_reset(void)
{
    g_inventory_champion_index = -1;
    g_open_chest = THING_NONE;
    for (int i = 0; i < SLOT_CHEST_COUNT; i++)
        g_open_chest_slots[i] = THING_NONE;
}

bool inventory_fill_leader_hand_at_fountain(void)
{
    uint16_t thing = g_leader_hand_object;
    int16_t icon = object_get_icon_index(thing);
    uint16_t weight = inventory_get_object_weight(thing);
    if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL)
        rec_junk_set_charges(thing, FILLED_WATERSKIN_CHARGES);
    else if (icon == ICON_EMPTY_FLASK)
        rec_potion_set_type(thing, POTION_WATER);
    else
        return false;
    g_champions[g_leader_index].load += inventory_get_object_weight(thing) - weight;
    g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
    return true;
}

void inventory_set_open_champion(int16_t champion_index)
{
    g_inventory_champion_index = champion_index;
}
