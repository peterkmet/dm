#include "inventory.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "ids.h"
#include "random.h"
#include "sound.h"
#include "ui.h"

/* KAOS data addresses. */
#define TORCH_CHARGE_TO_ICON_OFFSET 0x100ba0   /* g_torch_charge_to_icon_offset[16] */

/* Object info index of the first record of each object type. */
enum { INFO_SCROLL = 0, INFO_CONTAINER = 1, INFO_POTION = 2, INFO_WEAPON = 23, INFO_ARMOUR = 69, INFO_JUNK = 127 };

enum { EVENT_CHAMPION_SHIELD = 72 };
enum { SOUND_SWALLOW = 8 };
#define WATERSKIN_WATER 800
#define POTION_WATER_WATER 1600
#define FOOD_WATER_MAXIMUM 2048
#define CHEST_WEIGHT 50
#define SCROLL_WEIGHT 1
#define EMPTY_FLASK_WEIGHT 1
#define POTION_WEIGHT 3
#define JUNK_WATERSKIN 1                  /* each charge weighs 0.2 kg */
#define LIGHT_POWER_ILLUMULET 2
#define CURSED_SLOT_LAST 12               /* cursed objects lower luck in slots up to here */
#define CURSED_LUCK (-3)
#define SHIELD_POTION_STRONG 50           /* above this shield defense, a Ya potion adds a quarter */
#define MAXIMUM_POTION_MANA 900
#define HEAL_WOUND_TRIES 10
#define STAT_MANA STAT_COUNT              /* object_modifier: the change goes to the maximum mana */

int16_t g_inventory_champion_ordinal;
int16_t g_leader_empty_handed = 1;
uint16_t g_open_chest_slots[SLOT_CHEST_COUNT] = {THING_NONE, THING_NONE, THING_NONE, THING_NONE, THING_NONE, THING_NONE, THING_NONE, THING_NONE};

/* The object info record of an object thing, or -1. */
int16_t get_object_info_index(uint16_t thing)
{
    switch (THING_TYPE(thing)) {
    case THING_WEAPON: return INFO_WEAPON + weapon_type(thing);
    case THING_ARMOUR: return INFO_ARMOUR + armour_type(thing);
    case THING_SCROLL: return INFO_SCROLL;
    case THING_POTION: return INFO_POTION + potion_type(thing);
    case THING_CONTAINER: return INFO_CONTAINER + container_type(thing);
    case THING_JUNK: return INFO_JUNK + junk_type(thing);
    }
    return -1;
}

/* The object type (base icon), or -1 for no thing or a thing that is not an object. */
int16_t object_get_type(uint16_t thing)
{
    if (thing == THING_NONE)
        return -1;
    int16_t i = get_object_info_index(thing);
    return i == -1 ? -1 : tables_object_info(i)->type;
}

/* The icon shows the state: compass direction, lit torch, charges, worn jewels, closed scroll. */
int16_t object_get_icon_index(uint16_t thing)
{
    int16_t type = object_get_type(thing);
    if (type < 0 || type > ICON_LAST_WITH_STATE)
        return type;
    switch (type) {
    case ICON_COMPASS_NORTH:
        return type + g_party_direction;
    case ICON_TORCH_UNLIT:
        return weapon_is_lit(thing) ? type + kaos_u8(TORCH_CHARGE_TO_ICON_OFFSET + weapon_charges(thing)) : type;
    case ICON_WATERSKIN_EMPTY:
    case ICON_JEWEL_SYMAL:
    case ICON_ILLUMULET:
        return type + (junk_charges(thing) != 0);
    case ICON_FLAMITT:
    case ICON_EYE_OF_TIME:
    case ICON_STORMRING:
    case ICON_STAFF_OF_CLAWS:
    case ICON_BOLT_BLADE:
    case ICON_FURY:
        return type + (weapon_charges(thing) != 0);
    case ICON_SCROLL_OPEN:
        return type + scroll_is_closed(thing);
    }
    return type;
}

