/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine/champion.h"
#include "data/adventure.h"
#include "data/bytes.h"
#include "data/graphics.h"
#include "data/ids.h"
#include "data/tables.h"
#include "engine/action.h"
#include "engine/arith.h"
#include "engine/event.h"
#include "engine/game.h"
#include "engine/group.h"
#include "engine/inventory.h"
#include "engine/map.h"
#include "engine/message.h"
#include "engine/move.h"
#include "engine/party.h"
#include "engine/random.h"
#include "engine/record.h"
#include "engine/recruit.h"
#include "engine/sound.h"
#include "engine/timeline.h"
#include <string.h>

/* Statistic gain limits, factors and luck steps. */
#define STAT_HIGH               120 /* statistic gains above this value are halved */
#define STAT_VERY_HIGH          150 /* statistic gains above this value are quartered */
#define ATTACK_FACTOR_MIN       16  /* below this, the statistic factor divides the attack by 8 */
#define ATTACK_FACTOR_SHIFT     7
#define LUCK_STEP               2   /* luck change after each luck test */
#define PERCENT_RANGE           100 /* a luck test draws random16() % 100 */
#define ATTACK_LOW_FACTOR_SHIFT 3   /* a statistic factor below ATTACK_FACTOR_MIN divides the attack by 8 */

Champion g_champions[CHAMPION_MAX_COUNT];
int16_t g_party_champion_count;
int16_t g_leader_index = -1;
int16_t g_magic_caster_champion_index = -1;

/* Limits, tick counts and amounts of the champion bars, experience and reincarnation. */
#define MAXIMUM_HEALTH                    999
#define MAXIMUM_STAMINA                   9999
#define LEVEL_EXPERIENCE                  500 /* experience for level 2; each level doubles it */
#define TEMPORARY_EXPERIENCE_LIMIT        32000
#define RECENT_ATTACK_TICKS               25  /* hidden skills gain double experience */
#define OLD_ATTACK_TICKS                  150 /* fighting skills gain half experience */
#define TURN_BACK_TICKS                   60  /* a champion faces the party direction again */
#define HIDE_DAMAGE_TICKS                 5
#define POISON_STEP_TICKS                 36
#define REINCARNATION_POINTS              12
#define REINCARNATION_STAT_LOSS_SHIFT_CSB 3 /* CSB: each statistic but luck loses 1/8 */
#define REINCARNATION_BAR_SHIFT_CSB       2 /* CSB: health, stamina, and mana drop to 1/4 */
#define RESURRECTION_MINIMUM_HEALTH       25
#define SCENT_STRENGTH_MINIMUM            0
#define PIT_ATTACK                        20
#define RESURRECTION_HEALTH_LOSS_SHIFT    6 /* the Vi altar takes 1/64 of the maximum health */
#define FOUNTAIN_WATER                    2048
/* Level gain: stamina gain = maximum stamina / divisor, or >> shift. */
#define FIGHTER_STAMINA_GAIN_SHIFT        4
#define NINJA_STAMINA_GAIN_DIVISOR        21
#define PRIEST_STAMINA_GAIN_DIVISOR       25
#define WIZARD_STAMINA_GAIN_SHIFT         5
#define FIGHTER_HEALTH_GAIN_FACTOR        3
#define TEMPORARY_EXPERIENCE_GAIN_SHIFT   3 /* temporary experience gains experience / 8 */
#define TEMPORARY_EXPERIENCE_GAIN_MAXIMUM 100
/* Maximum load: strength * 8 + 100 tenths of a kilogram. */
#define LOAD_PER_STRENGTH                 8
#define LOAD_BASE                         100
#define LOAD_UNIT                         10 /* tenths of a kilogram in a kilogram */
#define WOUNDED_LOAD_SHIFT                3  /* a wound takes 1/8 of the maximum load */
#define WOUNDED_LEGS_LOAD_SHIFT           2  /* a leg wound takes 1/4 */
#define ELVEN_BOOTS_LOAD_SHIFT            4  /* Elven Boots add 1/16 */
/* Movement ticks: heavy above 5/8 of the maximum load; overloaded (load - maximum) * 4 / maximum + 4. */
#define MOVE_TICKS_HEAVY                  3
#define MOVE_TICKS_NORMAL                 2
#define OVERLOAD_TICKS_FACTOR             4
#define OVERLOAD_TICKS_BASE               4
#define ROLL_MAXIMUM                      100 /* dexterity, strength, and wound defense are at most 100 */
#define DEXTERITY_MINIMUM                 2
#define LIGHT_LOAD_SHIFT                  4  /* an object up to 1/16 of the maximum load is light */
#define STRENGTH_WEIGHT_OFFSET            12 /* strength changes by weight - 12 for a light object */

/* Bit masks of the random terms of dexterity and strength. */
#define DEXTERITY_RANDOM_MASK         0x7
#define STRENGTH_RANDOM_MASK          0xf
/* Wound defense: a shield adds (strength + defense) * factor >> 4 in its own slot, >> 5 in the other slots. */
#define SHIELD_DEFENSE_OWN_SLOT_SHIFT 4
#define SHIELD_DEFENSE_SHIFT          5
#define VITALITY_DEFENSE_SHIFT        3 /* the random defense is up to vitality / 8 */
#define WOUND_DEFENSE_PENALTY         8 /* a wounded slot loses random_2bits() + 8 */
/* Damage: attack * (limit - defense) / 64. */
#define DAMAGE_SCALE_SHIFT            6
#define ARMOUR_DEFENSE_LIMIT          130
#define PSYCHIC_WISDOM_LIMIT          115
#define WOUND_THRESHOLD_BASE          10
#define ROPE_STAMINA_FACTOR           25 /* rope climb stamina cost: load * 25 / maximum load + 1 */

/* Bit masks of the random terms of a wound. */
#define WOUND_RANDOM_MASK                0x7f /* wound threshold: (random16() & 127) + 10 */
#define WOUND_SLOT_RANDOM_MASK           0x07 /* a wound hits a random bit of the allowed wounds */
/* Time effects. */
#define MANA_GAIN_DIVISOR                40
#define MANA_STAMINA_COST_MINIMUM        7 /* stamina for one mana: max(7, 16 - wizard and priest levels) */
#define MANA_STAMINA_COST_BASE           16
#define FOOD_WATER_LAST_STEPS            4
#define STAMINA_RATE_SHIFT               8 /* stamina loss rate: maximum stamina / 256 - 1, from 1 to 6 */
#define STAMINA_RATE_MAXIMUM             6
#define IDLE_TICKS_HUNGRY                80 /* a party that does not move gets hungry faster */
#define IDLE_TICKS_VERY_HUNGRY           250
#define HEALTH_GAIN_VITALITY_BONUS       12
#define HEALTH_GAIN_SHIFT                7 /* health gain: maximum health / 128 + 1 */
#define POISON_DAMAGE_SHIFT              6 /* a poison step deals attack / 64 */
#define DAMAGE_ALL_SPREAD_SHIFT          3 /* damage to all champions spreads by attack / 8 */
/* The regeneration chance (0-112) takes the game time bits 6, 7, and 8 as its bits for 64, 32, and 16. */
#define TIME_CHANCE_HIGH_BIT             0x040
#define TIME_CHANCE_MIDDLE_BIT           0x080
#define TIME_CHANCE_LOW_BIT              0x100
#define STATISTICS_RESTORE_MASK          0x0ff /* statistics move every 256 ticks */
#define STATISTICS_RESTORE_SLEEPING_MASK 0x03f

uint16_t g_dead_champions;

/* Offsets of the original record. */
#define CHAMPION_RECORD_NAME             0x000
#define CHAMPION_RECORD_TITLE            0x008
#define CHAMPION_RECORD_DIRECTION        0x01c
#define CHAMPION_RECORD_CELL             0x01d
#define CHAMPION_RECORD_ACTION           0x020
#define CHAMPION_RECORD_SYMBOL_STEP      0x021
#define CHAMPION_RECORD_SYMBOLS          0x022
#define CHAMPION_RECORD_DAMAGE_DIRECTION 0x028
#define CHAMPION_RECORD_MAXIMUM_DAMAGE   0x029
#define CHAMPION_RECORD_POISON_EVENTS    0x02a
#define CHAMPION_RECORD_WORDS            0x02c
#define CHAMPION_RECORD_STATISTICS       0x046
#define CHAMPION_RECORD_SKILLS           0x05c
#define CHAMPION_RECORD_SLOTS            0x0d4
#define CHAMPION_RECORD_LOAD             0x110
#define CHAMPION_RECORD_SHIELD           0x112
#define CHAMPION_RECORD_MAGIC_MAP        0x118

/* Counts and sizes of the champion record parts. */
#define CHAMPION_RECORD_WORD_COUNT 13
#define CHAMPION_RECORD_SKILL_SIZE 6
#define WORDS(c)                                                                                                       \
    &c->enable_action_event_index, &c->hide_damage_received_event_index, (int16_t *)&c->attributes,                    \
        (int16_t *)&c->wounds, &c->current_health, &c->maximum_health, &c->current_stamina, &c->maximum_stamina,       \
        &c->current_mana, &c->maximum_mana, &c->action_defense, &c->food, &c->water

uint16_t champion_reset_for_new_game(void)
{
    uint16_t hand = g_leader_hand_object;
    g_leader_empty_handed = true;
    g_leader_hand_object = THING_NONE;
    g_leader_index = -1;
    return hand;
}

void champion_pack(const Champion *c, uint8_t *bytes)
{
    memset(bytes, 0, CHAMPION_RECORD_SIZE);
    memcpy(bytes + CHAMPION_RECORD_NAME, c->name, sizeof c->name);
    memcpy(bytes + CHAMPION_RECORD_TITLE, c->title, sizeof c->title);
    bytes[CHAMPION_RECORD_DIRECTION] = c->direction;
    bytes[CHAMPION_RECORD_CELL] = c->cell;
    bytes[CHAMPION_RECORD_ACTION] = c->action_index;
    bytes[CHAMPION_RECORD_SYMBOL_STEP] = c->symbol_step;
    memcpy(bytes + CHAMPION_RECORD_SYMBOLS, c->symbols, sizeof c->symbols);
    bytes[CHAMPION_RECORD_DAMAGE_DIRECTION] = c->direction_maximum_damage_received;
    bytes[CHAMPION_RECORD_MAXIMUM_DAMAGE] = c->maximum_damage_received;
    bytes[CHAMPION_RECORD_POISON_EVENTS] = c->poison_event_count;
    const int16_t *words[CHAMPION_RECORD_WORD_COUNT] = {WORDS(c)};
    for (int i = 0; i < CHAMPION_RECORD_WORD_COUNT; i++)
        wr16(bytes + CHAMPION_RECORD_WORDS + 2 * i, *words[i]);
    memcpy(bytes + CHAMPION_RECORD_STATISTICS, c->statistics, sizeof c->statistics);
    for (int i = 0; i < SKILL_COUNT; i++) {
        wr16(bytes + CHAMPION_RECORD_SKILLS + CHAMPION_RECORD_SKILL_SIZE * i, c->skills[i].temporary_experience);
        wr32(bytes + CHAMPION_RECORD_SKILLS + CHAMPION_RECORD_SKILL_SIZE * i + 2, (uint32_t)c->skills[i].experience);
    }
    for (int i = 0; i < SLOT_COUNT; i++)
        wr16(bytes + CHAMPION_RECORD_SLOTS + 2 * i, c->slots[i]);
    wr16(bytes + CHAMPION_RECORD_LOAD, c->load);
    wr16(bytes + CHAMPION_RECORD_SHIELD, c->shield_defense);
    memcpy(bytes + CHAMPION_RECORD_MAGIC_MAP, c->magic_map, sizeof c->magic_map);
}

void champion_unpack(const uint8_t *in, Champion *c)
{
    ChampionRuntime runtime = c->runtime; /* the save file does not hold it */
    memset(c, 0, sizeof *c);
    c->runtime = runtime;
    memcpy(c->name, in + CHAMPION_RECORD_NAME, sizeof c->name);
    memcpy(c->title, in + CHAMPION_RECORD_TITLE, sizeof c->title);
    c->direction = in[CHAMPION_RECORD_DIRECTION];
    c->cell = in[CHAMPION_RECORD_CELL];
    c->action_index = in[CHAMPION_RECORD_ACTION];
    c->symbol_step = in[CHAMPION_RECORD_SYMBOL_STEP];
    memcpy(c->symbols, in + CHAMPION_RECORD_SYMBOLS, sizeof c->symbols);
    c->direction_maximum_damage_received = in[CHAMPION_RECORD_DAMAGE_DIRECTION];
    c->maximum_damage_received = in[CHAMPION_RECORD_MAXIMUM_DAMAGE];
    c->poison_event_count = in[CHAMPION_RECORD_POISON_EVENTS];
    int16_t *words[CHAMPION_RECORD_WORD_COUNT] = {WORDS(c)};
    for (int i = 0; i < CHAMPION_RECORD_WORD_COUNT; i++)
        *words[i] = be16(in + CHAMPION_RECORD_WORDS + 2 * i);
    memcpy(c->statistics, in + CHAMPION_RECORD_STATISTICS, sizeof c->statistics);
    for (int i = 0; i < SKILL_COUNT; i++) {
        c->skills[i].temporary_experience = be16(in + CHAMPION_RECORD_SKILLS + CHAMPION_RECORD_SKILL_SIZE * i);
        c->skills[i].experience = be32(in + CHAMPION_RECORD_SKILLS + CHAMPION_RECORD_SKILL_SIZE * i + 2);
    }
    for (int i = 0; i < SLOT_COUNT; i++)
        c->slots[i] = be16(in + CHAMPION_RECORD_SLOTS + 2 * i);
    c->load = be16(in + CHAMPION_RECORD_LOAD);
    c->shield_defense = be16(in + CHAMPION_RECORD_SHIELD);
    memcpy(c->magic_map, in + CHAMPION_RECORD_MAGIC_MAP, sizeof c->magic_map);
}

/* "<prefix><name><suffix>" in the champion color on a new message line. */
static void print_champion_message(int16_t champion_index, int16_t text)
{
    int16_t color = tables_get_champion_color(champion_index);
    message_new_line();
    message_print(color, gfx_get_string(text));
    message_print(color, g_champions[champion_index].name);
    message_print(color, gfx_get_string(text + 1));
}