/* Tenths of a kilogram. A chest weighs 5 kg plus its contents. */
uint16_t get_object_weight(uint16_t thing)
{
    if (thing == THING_NONE)
        return 0;
    uint16_t weight = 0;
    switch (THING_TYPE(thing)) {
    case THING_WEAPON: return tables_weapon_info(weapon_type(thing))->weight;
    case THING_ARMOUR: return tables_armour_info(armour_type(thing))->weight;
    case THING_SCROLL: return SCROLL_WEIGHT;
    case THING_POTION: return potion_type(thing) == POTION_EMPTY_FLASK ? EMPTY_FLASK_WEIGHT : POTION_WEIGHT;
    case THING_CONTAINER:
        weight = CHEST_WEIGHT;
        for (uint16_t t = container_first_thing(thing); t != THING_END; t = get_next_thing(t))
            weight += get_object_weight(t);
        return weight;
    case THING_JUNK:
        weight = *tables_junk_weight(junk_type(thing));
        return junk_type(thing) == JUNK_WATERSKIN ? weight + junk_charges(thing) * 2 : weight;
    }
    return weight;
}

/* Sharp attacks use the sharp resistance (attributes bits 0-2) of the armour. */
uint16_t get_armour_defense(const ArmourInfo *info, int16_t use_sharp_defense)
{
    uint16_t defense = info->defense;
    return use_sharp_defense ? scale_value(defense, 3, (info->attributes & 7) + 4) : defense;
}

void champion_put_object_in_leader_hand(uint16_t thing)
{
    if (thing == THING_NONE)
        return;
    g_leader_empty_handed = 0;
    g_leader_hand_object = thing;
    if (g_leader_index != -1) {
        g_champions[g_leader_index].load += get_object_weight(thing);
        g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
    }
}

uint16_t champion_get_object_removed_from_leader_hand(void)
{
    uint16_t thing = g_leader_hand_object;
    g_leader_empty_handed = 1;
    if (thing != THING_NONE) {
        g_leader_hand_object = THING_NONE;
        if (g_leader_index != -1) {
            g_champions[g_leader_index].load -= get_object_weight(thing);
            g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
        }
    }
    return thing;
}

/* Mana bonus of staffs and wands in the action hand, from the Staff to the Sceptre of Lyf. */
static const int8_t staff_mana[] = {2, 1, 6, 4, 10, 8, 16, 7, 5};

/* The statistic (or STAT_MANA) and the bonus of an object in a slot. Returns 0 for no bonus. */
static int16_t object_modifier(uint16_t slot, int16_t icon, uint16_t thing, uint16_t *stat)
{
    uint16_t type = THING_TYPE(thing);
    *stat = STAT_LUCK;
    if ((type == THING_WEAPON || type == THING_ARMOUR) && slot <= CURSED_SLOT_LAST && object_is_cursed(thing))
        return CURSED_LUCK;
    if (icon == ICON_RABBITS_FOOT && slot < SLOT_COUNT)
        return 10;
    switch (slot) {
    case SLOT_HAND_ACTION:
        *stat = STAT_MANA;
        if (icon == ICON_MACE_OF_ORDER)
            return *stat = STAT_STRENGTH, 5;
        if (icon >= ICON_STAFF_OF_CLAWS && icon <= ICON_STAFF_OF_CLAWS_LAST)
            return 4;
        if (icon >= ICON_STAFF && icon <= ICON_SCEPTRE_OF_LYF)
            return staff_mana[icon - ICON_STAFF];
        return icon == ICON_VORPAL_BLADE ? 4 : icon == ICON_DELTA ? 1 : icon == ICON_INQUISITOR ? 2 : 0;
    case SLOT_LEGS:
        return icon == ICON_POWERTOWERS ? (*stat = STAT_STRENGTH, 10) : 0;
    case SLOT_HEAD:
        if (icon == ICON_CROWN_OF_NERRA)
            return *stat = STAT_WISDOM, 10;
        return icon == ICON_DEXHELM ? (*stat = STAT_DEXTERITY, 10) : 0;
    case SLOT_TORSO:
        if (icon == ICON_FLAMEBAIN)
            return *stat = STAT_ANTIFIRE, 12;
        return icon == ICON_CLOAK_OF_NIGHT ? (*stat = STAT_DEXTERITY, 8) : 0;
    case SLOT_NECK:
        if (icon == ICON_JEWEL_SYMAL || icon == ICON_JEWEL_SYMAL_WORN)
            return *stat = STAT_ANTIMAGIC, 15;
        if (icon == ICON_CLOAK_OF_NIGHT)
            return *stat = STAT_DEXTERITY, 8;
        return icon == ICON_MOONSTONE ? (*stat = STAT_MANA, 3) : 0;
    }
    return 0;
}