int16_t champion_get_index_in_cell(int16_t cell)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].cell == cell && g_champions[i].current_health)
            return i;
    return -1;
}

void champion_set_leader(int16_t champion_index)
{
    if (champion_index == g_leader_index || (champion_index != -1 && !g_champions[champion_index].current_health))
        return;
    if (g_leader_index != -1) {
        Champion *old = &g_champions[g_leader_index];
        old->attributes |= CHAMPION_ATTR_LOAD | CHAMPION_ATTR_NAME_TITLE;
        old->load -= inventory_get_object_weight(g_leader_hand_object);
        g_leader_index = -1;
    }
    if (champion_index != -1) {
        Champion *c = &g_champions[champion_index];
        g_leader_index = champion_index;
        c->direction = g_party_direction;
        c->load += inventory_get_object_weight(g_leader_hand_object);
        if (champion_index != g_session.candidate_champion_index)
            c->attributes |= CHAMPION_ATTR_LOAD | CHAMPION_ATTR_NAME_TITLE | CHAMPION_ATTR_ICON;
    }
}

void champion_wake_up(void)
{
    g_session.party_sleeping = false;
}

void champion_go_to_sleep(void)
{
    g_session.party_sleeping = true;
}

void champion_drink_from_fountain(bool whole_party)
{
    g_champions[g_leader_index].water = FOUNTAIN_WATER;
    for (uint16_t i = 0; whole_party && i < g_party_champion_count; i++)
        if (g_champions[i].current_health)
            g_champions[i].water = FOUNTAIN_WATER;
    sound_request_play(SOUND_SWALLOW, (Location){g_party_map_x, g_party_map_y}, SOUND_MODE_NOW, 0);
}

uint16_t champion_turn_to_last_attackers(void)
{
    uint16_t turned = 0;
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *champion = &g_champions[i];
        if (i != g_leader_index && i != g_session.acting_champion_index && champion->maximum_damage_received &&
            champion->direction != champion->direction_maximum_damage_received) {
            champion->direction = champion->direction_maximum_damage_received;
            champion->attributes |= CHAMPION_ATTR_ICON;
            turned |= 1 << i;
        }
        champion->maximum_damage_received = 0;
    }
    return turned;
}

int16_t champion_move_to_cell(int16_t champion_index, int16_t cell)
{
    Champion *moved = &g_champions[champion_index];
    moved->direction = g_party_direction;
    moved->attributes |= CHAMPION_ATTR_ICON;
    if (cell == moved->cell)
        return -1;
    int16_t other = champion_get_index_in_cell(cell);
    if (other >= 0) {
        g_champions[other].cell = moved->cell;
        g_champions[other].attributes |= CHAMPION_ATTR_ICON;
    }
    moved->cell = cell;
    return other;
}

/* The slot that a candidate possession goes to: hands, body, pouches, neck, then the backpack. */
static uint16_t get_candidate_slot(const Champion *c, uint16_t thing, uint16_t *next_backpack_slot)
{
    uint16_t allowed = tables_get_object_info(inventory_get_object_info_index(thing))->allowed_slots;
    bool neck_free = allowed & tables_get_slot_mask(SLOT_NECK) && c->slots[SLOT_NECK] == THING_NONE;
    uint16_t slot = THING_NONE;
    switch (THING_TYPE(thing)) {
    case THING_ARMOUR:
        for (slot = SLOT_HEAD; slot <= SLOT_FEET && !(allowed & tables_get_slot_mask(slot)); slot++)
            ;
        if (slot > SLOT_FEET)
            slot = neck_free ? SLOT_NECK : THING_NONE;
        break;
    case THING_WEAPON:
        if (c->slots[SLOT_HAND_ACTION] == THING_NONE)
            slot = SLOT_HAND_ACTION;
        break;
    case THING_SCROLL:
    case THING_POTION:
        if (c->slots[SLOT_POUCH_1] == THING_NONE)
            slot = SLOT_POUCH_1;
        else if (c->slots[SLOT_POUCH_2] == THING_NONE)
            slot = SLOT_POUCH_2;
        break;
    }
    while (slot == THING_NONE || c->slots[slot] != THING_NONE) {
        neck_free = allowed & tables_get_slot_mask(SLOT_NECK) && c->slots[SLOT_NECK] == THING_NONE;
        slot = neck_free ? SLOT_NECK : (*next_backpack_slot)++;
    }
    return slot;
}

/* The objects in the facing cell of the mirror wall become the candidate's possessions. They
   stay in the wall square list until the player accepts the candidate. */
static void add_candidate_possessions(int16_t champion_index)
{
    int16_t x, y;
    x = g_party_map_x;
    y = g_party_map_y;
    map_step(g_party_direction, 1, 0, &x, &y);
    uint16_t cell = (g_party_direction + 2) % CELL_COUNT;
    uint16_t backpack = SLOT_BACKPACK_FIRST;
    for (uint16_t t = map_get_square_first_thing(x, y); t != THING_ENDOFLIST; t = rec_thing_next(t)) {
        uint16_t type = THING_TYPE(t);
        /* Fix: the original reuses a stale slot for things that are not objects; none lie on mirror walls. */
        if (type < THING_WEAPON || type > THING_JUNK || THING_CELL(t) != cell)
            continue;
        inventory_add_object_in_slot(champion_index, t, get_candidate_slot(&g_champions[champion_index], t, &backpack));
    }
}

void champion_set_portrait(int16_t champion_index, int16_t portrait_index)
{
    const Bitmap *portraits = gfx_get_bitmap(GFX_CHAMPION_PORTRAITS);
    uint8_t *px = g_champions[champion_index].runtime.portrait;
    int16_t x, y;
    memset(px, 0, CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT);
    recruit_portrait_origin(portrait_index, &x, &y);
    if (!portraits)
        return;
    for (int row = 0; row < CHAMPION_PORTRAIT_HEIGHT; row++)
        for (int column = 0; column < CHAMPION_PORTRAIT_WIDTH; column++) {
            int sx = x + column;
            int sy = y + row;
            if (sx >= 0 && sx < portraits->w && sy >= 0 && sy < portraits->h)
                px[row * CHAMPION_PORTRAIT_WIDTH + column] = portraits->px[sy * portraits->w + sx];
        }
}

void champion_add_candidate_to_party(int16_t portrait_index)
{
    if (!g_leader_empty_handed || g_party_champion_count >= CHAMPION_MAX_COUNT)
        return;
    uint16_t index = g_party_champion_count;
    Champion *c = &g_champions[index];
    memset(c, 0, sizeof *c);
    champion_set_portrait(index, portrait_index);
    c->action_index = ACTION_NONE;
    c->enable_action_event_index = -1;
    c->hide_damage_received_event_index = -1;
    c->direction = g_party_direction;
    int16_t n = 0;
    while (champion_get_index_in_cell((g_party_direction + n) % CELL_COUNT) != -1)
        n++;
    c->cell = (g_party_direction + n) % CELL_COUNT;
    c->attributes = CHAMPION_ATTR_ICON;
    c->direction_maximum_damage_received = g_party_direction;
    c->food = (random16() & FOOD_WATER_RANDOM_MASK) + INITIAL_FOOD_WATER;
    c->water = (random16() & FOOD_WATER_RANDOM_MASK) + INITIAL_FOOD_WATER;
    for (int s = 0; s < SLOT_COUNT; s++)
        c->slots[s] = THING_NONE;
    RecruitData r;
    if (!recruit_parse(recruit_text_thing(g_party_map_index, g_party_map_x, g_party_map_y), &r))
        return;
    memcpy(c->name, r.name, sizeof c->name);
    memcpy(c->title, r.title, sizeof c->title);
    if (r.male)
        c->attributes |= CHAMPION_ATTR_MALE;
    c->current_health = r.current_health;
    c->maximum_health = r.maximum_health;
    c->current_stamina = r.current_stamina;
    c->maximum_stamina = r.maximum_stamina;
    c->current_mana = r.current_mana;
    c->maximum_mana = r.maximum_mana;
    memcpy(c->statistics, r.statistics, sizeof c->statistics);
    for (int s = 0; s < SKILL_COUNT; s++)
        c->skills[s].experience = r.experience[s];
    g_session.candidate_champion_index = index;
    c->attributes |= CHAMPION_ATTR_ACTION_HAND;
    if (++g_party_champion_count == 1) {
        champion_set_leader(0);
        g_magic_caster_champion_index = 0;
    }
    add_candidate_possessions(index);
    g_session.inventory_champion_index = index;
}

/* Reincarnation: all experience is lost and 12 random statistic points are gained. In CSB, the statistics but luck
   lose 1/8 first, down to their minimum, and health, stamina, and mana drop to 1/4. */
static void reincarnate(Champion *c)
{
    memset(c->skills, 0, sizeof c->skills);
    if (g_adventure == ADVENTURE_CSB) {
        for (int s = STAT_STRENGTH; s < STAT_COUNT; s++) {
            int16_t value = c->statistics[s][STAT_CURRENT];
            value = max16(c->statistics[s][STAT_MINIMUM], value - (value >> REINCARNATION_STAT_LOSS_SHIFT_CSB));
            c->statistics[s][STAT_MAXIMUM] = value;
            c->statistics[s][STAT_CURRENT] = c->statistics[s][STAT_MAXIMUM];
        }
        c->current_health >>= REINCARNATION_BAR_SHIFT_CSB;
        c->maximum_health >>= REINCARNATION_BAR_SHIFT_CSB;
        c->current_stamina >>= REINCARNATION_BAR_SHIFT_CSB;
        c->maximum_stamina >>= REINCARNATION_BAR_SHIFT_CSB;
        c->current_mana >>= REINCARNATION_BAR_SHIFT_CSB;
        c->maximum_mana >>= REINCARNATION_BAR_SHIFT_CSB;
    }
    for (int i = 0; i < REINCARNATION_POINTS; i++) {
        uint16_t s = random16() % STAT_COUNT;
        c->statistics[s][STAT_CURRENT]++;
        c->statistics[s][STAT_MAXIMUM]++;
    }
}

void champion_click_rebirth_panel(int16_t command)
{
    uint16_t index = g_party_champion_count - 1;
    Champion *c = &g_champions[index];
    g_session.inventory_champion_index = -1;
    g_session.candidate_champion_index = -1;
    if (command == PANEL_CANCEL) {
        if (g_party_champion_count == 1) {
            champion_set_leader(-1);
            g_magic_caster_champion_index = -1;
        }
        g_party_champion_count--;
        return;
    }
    int16_t x, y;
    x = g_party_map_x;
    y = g_party_map_y;
    map_step(g_party_direction, 1, 0, &x, &y);
    for (uint16_t s = 0; s < SLOT_COUNT; s++)
        if (c->slots[s] != THING_NONE)
            map_unlink_thing_from_list(c->slots[s], 0, x, y);
    uint16_t mirror = map_find_thing((Location){x, y}, THING_SENSOR);
    rec_sensor_set_type(mirror, SENSOR_DISABLED);
    if (command == PANEL_REINCARNATE)
        reincarnate(c);
    if (g_party_champion_count == 1)
        g_last_party_movement_time = g_game_time;
    print_champion_message(index, command == PANEL_RESURRECT ? TEXT_RESURRECTED : TEXT_REINCARNATED);
}

void champion_resurrect(int16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    if (champion_get_index_in_cell(c->cell) != -1) {
        int16_t cell = 0;
        while (champion_get_index_in_cell(cell) != -1)
            cell++;
        c->cell = cell;
    }
    c->load = 0;
    for (int s = 0; s < SLOT_COUNT; s++)
        c->slots[s] = THING_NONE;
    /* unsigned shift as in the original */
    int16_t health = max16(RESURRECTION_MINIMUM_HEALTH,
                           c->maximum_health - ((uint16_t)c->maximum_health >> RESURRECTION_HEALTH_LOSS_SHIFT) - 1);
    c->maximum_health = health;
    c->current_health = health >> 1;
    c->direction = g_party_direction;
    c->attributes |= CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ICON;
}

static uint16_t get_base_skill(uint16_t skill)
{
    return skill < SKILL_BASE_COUNT ? skill : (skill - SKILL_BASE_COUNT) / SKILL_BASE_COUNT;
}

/* Extra levels from the Firestaff in the action hand and from neck jewels. */
static int16_t get_object_skill_bonus(const Champion *c, uint16_t skill)
{
    int16_t action = object_get_icon_index(c->slots[SLOT_HAND_ACTION]);
    int16_t neck = object_get_icon_index(c->slots[SLOT_NECK]);
    int16_t bonus = action == ICON_FIRESTAFF ? 1 : action == ICON_FIRESTAFF_COMPLETE ? 2 : 0;
    switch (skill) {
    case SKILL_HEAL:
        return bonus + (neck == ICON_GEM_OF_AGES || action == ICON_SCEPTRE_OF_LYF);
    case SKILL_WIZARD:
        return bonus + (neck == ICON_PENDANT_FERAL);
    case SKILL_DEFEND:
        return bonus + (neck == ICON_EKKHARD_CROSS);
    case SKILL_INFLUENCE:
        return bonus + (neck == ICON_MOONSTONE);
    }
    return bonus;
}

int16_t champion_get_skill_level(int16_t champion_index, int16_t skill_index, bool ignore_modifiers,
                                 bool ignore_temporary)
{
    if (g_session.party_sleeping)
        return 1;
    const Champion *c = &g_champions[champion_index];
    bool temporary = !ignore_temporary;
    uint32_t experience = c->skills[skill_index].experience;
    /* The original adds the temporary experience as an unsigned word. */
    if (temporary)
        experience += (uint16_t)c->skills[skill_index].temporary_experience;
    if (skill_index >= SKILL_BASE_COUNT) {
        const Skill *base = &c->skills[get_base_skill(skill_index)];
        experience += base->experience;
        if (temporary)
            experience += (uint16_t)base->temporary_experience;
        experience >>= 1;
    }
    int16_t level = 1;
    for (; experience >= LEVEL_EXPERIENCE; experience >>= 1)
        level++;
    if (!ignore_modifiers)
        level += get_object_skill_bonus(c, skill_index);
    return level;
}