/* Statistic, luck, and mana changes of worn objects; sign is 1 to add, -1 to remove. */
static void apply_object_modifiers(Champion *c, uint16_t slot, int16_t icon, int16_t sign, uint16_t thing)
{
    uint16_t stat;
    int16_t delta = object_modifier(slot, icon, thing, &stat) * sign;
    if (!delta)
        return;
    if (stat == STAT_MANA)
        c->maximum_mana += delta;
    else
        for (int k = STAT_MAXIMUM; k <= STAT_MINIMUM; k++)
            c->statistics[stat][k] += delta;
    c->attributes |= CHAMPION_ATTR_STATISTICS | CHAMPION_ATTR_PANEL;
}

static uint16_t *slot_ptr(Champion *c, uint16_t slot)
{
    return slot < SLOT_COUNT ? &c->slots[slot] : &g_open_chest_slots[slot - SLOT_CHEST_FIRST];
}

static int is_illumulet(int16_t icon) { return icon == ICON_ILLUMULET || icon == ICON_ILLUMULET_WORN; }
static int is_neck_jewel(int16_t icon) { return icon >= ICON_JEWEL_SYMAL && icon <= ICON_ILLUMULET_WORN; }
static int is_scroll(int16_t icon) { return icon == ICON_SCROLL_OPEN || icon == ICON_SCROLL_CLOSED; }
static int is_torch(int16_t icon) { return icon >= ICON_TORCH_UNLIT && icon <= ICON_TORCH_LAST; }

/* Takes an object from a slot: bonuses end, a torch goes out, a scroll closes. */
uint16_t champion_get_object_removed_from_slot(uint16_t champion_index, uint16_t slot)
{
    Champion *c = &g_champions[champion_index];
    uint16_t *p = slot_ptr(c, slot), thing = *p;
    *p = THING_NONE;
    if (thing == THING_NONE)
        return THING_NONE;
    int inventory_open = champion_index + 1 == g_inventory_champion_ordinal;
    int16_t icon = object_get_icon_index(thing);
    apply_object_modifiers(c, slot, icon, -1, thing);
    if (slot == SLOT_NECK && is_neck_jewel(icon)) {
        junk_set_charges(thing, 0);
        if (is_illumulet(icon))
            g_party.magical_light_amount -= *tables_light_power_to_light_amount(LIGHT_POWER_ILLUMULET);
    }
    if (inventory_open)
        c->attributes |= CHAMPION_ATTR_VIEWPORT;
    if (slot <= SLOT_HAND_ACTION) {
        if (slot == SLOT_HAND_ACTION) {
            c->attributes |= CHAMPION_ATTR_ACTION_HAND;
            if (is_scroll(icon))
                scroll_set_closed(thing, 1);
        }
        if (is_torch(icon))
            weapon_set_lit(thing, 0);
        if (inventory_open && slot == SLOT_HAND_ACTION && (icon == ICON_CHEST_CLOSED || is_scroll(icon)))
            c->attributes |= CHAMPION_ATTR_PANEL;
    }
    c->load -= get_object_weight(thing);
    c->attributes |= CHAMPION_ATTR_LOAD;
    return thing;
}