/* A new base skill level raises statistics, maximum health, stamina, and mana. */
static void gain_level(int16_t champion_index, uint16_t base, uint16_t level)
{
    Champion *c = &g_champions[champion_index];
    uint8_t (*maximum)[STAT_VALUE_COUNT] = c->statistics;
    uint16_t r1 = random_bit();
    uint16_t r2 = random_bit() + 1;
    uint16_t vitality = random_bit();
    if (base != SKILL_PRIEST)
        vitality &= level;
    maximum[STAT_VITALITY][STAT_MAXIMUM] += vitality;
    uint16_t stamina_gain = c->maximum_stamina;
    uint16_t health_gain = level;
    maximum[STAT_ANTIFIRE][STAT_MAXIMUM] += ~level & random_bit();
    switch (base) {
    case SKILL_FIGHTER:
        stamina_gain >>= FIGHTER_STAMINA_GAIN_SHIFT;
        health_gain = level * FIGHTER_HEALTH_GAIN_FACTOR;
        maximum[STAT_STRENGTH][STAT_MAXIMUM] += r2;
        maximum[STAT_DEXTERITY][STAT_MAXIMUM] += r1;
        break;
    case SKILL_NINJA:
        stamina_gain /= NINJA_STAMINA_GAIN_DIVISOR;
        health_gain = level * 2;
        maximum[STAT_STRENGTH][STAT_MAXIMUM] += r1;
        maximum[STAT_DEXTERITY][STAT_MAXIMUM] += r2;
        break;
    case SKILL_PRIEST:
    case SKILL_WIZARD:
        if (base == SKILL_PRIEST) {
            stamina_gain /= PRIEST_STAMINA_GAIN_DIVISOR;
            c->maximum_mana += level;
            health_gain = level + ((level + 1) >> 1);
            maximum[STAT_WISDOM][STAT_MAXIMUM] += r1;
        } else {
            stamina_gain >>= WIZARD_STAMINA_GAIN_SHIFT;
            c->maximum_mana += level + (level >> 1);
            maximum[STAT_WISDOM][STAT_MAXIMUM] += r2;
        }
        c->maximum_mana += min16(random_2bits(), level - 1);
        if (c->maximum_mana > MAXIMUM_MANA)
            c->maximum_mana = MAXIMUM_MANA;
        maximum[STAT_ANTIMAGIC][STAT_MAXIMUM] += random_2bits();
        break;
    }
    c->maximum_health += health_gain + random16() % ((health_gain >> 1) + 1);
    if (c->maximum_health > MAXIMUM_HEALTH)
        c->maximum_health = MAXIMUM_HEALTH;
    c->maximum_stamina += stamina_gain + random16() % ((stamina_gain >> 1) + 1);
    if (c->maximum_stamina > MAXIMUM_STAMINA)
        c->maximum_stamina = MAXIMUM_STAMINA;
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    g_champions[champion_index].runtime.skill_level_up[base] = true;
    print_champion_message(champion_index, TEXT_LEVEL_GAINED + 2 * base);
}

void champion_add_skill_experience(int16_t champion_index, uint16_t skill, uint16_t experience)
{
    if (skill >= SKILL_SWING && skill <= SKILL_SHOOT &&
        g_game_time - OLD_ATTACK_TICKS > (uint32_t)g_last_creature_attack_time)
        experience >>= 1;
    if (!experience)
        return;
    uint16_t difficulty = rec_current_map_difficulty();
    if (difficulty)
        experience *= difficulty;
    Champion *c = &g_champions[champion_index];
    uint16_t base = get_base_skill(skill);
    uint16_t old_level = champion_get_skill_level(champion_index, base, true, true);
    if (skill >= SKILL_BASE_COUNT && g_game_time - RECENT_ATTACK_TICKS < (uint32_t)g_last_creature_attack_time)
        experience *= 2;
    Skill *s = &c->skills[skill];
    s->experience = (uint32_t)s->experience + experience;               /* 32-bit wrap */
    if ((uint16_t)s->temporary_experience < TEMPORARY_EXPERIENCE_LIMIT) /* unsigned compare as in the original */
        s->temporary_experience +=
            bounded_value(1, experience >> TEMPORARY_EXPERIENCE_GAIN_SHIFT, TEMPORARY_EXPERIENCE_GAIN_MAXIMUM);
    if (skill >= SKILL_BASE_COUNT)
        c->skills[base].experience = (uint32_t)c->skills[base].experience + experience;
    uint16_t level = champion_get_skill_level(champion_index, base, true, true);
    if (level > old_level)
        gain_level(champion_index, base, level);
}

uint16_t champion_get_maximum_load(int16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t load = champion_adjust_by_stamina(
        champion_index, c->statistics[STAT_STRENGTH][STAT_CURRENT] * LOAD_PER_STRENGTH + LOAD_BASE);
    if (c->wounds)
        load -= load >> (c->wounds & WOUND_LEGS ? WOUNDED_LEGS_LOAD_SHIFT : WOUNDED_LOAD_SHIFT);
    if (object_get_icon_index(c->slots[SLOT_FEET]) == ICON_ELVEN_BOOTS)
        load += load >> ELVEN_BOOTS_LOAD_SHIFT;
    load += LOAD_UNIT - 1;
    return load - load % LOAD_UNIT;
}

LoadLevel champion_get_load_level(int16_t champion_index)
{
    uint16_t max = champion_get_maximum_load(champion_index);
    uint16_t load = g_champions[champion_index].load;
    if (max < load)
        return LOAD_OVERLOADED;
    if ((int32_t)max * HEAVY_LOAD_NUMERATOR < (int32_t)load * HEAVY_LOAD_DENOMINATOR)
        return LOAD_HEAVY;
    return LOAD_NORMAL;
}

int16_t champion_get_movement_ticks(int16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t maximum = champion_get_maximum_load(champion_index);
    uint16_t load = c->load;
    int16_t ticks, wounded_feet;
    if (load < maximum) {
        ticks = (int32_t)maximum * HEAVY_LOAD_NUMERATOR < (int32_t)load * HEAVY_LOAD_DENOMINATOR ? MOVE_TICKS_HEAVY
                                                                                                 : MOVE_TICKS_NORMAL;
        wounded_feet = 1;
    } else {
        /* 16-bit wrap as in the original */
        ticks = (uint16_t)((load - maximum) * OVERLOAD_TICKS_FACTOR) / maximum + OVERLOAD_TICKS_BASE;
        wounded_feet = 2;
    }
    if (c->wounds & WOUND_FEET)
        ticks += wounded_feet;
    if (object_get_icon_index(c->slots[SLOT_FEET]) == ICON_BOOTS_OF_SPEED)
        ticks--;
    return ticks;
}

int16_t champion_get_dexterity(int16_t champion_index)
{
    const Champion *c = &g_champions[champion_index];
    int16_t dexterity = c->statistics[STAT_DEXTERITY][STAT_CURRENT] + (random16() & DEXTERITY_RANDOM_MASK);
    int32_t product = (int32_t)((uint32_t)c->load * (uint32_t)(dexterity >> 1)); /* 32-bit signed product */
    dexterity -= product / champion_get_maximum_load(champion_index);
    dexterity = max16(DEXTERITY_MINIMUM, dexterity);
    if (g_session.party_sleeping)
        dexterity >>= 1;
    int16_t high = ROLL_MAXIMUM - (random16() & DEXTERITY_RANDOM_MASK);
    int16_t low = (random16() & DEXTERITY_RANDOM_MASK) + 1;
    return bounded_value(low, dexterity >> 1, high);
}

int16_t champion_get_strength(int16_t champion_index, int16_t slot)
{
    const Champion *c = &g_champions[champion_index];
    int16_t strength = c->statistics[STAT_STRENGTH][STAT_CURRENT] + (random16() & STRENGTH_RANDOM_MASK);
    uint16_t thing = c->slots[slot];
    uint16_t weight = inventory_get_object_weight(thing);
    uint16_t light = champion_get_maximum_load(champion_index) >> LIGHT_LOAD_SHIFT;
    if (light >= weight) {
        strength += weight - STRENGTH_WEIGHT_OFFSET;
    } else {
        /* unsigned shift as in the original: light below 12 gives a large value */
        uint16_t heavy = light + ((uint16_t)(light - STRENGTH_WEIGHT_OFFSET) >> 1);
        if (heavy >= weight)
            strength += (weight - light) >> 1;
        else
            strength -= (weight - heavy) * 2;
    }
    if (THING_TYPE(thing) == THING_WEAPON) {
        const WeaponInfo *w = tables_get_weapon_info(rec_weapon_type(thing));
        int16_t skill = 0;
        strength += w->strength;
        if (w->weapon_class == WEAPON_CLASS_SWING || w->weapon_class == WEAPON_CLASS_DAGGER_AND_AXES)
            skill = champion_get_skill_level(champion_index, SKILL_SWING, false, false);
        if (w->weapon_class != WEAPON_CLASS_SWING && w->weapon_class < WEAPON_CLASS_FIRST_AMMUNITION)
            skill += champion_get_skill_level(champion_index, SKILL_THROW, false, false);
        if (w->weapon_class >= WEAPON_CLASS_FIRST_AMMUNITION && w->weapon_class < WEAPON_CLASS_AMMUNITION_END)
            skill += champion_get_skill_level(champion_index, SKILL_SHOOT, false, false);
        strength += skill * 2;
    }
    strength = champion_adjust_by_stamina(champion_index, strength);
    if (c->wounds & (slot == SLOT_HAND_READY ? WOUND_READY_HAND : WOUND_ACTION_HAND))
        strength >>= 1;
    return bounded_value(0, strength >> 1, ROLL_MAXIMUM);
}

static const ArmourInfo *get_armour_info(uint16_t thing)
{
    return tables_get_armour_info(rec_armour_type(thing));
}

int16_t champion_get_wound_defense(int16_t champion_index, int16_t slot, bool sharp)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t shields = 0;
    for (int16_t hand = SLOT_HAND_READY; hand <= SLOT_HAND_ACTION; hand++) {
        uint16_t thing = c->slots[hand];
        if (THING_TYPE(thing) != THING_ARMOUR || !(get_armour_info(thing)->attributes & ARMOUR_SHIELD))
            continue;
        uint16_t sum =
            champion_get_strength(champion_index, hand) + inventory_get_armour_defense(get_armour_info(thing), sharp);
        /* 16-bit truncation of the product as in the original */
        shields += (uint16_t)(sum * tables_get_wound_defense_factor(slot)) >>
                   (hand == slot ? SHIELD_DEFENSE_OWN_SLOT_SHIFT : SHIELD_DEFENSE_SHIFT);
    }
    uint16_t luck = random16() % ((c->statistics[STAT_VITALITY][STAT_CURRENT] >> VITALITY_DEFENSE_SHIFT) + 1);
    if (sharp)
        luck >>= 1;
    int16_t defense = luck + c->action_defense + c->shield_defense + g_party.shield_defense + shields;
    if (slot > SLOT_HAND_ACTION && THING_TYPE(c->slots[slot]) == THING_ARMOUR)
        defense += inventory_get_armour_defense(get_armour_info(c->slots[slot]), sharp);
    if (c->wounds & 1 << slot)
        defense -= random_2bits() + WOUND_DEFENSE_PENALTY;
    if (g_session.party_sleeping)
        defense >>= 1;
    return bounded_value(0, defense >> 1, ROLL_MAXIMUM);
}

/* Attack after the champion's resistances; armour (wound defense) reduces normal attacks. */
static int16_t resisted_attack(int16_t champion_index, int16_t attack, uint16_t allowed_wounds, int16_t attack_type)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t count = 0;
    uint16_t defense = 0;
    for (int16_t slot = SLOT_HAND_READY; slot <= SLOT_FEET; slot++)
        if (allowed_wounds & 1 << slot) {
            count++;
            defense += champion_get_wound_defense(champion_index, slot, attack_type == ATTACK_SHARP);
        }
    if (count)
        defense /= count;
    switch (attack_type) {
    case ATTACK_FIRE:
        attack = champion_get_adjusted_attack(champion_index, STAT_ANTIFIRE, attack) - g_party.fire_shield_defense;
        break;
    case ATTACK_SELF:
        defense = (defense >> 1) + champion_get_skill_level(champion_index, SKILL_NINJA, false, false);
        break;
    case ATTACK_MAGIC:
        return champion_get_adjusted_attack(champion_index, STAT_ANTIMAGIC, attack) - g_party.spell_shield_defense;
    case ATTACK_PSYCHIC: {
        int16_t wisdom = PSYCHIC_WISDOM_LIMIT - c->statistics[STAT_WISDOM][STAT_CURRENT];
        return wisdom <= 0 ? 0 : scale_value(attack, DAMAGE_SCALE_SHIFT, wisdom);
    }
    }
    return attack <= 0 ? 0 : scale_value(attack, DAMAGE_SCALE_SHIFT, ARMOUR_DEFENSE_LIMIT - defense);
}

int16_t champion_add_pending_damage(int16_t champion_index, int16_t attack, uint16_t allowed_wounds,
                                    int16_t attack_type)
{
    if (champion_index == -1 || champion_index == g_session.candidate_champion_index || attack <= 0)
        return 0;
    const Champion *c = &g_champions[champion_index];
    if (!c->current_health)
        return 0;
    if (attack_type != ATTACK_RAW) {
        attack = resisted_attack(champion_index, attack, allowed_wounds, attack_type);
        if (attack <= 0)
            return 0;
        int16_t threshold = champion_get_adjusted_attack(champion_index, STAT_VITALITY,
                                                         (random16() & WOUND_RANDOM_MASK) + WOUND_THRESHOLD_BASE);
        if (threshold < attack)
            do {
                g_champions[champion_index].runtime.pending_wounds |=
                    allowed_wounds & 1 << (random16() & WOUND_SLOT_RANDOM_MASK);
                threshold *= 2;
            } while (threshold < attack && threshold);
        if (g_session.party_sleeping)
            champion_wake_up();
    }
    g_champions[champion_index].runtime.pending_damage += attack;
    return attack;
}

void champion_apply_pending_damage(void)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *c = &g_champions[i];
        uint16_t wounds = c->runtime.pending_wounds;
        c->wounds |= wounds;
        c->runtime.pending_wounds = 0;
        int16_t damage = c->runtime.pending_damage;
        if (!damage)
            continue;
        c->runtime.pending_damage = 0;
        if (!c->current_health)
            continue;
        int16_t health = c->current_health - damage;
        if (health <= 0 && g_session.cheat_immortal)
            health = 1;
        if (health <= 0) {
            champion_kill(i);
            continue;
        }
        c->current_health = health;
        c->attributes |= wounds ? CHAMPION_ATTR_STATISTICS | CHAMPION_ATTR_WOUNDS : CHAMPION_ATTR_STATISTICS;
        uint32_t map_time = EVENT_MAP_TIME(g_party_map_index, g_game_time + HIDE_DAMAGE_TICKS);
        int16_t e = c->hide_damage_received_event_index;
        if (e == -1) {
            TimelineEvent event = {map_time, EVENT_HIDE_DAMAGE_RECEIVED, i, 0, 0};
            c->hide_damage_received_event_index = timeline_add_event(&event);
        } else {
            g_timeline.events[e].map_time = map_time;
            timeline_fix_chronology(timeline_get_index(e));
        }
    }
}