/* Puts an object in a slot: bonuses start, a torch in a hand burns, a scroll opens. */
void champion_add_object_in_slot(uint16_t champion_index, uint16_t thing, uint16_t slot)
{
    if (thing == THING_NONE)
        return;
    Champion *c = &g_champions[champion_index];
    *slot_ptr(c, slot) = thing;
    c->load += get_object_weight(thing);
    c->attributes |= CHAMPION_ATTR_LOAD;
    int16_t icon = object_get_icon_index(thing);
    int inventory_open = champion_index + 1 == g_inventory_champion_ordinal;
    apply_object_modifiers(c, slot, icon, 1, thing);
    if (slot <= SLOT_HAND_ACTION) {
        if (slot == SLOT_HAND_ACTION) {
            c->attributes |= CHAMPION_ATTR_ACTION_HAND;
            if (is_scroll(icon))
                scroll_set_closed(thing, 0);
        }
        if (icon == ICON_TORCH_UNLIT)
            weapon_set_lit(thing, 1);
        else if (inventory_open && slot == SLOT_HAND_ACTION && (icon == ICON_CHEST_CLOSED || is_scroll(icon)))
            c->attributes |= CHAMPION_ATTR_PANEL;
    } else if (slot == SLOT_NECK && is_neck_jewel(icon)) {
        junk_set_charges(thing, 1);
        if (is_illumulet(icon))
            g_party.magical_light_amount += *tables_light_power_to_light_amount(LIGHT_POWER_ILLUMULET);
    }
    if (inventory_open)
        c->attributes |= CHAMPION_ATTR_VIEWPORT;
}

/* Slot boxes 0-7: the two hands of champions 0-3. 8 and up: slot (box - 8) of the open inventory.
   Swaps the leader hand object with the slot object when the slot accepts it. */
void champion_click_on_slot_box(uint16_t slot_box)
{
    enum { HAND_BOX_COUNT = 8 };
    uint16_t champion_index, slot, hand = g_leader_hand_object;
    if (slot_box < HAND_BOX_COUNT) {
        champion_index = slot_box >> 1;
        if (g_candidate_champion_ordinal || champion_index >= g_party_champion_count ||
            champion_index + 1 == g_inventory_champion_ordinal || !g_champions[champion_index].current_health)
            return;
        slot = slot_box & 1;
    } else {
        champion_index = g_inventory_champion_ordinal - 1;
        slot = slot_box - HAND_BOX_COUNT;
    }
    uint16_t thing = *slot_ptr(&g_champions[champion_index], slot);
    if (thing == THING_NONE && hand == THING_NONE)
        return;
    if (hand != THING_NONE && !(*tables_slot_mask(slot) & tables_object_info(get_object_info_index(hand))->allowed_slots))
        return;
    if (hand != THING_NONE)
        champion_get_object_removed_from_leader_hand();
    if (thing != THING_NONE) {
        champion_get_object_removed_from_slot(champion_index, slot);
        champion_put_object_in_leader_hand(thing);
    }
    if (hand != THING_NONE)
        champion_add_object_in_slot(champion_index, hand, slot);
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
    uint16_t power = potion_power(potion);
    uint16_t divisor = (uint16_t)((511 - power) / (((power + 1) >> 3) + 32)) >> 1;
    uint16_t amount = power / 25 + 8;
    switch (potion_type(potion)) {
    case POTION_ROS: adjust_statistic_current_value(c, STAT_DEXTERITY, amount); break;
    case POTION_KU: adjust_statistic_current_value(c, STAT_STRENGTH, power / 35 + 5); break;
    case POTION_DANE: adjust_statistic_current_value(c, STAT_WISDOM, amount); break;
    case POTION_NETA: adjust_statistic_current_value(c, STAT_VITALITY, amount); break;
    case POTION_ANTIVENIN: champion_unpoison(champion_index); break;
    case POTION_MON:
        c->current_stamina += min16(c->maximum_stamina - c->current_stamina, (uint16_t)c->maximum_stamina / divisor);
        break;
    case POTION_YA: {
        amount += amount >> 1;
        if (c->shield_defense > SHIELD_POTION_STRONG)
            amount >>= 2;
        c->shield_defense += amount;
        TimelineEvent e = {(uint32_t)g_party_map_index << 24 | (g_game_time + (uint16_t)(amount * amount)),
                           EVENT_CHAMPION_SHIELD, champion_index, amount, 0};
        timeline_add_event(&e);
        c->attributes |= CHAMPION_ATTR_STATUS_BOX;
        break;
    }
    case POTION_EE: {
        uint16_t mana = min16(MAXIMUM_POTION_MANA, c->current_mana + amount + (amount - 8));
        if (mana > (uint16_t)c->maximum_mana)
            mana -= (uint16_t)(mana - max16(c->current_mana, c->maximum_mana)) >> 1;
        c->current_mana = mana;
        break;
    }
    case POTION_VI:
        c->current_health += (uint16_t)c->maximum_health / divisor;
        heal_wounds(c, max16(1, power / 42));
        c->attributes |= CHAMPION_ATTR_WOUNDS | CHAMPION_ATTR_LOAD;
        break;
    case POTION_WATER:
        c->water = min16(c->water + POTION_WATER_WATER, FOOD_WATER_MAXIMUM);
        break;
    }
    potion_set_type(potion, POTION_EMPTY_FLASK);
}