/* The object appears on the party square, in the cell. */
static void drop_on_party_square(uint16_t thing, int16_t cell)
{
    move_get_move_result(
        &(Move){.thing = THING_WITH_CELL(thing, cell), .to.x = g_party_map_x, .to.y = g_party_map_y, .enters = true},
        NULL);
}

/* All objects fall to the floor in the champion's cell. */
static void drop_all_objects(int16_t champion_index)
{
    uint16_t cell = g_champions[champion_index].cell;
    for (uint16_t i = 0; i < SLOT_COUNT; i++) {
        uint16_t thing = inventory_take_slot_object(champion_index, tables_get_slot_drop_order(i));
        if (thing != THING_NONE)
            drop_on_party_square(thing, cell);
    }
}

void champion_kill(int16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    if (champion_index == g_session.acting_champion_index)
        action_clear_acting_champion();
    c->current_health = 0;
    c->attributes |= CHAMPION_ATTR_STATUS_BOX;
    g_dead_champions |= 1 << champion_index;
    drop_all_objects(champion_index);
    uint16_t bones = rec_find_unused_thing(THING_JUNK | NO_JUNK_RESERVE);
    if (bones != THING_NONE) {
        rec_junk_set_type(bones, JUNK_BONES);
        rec_object_set_important(bones, true);
        rec_junk_set_charges(bones, champion_index);
        drop_on_party_square(bones, c->cell);
    }
    c->symbol_step = 0;
    c->symbols[0] = 0;
    c->direction = g_party_direction;
    c->maximum_damage_received = 0;
    if (c->poison_event_count)
        champion_unpoison(champion_index);
    int16_t alive = 0;
    while (alive < g_party_champion_count && !g_champions[alive].current_health)
        alive++;
    if (alive == g_party_champion_count) {
        g_session.party_dead = true;
        return;
    }
    if (champion_index == g_leader_index)
        champion_set_leader(alive);
    if (champion_index == g_magic_caster_champion_index) {
        g_magic_caster_champion_index = alive;
        g_redraw |= REDRAW_SPELL_AREA;
    } else {
        g_redraw |= REDRAW_SPELL_TABS;
    }
}

void champion_poison(int16_t champion_index, int16_t attack)
{
    if (champion_index == -1 || champion_index == g_session.candidate_champion_index)
        return;
    Champion *c = &g_champions[champion_index];
    champion_add_pending_damage(champion_index, max16(1, (uint16_t)attack >> POISON_DAMAGE_SHIFT), 0, ATTACK_RAW);
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    if (--attack) {
        c->poison_event_count++;
        TimelineEvent e = {EVENT_MAP_TIME(g_party_map_index, g_game_time + POISON_STEP_TICKS), EVENT_POISON,
                           champion_index, attack, 0};
        timeline_add_event(&e);
    }
}

void champion_unpoison(int16_t champion_index)
{
    if (champion_index == -1)
        return;
    for (uint16_t i = 0; i < g_timeline.high_water; i++)
        if (g_timeline.events[i].type == EVENT_POISON && g_timeline.events[i].priority == champion_index)
            timeline_delete_event(i);
    g_champions[champion_index].poison_event_count = 0;
}

int16_t champion_damage_all(int16_t attack, uint16_t allowed_wounds, int16_t attack_type)
{
    if (!attack)
        return 0;
    uint16_t spread = ((uint16_t)attack >> DAMAGE_ALL_SPREAD_SHIFT) + 1;
    attack -= spread;
    spread *= 2;
    int16_t count = 0;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (champion_add_pending_damage(i, max16(1, attack + random16() % spread), allowed_wounds, attack_type))
            count++;
    return count;
}

void champion_decrement_stamina(int16_t champion_index, int16_t decrement)
{
    if (champion_index == -1)
        return;
    Champion *c = &g_champions[champion_index];
    c->current_stamina -= decrement;
    int16_t stamina = c->current_stamina;
    if (stamina <= 0) {
        c->current_stamina = 0;
        champion_add_pending_damage(champion_index, (int16_t)-stamina >> 1, 0,
                                    ATTACK_RAW); /* 16-bit wrap as in the original */
    } else if (stamina > c->maximum_stamina) {
        c->current_stamina = c->maximum_stamina;
    }
    c->attributes |= CHAMPION_ATTR_STATISTICS | CHAMPION_ATTR_LOAD;
}

void champion_fall_damage(bool use_rope)
{
    if (!g_party_champion_count)
        return;
    if (!use_rope) {
        if (champion_damage_all(PIT_ATTACK, WOUND_LEGS | WOUND_FEET, ATTACK_SELF))
            sound_request_play(SOUND_SCREAM, (Location){g_party_map_x, g_party_map_y}, SOUND_MODE_NOW, 0);
        return;
    }
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *c = &g_champions[i];
        if (c->current_health)
            champion_decrement_stamina(i, (uint16_t)(c->load * ROPE_STAMINA_FACTOR) / champion_get_maximum_load(i) +
                                              1); /* 16-bit wrap */
    }
}

/* Scents away from the party fade; a faded first scent is removed. */
static void fade_scents(void)
{
    uint16_t here = party_pack_position(g_party_map_index, g_party_map_x, g_party_map_y);
    for (int16_t i = 0; i < g_party.scent_count - 1;) {
        if (g_party.scents[i] != here) {
            g_party.scent_strengths[i] = max16(SCENT_STRENGTH_MINIMUM, g_party.scent_strengths[i] - 1);
            if (!g_party.scent_strengths[i] && !i) {
                party_delete_scent(0);
                continue;
            }
        }
        i++;
    }
}

/* Mana comes back for stamina; wizard and priest levels and wisdom make it more likely. */
static void regenerate_mana(uint16_t i, uint16_t chance)
{
    Champion *c = &g_champions[i];
    if (c->current_mana < c->maximum_mana) {
        int16_t levels = champion_get_skill_level(i, SKILL_WIZARD, false, false) +
                         champion_get_skill_level(i, SKILL_PRIEST, false, false);
        if (levels + c->statistics[STAT_WISDOM][STAT_CURRENT] <= chance)
            return;
        int16_t gain = c->maximum_mana / MANA_GAIN_DIVISOR;
        if (g_session.party_sleeping)
            gain *= 2;
        gain++;
        champion_decrement_stamina(i, max16(MANA_STAMINA_COST_MINIMUM, MANA_STAMINA_COST_BASE - levels) * gain);
        c->current_mana += min16(gain, c->maximum_mana - c->current_mana);
    } else if (c->current_mana > c->maximum_mana) {
        c->current_mana--;
    }
}