/* The inventory champion eats or drinks the leader hand object. Food is used up, a potion
   leaves an empty flask, and a waterskin loses one charge. */
void process_click_on_mouth(void)
{
    uint16_t thing = g_leader_hand_object;
    if (g_leader_empty_handed || g_candidate_champion_ordinal ||
        !(tables_object_info(get_object_info_index(thing))->allowed_slots & OBJECT_EDIBLE))
        return;
    int16_t icon = object_get_icon_index(thing);
    uint16_t type = THING_TYPE(thing), weight = get_object_weight(thing);
    uint16_t champion_index = g_inventory_champion_ordinal - 1;
    Champion *c = &g_champions[champion_index];
    int consumed = 0;
    if (icon == ICON_WATERSKIN_EMPTY || icon == ICON_WATERSKIN_FULL) {
        if (!junk_charges(thing))
            return;
        c->water = min16(c->water + WATERSKIN_WATER, FOOD_WATER_MAXIMUM);
        junk_set_charges(thing, junk_charges(thing) - 1);
    } else if (type != THING_POTION) {
        thing_set_free(thing);
        consumed = 1;
    }
    if (consumed)
        champion_get_object_removed_from_leader_hand();
    if (type == THING_POTION)
        drink_potion(champion_index, thing);
    else if (icon >= ICON_FOOD_FIRST && icon <= ICON_FOOD_LAST)
        c->food = min16(c->food + *tables_food_value(icon - ICON_FOOD_FIRST), FOOD_WATER_MAXIMUM);
    if (c->current_stamina > c->maximum_stamina)
        c->current_stamina = c->maximum_stamina;
    if (c->current_health > c->maximum_health)
        c->current_health = c->maximum_health;
    if (!consumed) {
        /* The leader carries the hand object, also when another champion drinks. */
        g_champions[g_leader_index].load += get_object_weight(thing) - weight;
        g_champions[g_leader_index].attributes |= CHAMPION_ATTR_LOAD;
    }
    sound_request_play(SOUND_SWALLOW, g_party_map_x, g_party_map_y, 0);
    c->attributes |= CHAMPION_ATTR_STATISTICS;
}