/* Food and water go down. Stamina comes back while fed, and goes down while starving. */
static void consume_food_and_water(uint16_t i, uint16_t time)
{
    Champion *c = &g_champions[i];
    int16_t food = c->food;
    int16_t water = c->water;
    uint16_t steps = FOOD_WATER_LAST_STEPS;
    for (int16_t half = c->maximum_stamina >> 1; half > c->current_stamina; half >>= 1)
        steps += 2;
    int16_t loss = 0;
    int16_t rate = bounded_value(1, (c->maximum_stamina >> STAMINA_RATE_SHIFT) - 1, STAMINA_RATE_MAXIMUM);
    if (g_session.party_sleeping)
        rate *= 2;
    uint16_t idle = time - g_last_party_movement_time; /* 16-bit difference */
    if (idle > IDLE_TICKS_HUNGRY)
        rate += idle > IDLE_TICKS_VERY_HUNGRY ? 2 : 1;
    do {
        bool last_steps = steps <= FOOD_WATER_LAST_STEPS;
        if (c->food < FOOD_WATER_STARVING) {
            if (last_steps) {
                loss += rate;
                c->food -= 2;
            }
        } else {
            if (c->food >= 0)
                loss -= rate;
            c->food -= last_steps ? 2 : steps >> 1;
        }
        if (c->water < FOOD_WATER_STARVING) {
            if (last_steps) {
                loss += rate;
                c->water -= 1;
            }
        } else {
            if (c->water >= 0)
                loss -= rate;
            c->water -= last_steps ? 1 : steps >> 2;
        }
    } while (--steps && c->current_stamina - loss < c->maximum_stamina);
    champion_decrement_stamina(i, loss);
    if (c->food < FOOD_WATER_MINIMUM)
        c->food = FOOD_WATER_MINIMUM;
    if (c->water < FOOD_WATER_MINIMUM)
        c->water = FOOD_WATER_MINIMUM;
    if (g_session.cheat_food) {
        c->food = food;
        c->water = water;
    }
}

/* Health comes back while stamina is above a quarter; vitality makes it more likely. */
static void regenerate_health(Champion *c, uint16_t chance)
{
    if (c->current_health >= c->maximum_health || c->maximum_stamina >> 2 > c->current_stamina ||
        c->statistics[STAT_VITALITY][STAT_CURRENT] + HEALTH_GAIN_VITALITY_BONUS <= chance)
        return;
    int16_t gain = (c->maximum_health >> HEALTH_GAIN_SHIFT) + 1;
    if (g_session.party_sleeping)
        gain *= 2;
    if (object_get_icon_index(c->slots[SLOT_NECK]) == ICON_EKKHARD_CROSS)
        gain += (gain >> 1) + 1;
    c->current_health += min16(gain, c->maximum_health - c->current_health);
}

/* Current statistics move one step toward their maximum. */
static void restore_statistics(Champion *c)
{
    for (int s = 0; s < STAT_COUNT; s++) {
        uint8_t *st = c->statistics[s];
        if (st[STAT_MAXIMUM] > st[STAT_CURRENT])
            st[STAT_CURRENT]++;
        else if (st[STAT_MAXIMUM] < st[STAT_CURRENT])
            st[STAT_CURRENT] -= st[STAT_CURRENT] / st[STAT_MAXIMUM];
    }
}

void champion_apply_time_effects(void)
{
    if (!g_party_champion_count)
        return;
    fade_scents();
    uint16_t time = g_game_time;
    uint16_t chance = ((time & TIME_CHANCE_MIDDLE_BIT) + ((time & TIME_CHANCE_LOW_BIT) >> 2) +
                       ((time & TIME_CHANCE_HIGH_BIT) << 2)) >>
                      2; /* time bits 6-8 shuffled: 0-112 */
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *c = &g_champions[i];
        if (!c->current_health || i == g_session.candidate_champion_index)
            continue;
        regenerate_mana(i, chance);
        for (int16_t s = SKILL_COUNT - 1; s >= 0; s--)
            if (c->skills[s].temporary_experience)
                c->skills[s].temporary_experience--;
        consume_food_and_water(i, time);
        regenerate_health(c, chance);
        if (!(time & (g_session.party_sleeping ? STATISTICS_RESTORE_SLEEPING_MASK : STATISTICS_RESTORE_MASK)))
            restore_statistics(c);
        if (!g_session.party_sleeping && c->direction != g_party_direction &&
            g_game_time - TURN_BACK_TICKS > (uint32_t)g_last_creature_attack_time) {
            c->direction = g_party_direction;
            c->maximum_damage_received = 0;
            c->attributes |= CHAMPION_ATTR_ICON;
        }
        c->attributes |= CHAMPION_ATTR_STATISTICS;
    }
}

uint16_t champion_take_dead_champions(void)
{
    uint16_t dead = g_dead_champions;
    g_dead_champions = 0;
    return dead;
}

void champion_add_redraw_flags(int16_t champion_index, uint16_t attributes)
{
    g_champions[champion_index].attributes |= attributes;
}

void champion_face_party_direction(Champion *champion)
{
    if (champion->direction != g_party_direction) {
        champion->direction = g_party_direction;
        champion->attributes |= CHAMPION_ATTR_ICON;
    }
}

int16_t champion_adjust_by_stamina(int16_t champion_index, int16_t number)
{
    const Champion *champion = &g_champions[champion_index];
    int16_t half = champion->maximum_stamina >> 1;
    if (champion->current_stamina < half)
        number = (number >> 1) + (int16_t)((int32_t)number * champion->current_stamina / half); /* 16-bit quotient */
    return number;
}

uint16_t champion_get_adjusted_attack(int16_t champion_index, uint16_t statistic, uint16_t amount)
{
    const Champion *champion = &g_champions[champion_index];
    int16_t factor = STAT_LIMIT - champion->statistics[statistic][STAT_CURRENT];
    return factor < ATTACK_FACTOR_MIN ? amount >> ATTACK_LOW_FACTOR_SHIFT
                                      : scale_value(amount, ATTACK_FACTOR_SHIFT, factor);
}

void champion_adjust_statistic(int16_t champion_index, uint16_t statistic, int16_t delta)
{
    uint8_t *s = g_champions[champion_index].statistics[statistic];
    if (delta < 0) {
        delta = max16(delta, s[STAT_MINIMUM] - s[STAT_CURRENT]);
    } else {
        if (s[STAT_CURRENT] > STAT_HIGH)
            delta = (s[STAT_CURRENT] > STAT_VERY_HIGH ? delta >> 2 : delta >> 1) + 1;
        delta = min16(delta, STAT_LIMIT - s[STAT_CURRENT]);
    }
    s[STAT_CURRENT] += delta;
}

bool champion_is_lucky(int16_t champion_index, uint16_t percentage)
{
    uint8_t *luck = g_champions[champion_index].statistics[STAT_LUCK];
    if (random_bit() && percentage < random16() % PERCENT_RANGE)
        return true;
    bool lucky = (int8_t)luck[STAT_CURRENT] > 0 && percentage < random16() % (luck[STAT_CURRENT] * 2);
    int16_t change = lucky ? -LUCK_STEP : LUCK_STEP;
    luck[STAT_CURRENT] =
        bounded_value((int8_t)luck[STAT_MINIMUM], (int8_t)luck[STAT_CURRENT] + change, (int8_t)luck[STAT_MAXIMUM]);
    return lucky;
}
