#include "champion.h"
#include "action.h"
#include "adventure.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "random.h"
#include "recruit.h"
#include "sound.h"
#include "tables.h"
#include "timeline_process.h"
#include <string.h>

/* KAOS data addresses. */
#define SLOT_DROP_ORDER 0x100d0e   /* g_slot_drop_order[30] */

#define CELL_COUNT 4
#define INITIAL_FOOD_WATER 1500
#define MAXIMUM_HEALTH 999
#define MAXIMUM_STAMINA 9999
#define MAXIMUM_MANA 900
#define FOOD_WATER_STARVING (-512)
#define LEVEL_EXPERIENCE 500              /* experience for level 2; each level doubles it */
#define TEMPORARY_EXPERIENCE_LIMIT 32000
#define RECENT_ATTACK_TICKS 25            /* hidden skills gain double experience */
#define OLD_ATTACK_TICKS 150              /* fighting skills gain half experience */
#define TURN_BACK_TICKS 60                /* a champion faces the party direction again */
#define HIDE_DAMAGE_TICKS 5
#define POISON_STEP_TICKS 36
#define REINCARNATION_POINTS 12
#define CSB_REINCARNATION_STAT_LOSS_SHIFT 3       /* CSB: each statistic but luck loses 1/8 */
#define CSB_REINCARNATION_BAR_SHIFT 2             /* CSB: health, stamina, and mana drop to 1/4 */
#define RESURRECTION_MINIMUM_HEALTH 25
#define SCENT_STRENGTH_MINIMUM 0
#define PIT_ATTACK 20

/* Weapon classes that use the swing, throw, and shoot skills. */
enum { CLASS_SWING_WEAPON = 0, CLASS_DAGGER_AND_AXES = 2, CLASS_FIRST_AMMUNITION = 16, CLASS_AMMUNITION_END = 112 };

uint8_t g_champion_portraits[CHAMPION_MAX_COUNT][CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT];
int16_t g_candidate_champion_ordinal;
int16_t g_champion_pending_damage[CHAMPION_MAX_COUNT];
uint16_t g_champion_pending_wounds[CHAMPION_MAX_COUNT];
int16_t g_champion_skill_level_up[CHAMPION_MAX_COUNT][SKILL_BASE_COUNT];
uint16_t g_dead_champions;

/* Offsets of the original record. */
enum {
    REC_NAME = 0x00, REC_TITLE = 0x08, REC_DIRECTION = 0x1c, REC_CELL = 0x1d, REC_ACTION = 0x20, REC_SYMBOL_STEP = 0x21,
    REC_SYMBOLS = 0x22, REC_DAMAGE_DIRECTION = 0x28, REC_MAXIMUM_DAMAGE = 0x29, REC_POISON_EVENTS = 0x2a, REC_WORDS = 0x2c,
    REC_STATISTICS = 0x46, REC_SKILLS = 0x5c, REC_SLOTS = 0xd4, REC_LOAD = 0x110, REC_SHIELD = 0x112, REC_MAGIC_MAP = 0x118,
};
#define REC_WORD_COUNT 13
#define REC_SKILL_SIZE 6
#define WORDS(c) &c->enable_action_event_index, &c->hide_damage_received_event_index, (int16_t *)&c->attributes, \
    (int16_t *)&c->wounds, &c->current_health, &c->maximum_health, &c->current_stamina, &c->maximum_stamina, \
    &c->current_mana, &c->maximum_mana, &c->action_defense, &c->food, &c->water

void champion_pack(const Champion *c, uint8_t *out)
{
    memset(out, 0, CHAMPION_RECORD_SIZE);
    memcpy(out + REC_NAME, c->name, sizeof c->name);
    memcpy(out + REC_TITLE, c->title, sizeof c->title);
    out[REC_DIRECTION] = c->direction, out[REC_CELL] = c->cell;
    out[REC_ACTION] = c->action_index, out[REC_SYMBOL_STEP] = c->symbol_step;
    memcpy(out + REC_SYMBOLS, c->symbols, sizeof c->symbols);
    out[REC_DAMAGE_DIRECTION] = c->direction_maximum_damage_received;
    out[REC_MAXIMUM_DAMAGE] = c->maximum_damage_received;
    out[REC_POISON_EVENTS] = c->poison_event_count;
    const int16_t *words[REC_WORD_COUNT] = {WORDS(c)};
    for (int i = 0; i < REC_WORD_COUNT; i++)
        WR16(out + REC_WORDS + 2 * i, *words[i]);
    memcpy(out + REC_STATISTICS, c->statistics, sizeof c->statistics);
    for (int i = 0; i < SKILL_COUNT; i++) {
        WR16(out + REC_SKILLS + REC_SKILL_SIZE * i, c->skills[i].temporary_experience);
        WR32(out + REC_SKILLS + REC_SKILL_SIZE * i + 2, (uint32_t)c->skills[i].experience);
    }
    for (int i = 0; i < SLOT_COUNT; i++)
        WR16(out + REC_SLOTS + 2 * i, c->slots[i]);
    WR16(out + REC_LOAD, c->load);
    WR16(out + REC_SHIELD, c->shield_defense);
    memcpy(out + REC_MAGIC_MAP, c->magic_map, sizeof c->magic_map);
}

void champion_unpack(const uint8_t *in, Champion *c)
{
    memset(c, 0, sizeof *c);
    memcpy(c->name, in + REC_NAME, sizeof c->name);
    memcpy(c->title, in + REC_TITLE, sizeof c->title);
    c->direction = in[REC_DIRECTION], c->cell = in[REC_CELL];
    c->action_index = in[REC_ACTION], c->symbol_step = in[REC_SYMBOL_STEP];
    memcpy(c->symbols, in + REC_SYMBOLS, sizeof c->symbols);
    c->direction_maximum_damage_received = in[REC_DAMAGE_DIRECTION];
    c->maximum_damage_received = in[REC_MAXIMUM_DAMAGE];
    c->poison_event_count = in[REC_POISON_EVENTS];
    int16_t *words[REC_WORD_COUNT] = {WORDS(c)};
    for (int i = 0; i < REC_WORD_COUNT; i++)
        *words[i] = (int16_t)BE16(in + REC_WORDS + 2 * i);
    memcpy(c->statistics, in + REC_STATISTICS, sizeof c->statistics);
    for (int i = 0; i < SKILL_COUNT; i++) {
        c->skills[i].temporary_experience = (int16_t)BE16(in + REC_SKILLS + REC_SKILL_SIZE * i);
        c->skills[i].experience = (int32_t)BE32(in + REC_SKILLS + REC_SKILL_SIZE * i + 2);
    }
    for (int i = 0; i < SLOT_COUNT; i++)
        c->slots[i] = BE16(in + REC_SLOTS + 2 * i);
    c->load = BE16(in + REC_LOAD);
    c->shield_defense = (int16_t)BE16(in + REC_SHIELD);
    memcpy(c->magic_map, in + REC_MAGIC_MAP, sizeof c->magic_map);
}

/* "<prefix><name><suffix>" in the champion color on a new message line. */
static void print_champion_message(uint16_t champion_index, int16_t text)
{
    int16_t color = kaos_u8(CHAMPION_COLORS + champion_index);
    message_new_line(g_game_time);
    message_print(color, gfx_string(text), g_game_time);
    message_print(color, g_champions[champion_index].name, g_game_time);
    message_print(color, gfx_string(text + 1), g_game_time);
}

static void square_in_front(int16_t *x, int16_t *y)
{
    *x = g_party_map_x, *y = g_party_map_y;
    update_map_coordinates_after_relative_movement(g_party_direction, 1, 0, x, y);
}

#define THING_WITHOUT_CELL 0x3fff

static uint16_t with_cell(uint16_t thing, uint16_t cell)
{
    return cell << 14 | (thing & THING_WITHOUT_CELL);
}

/* The living champion in a cell of the party square, or -1. */
int16_t champion_get_index_in_cell(int16_t cell)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].cell == cell && g_champions[i].current_health)
            return i;
    return -1;
}

/* The leader carries the hand object, so its weight moves to the new leader. */
void command_set_leader(int16_t champion_index)
{
    if (champion_index == g_leader_index || (champion_index != -1 && !g_champions[champion_index].current_health))
        return;
    if (g_leader_index != -1) {
        Champion *old = &g_champions[g_leader_index];
        old->attributes |= CHAMPION_ATTR_LOAD | CHAMPION_ATTR_NAME_TITLE;
        old->load -= get_object_weight(g_leader_hand_object);
        g_leader_index = -1;
    }
    if (champion_index != -1) {
        Champion *c = &g_champions[champion_index];
        g_leader_index = champion_index;
        c->direction = g_party_direction;
        c->load += get_object_weight(g_leader_hand_object);
        if (champion_index + 1 != g_candidate_champion_ordinal)
            c->attributes |= CHAMPION_ATTR_LOAD | CHAMPION_ATTR_NAME_TITLE | CHAMPION_ATTR_ICON;
    }
}

/* Input and screen changes of waking up belong to the interface. */
void champion_wake_up(void)
{
    g_party_is_sleeping = 0;
}

/* The slot that a candidate possession goes to: hands, body, pouches, neck, then the backpack. */
static uint16_t candidate_slot(const Champion *c, uint16_t thing, uint16_t *next_backpack_slot)
{
    uint16_t allowed = tables_object_info(get_object_info_index(thing))->allowed_slots;
    int neck_free = allowed & *tables_slot_mask(SLOT_NECK) && c->slots[SLOT_NECK] == THING_NONE;
    uint16_t slot = THING_NONE;
    switch (THING_TYPE(thing)) {
    case THING_ARMOUR:
        for (slot = SLOT_HEAD; slot <= SLOT_FEET && !(allowed & *tables_slot_mask(slot)); slot++)
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
        neck_free = allowed & *tables_slot_mask(SLOT_NECK) && c->slots[SLOT_NECK] == THING_NONE;
        slot = neck_free ? SLOT_NECK : (*next_backpack_slot)++;
    }
    return slot;
}

/* The objects in the facing cell of the mirror wall become the candidate's possessions. They
   stay in the wall square list until the player accepts the candidate. */
static void add_candidate_possessions(uint16_t champion_index)
{
    int16_t x, y;
    square_in_front(&x, &y);
    uint16_t cell = (g_party_direction + 2) % CELL_COUNT, backpack = SLOT_BACKPACK_FIRST;
    for (uint16_t t = get_square_first_thing(x, y); t != THING_END; t = get_next_thing(t)) {
        uint16_t type = THING_TYPE(t);
        /* Fix: the original reuses a stale slot for things that are not objects; none lie on mirror walls. */
        if (type < THING_WEAPON || type > THING_JUNK || THING_CELL(t) != cell)
            continue;
        inventory_add_object_in_slot(champion_index, t, candidate_slot(&g_champions[champion_index], t, &backpack));
    }
}

/* Copies the portrait from the portrait sheet, clipped to the sheet; the pixels outside the sheet stay 0. */
void champion_set_portrait(int16_t champion_index, int16_t portrait_index)
{
    const Bitmap *portraits = gfx_bitmap(GFX_CHAMPION_PORTRAITS);
    uint8_t *px = g_champion_portraits[champion_index];
    int16_t x, y;
    memset(px, 0, CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT);
    recruit_portrait_origin(portrait_index, &x, &y);
    if (!portraits) return;
    for (int row = 0; row < CHAMPION_PORTRAIT_HEIGHT; row++)
        for (int column = 0; column < CHAMPION_PORTRAIT_WIDTH; column++) {
            int sx = x + column, sy = y + row;
            if (sx >= 0 && sx < portraits->w && sy >= 0 && sy < portraits->h)
                px[row * CHAMPION_PORTRAIT_WIDTH + column] = portraits->px[sy * portraits->w + sx];
        }
}

/* The party stands in front of a mirror. The text on the party square holds the champion data. */
void champion_add_candidate_to_party(int16_t portrait_index)
{
    if (!g_leader_empty_handed || g_party_champion_count >= CHAMPION_MAX_COUNT)
        return;
    uint16_t index = g_party_champion_count;
    champion_set_portrait((int16_t)index, portrait_index);
    Champion *c = &g_champions[index];
    memset(c, 0, sizeof *c);
    c->action_index = ACTION_NONE;
    c->enable_action_event_index = c->hide_damage_received_event_index = -1;
    c->direction = g_party_direction;
    int16_t n = 0;
    while (champion_get_index_in_cell((g_party_direction + n) % CELL_COUNT) != -1)
        n++;
    c->cell = (g_party_direction + n) % CELL_COUNT;
    c->attributes = CHAMPION_ATTR_ICON;
    c->direction_maximum_damage_received = g_party_direction;
    c->food = (random16() & 255) + INITIAL_FOOD_WATER;
    c->water = (random16() & 255) + INITIAL_FOOD_WATER;
    for (int s = 0; s < SLOT_COUNT; s++)
        c->slots[s] = THING_NONE;
    RecruitData r;
    if (!recruit_parse(recruit_text_thing(g_party_map_index, g_party_map_x, g_party_map_y), &r))
        return;
    memcpy(c->name, r.name, sizeof c->name);
    memcpy(c->title, r.title, sizeof c->title);
    if (r.male)
        c->attributes |= CHAMPION_ATTR_MALE;
    c->current_health = r.current_health, c->maximum_health = r.maximum_health;
    c->current_stamina = r.current_stamina, c->maximum_stamina = r.maximum_stamina;
    c->current_mana = r.current_mana, c->maximum_mana = r.maximum_mana;
    memcpy(c->statistics, r.statistics, sizeof c->statistics);
    for (int s = 0; s < SKILL_COUNT; s++)
        c->skills[s].experience = r.experience[s];
    g_candidate_champion_ordinal = index + 1;
    c->attributes |= CHAMPION_ATTR_ACTION_HAND;
    if (++g_party_champion_count == 1) {
        command_set_leader(0);
        g_magic_caster_champion_index = 0;
    }
    add_candidate_possessions(index);
    g_inventory_champion_ordinal = index + 1;
}

/* Reincarnation: all experience is lost and 12 random statistic points are gained. In CSB, the statistics but luck
   lose 1/8 first, down to their minimum, and health, stamina, and mana drop to 1/4. */
static void reincarnate(Champion *c)
{
    memset(c->skills, 0, sizeof c->skills);
    if (g_adventure == ADVENTURE_CSB) {
        for (int s = STAT_STRENGTH; s < STAT_COUNT; s++) {
            int16_t value = c->statistics[s][STAT_CURRENT];
            value = max16(c->statistics[s][STAT_MINIMUM], value - (value >> CSB_REINCARNATION_STAT_LOSS_SHIFT));
            c->statistics[s][STAT_CURRENT] = c->statistics[s][STAT_MAXIMUM] = (uint8_t)value;
        }
        c->current_health >>= CSB_REINCARNATION_BAR_SHIFT, c->maximum_health >>= CSB_REINCARNATION_BAR_SHIFT;
        c->current_stamina >>= CSB_REINCARNATION_BAR_SHIFT, c->maximum_stamina >>= CSB_REINCARNATION_BAR_SHIFT;
        c->current_mana >>= CSB_REINCARNATION_BAR_SHIFT, c->maximum_mana >>= CSB_REINCARNATION_BAR_SHIFT;
    }
    for (int i = 0; i < REINCARNATION_POINTS; i++) {
        uint16_t s = random16() % STAT_COUNT;
        c->statistics[s][STAT_CURRENT]++;
        c->statistics[s][STAT_MAXIMUM]++;
    }
}

/* Cancel removes the candidate. Resurrect and reincarnate take the possessions from the wall
   square and disable the mirror. The original also asks for a new name on reincarnation. */
void champion_click_in_resurrect_reincarnate_panel(int16_t command)
{
    uint16_t index = g_party_champion_count - 1;
    Champion *c = &g_champions[index];
    g_inventory_champion_ordinal = 0;
    g_candidate_champion_ordinal = 0;
    if (command == PANEL_CANCEL) {
        if (g_party_champion_count == 1) {
            command_set_leader(-1);
            g_magic_caster_champion_index = -1;
        }
        g_party_champion_count--;
        return;
    }
    int16_t x, y;
    square_in_front(&x, &y);
    for (uint16_t s = 0; s < SLOT_COUNT; s++)
        if (c->slots[s] != THING_NONE)
            unlink_thing_from_list(c->slots[s], 0, x, y);
    uint16_t mirror = get_square_first_thing(x, y);
    while (THING_TYPE(mirror) != THING_SENSOR)
        mirror = get_next_thing(mirror);
    sensor_disable(mirror);
    if (command == PANEL_REINCARNATE)
        reincarnate(c);
    if (g_party_champion_count == 1)
        g_last_party_movement_time = g_game_time;
    print_champion_message(index, command == PANEL_RESURRECT ? TEXT_RESURRECTED : TEXT_REINCARNATED);
}

/* Vi altar: the champion comes back with no objects and with a lower maximum health. */
void champion_resurrect(uint16_t champion_index)
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
    int16_t health = max16(RESURRECTION_MINIMUM_HEALTH, c->maximum_health - ((uint16_t)c->maximum_health >> 6) - 1);
    c->maximum_health = health;
    c->current_health = health >> 1;
    c->direction = g_party_direction;
    c->attributes |= CHAMPION_ATTR_ACTION_HAND | CHAMPION_ATTR_STATUS_BOX | CHAMPION_ATTR_ICON;
}

static uint16_t base_skill(uint16_t skill)
{
    return skill < SKILL_BASE_COUNT ? skill : (skill - SKILL_BASE_COUNT) / SKILL_BASE_COUNT;
}

/* Extra levels from the Firestaff in the action hand and from neck jewels. */
static int16_t object_skill_bonus(const Champion *c, uint16_t skill)
{
    int16_t action = object_get_icon_index(c->slots[SLOT_HAND_ACTION]);
    int16_t neck = object_get_icon_index(c->slots[SLOT_NECK]);
    int16_t bonus = action == ICON_FIRESTAFF ? 1 : action == ICON_FIRESTAFF_COMPLETE ? 2 : 0;
    switch (skill) {
    case SKILL_HEAL: return bonus + (neck == ICON_GEM_OF_AGES || action == ICON_SCEPTRE_OF_LYF);
    case SKILL_WIZARD: return bonus + (neck == ICON_PENDANT_FERAL);
    case SKILL_DEFEND: return bonus + (neck == ICON_EKKHARD_CROSS);
    case SKILL_INFLUENCE: return bonus + (neck == ICON_MOONSTONE);
    }
    return bonus;
}

/* Level 1 below 500 experience, one more level for each doubling. A hidden skill counts the
   mean of its own and its base skill experience. Sleeping champions have level 1. */
int16_t champion_get_skill_level(int16_t champion_index, uint16_t skill_and_flags)
{
    if (g_party_is_sleeping)
        return 1;
    const Champion *c = &g_champions[champion_index];
    uint16_t skill = skill_and_flags & ~(SKILL_IGNORE_OBJECT_MODIFIERS | SKILL_IGNORE_TEMPORARY_EXPERIENCE);
    int temporary = !(skill_and_flags & SKILL_IGNORE_TEMPORARY_EXPERIENCE);
    uint32_t experience = c->skills[skill].experience;
    if (temporary)
        experience += (uint16_t)c->skills[skill].temporary_experience;
    if (skill >= SKILL_BASE_COUNT) {
        const Skill *base = &c->skills[base_skill(skill)];
        experience += base->experience;
        if (temporary)
            experience += (uint16_t)base->temporary_experience;
        experience >>= 1;
    }
    int16_t level = 1;
    for (; experience >= LEVEL_EXPERIENCE; experience >>= 1)
        level++;
    if (!(skill_and_flags & SKILL_IGNORE_OBJECT_MODIFIERS))
        level += object_skill_bonus(c, skill);
    return level;
}

/* A new base skill level raises statistics, maximum health, stamina, and mana. */
static void gain_level(uint16_t champion_index, uint16_t base, uint16_t level)
{
    Champion *c = &g_champions[champion_index];
    uint8_t (*maximum)[3] = c->statistics;
    uint16_t r1 = random_bit(), r2 = random_bit() + 1, vitality = random_bit();
    if (base != SKILL_PRIEST)
        vitality &= level;
    maximum[STAT_VITALITY][STAT_MAXIMUM] += vitality;
    uint16_t stamina_gain = c->maximum_stamina, health_gain = level;
    maximum[STAT_ANTIFIRE][STAT_MAXIMUM] += ~level & random_bit();
    switch (base) {
    case SKILL_FIGHTER:
        stamina_gain >>= 4;
        health_gain = level * 3;
        maximum[STAT_STRENGTH][STAT_MAXIMUM] += r2;
        maximum[STAT_DEXTERITY][STAT_MAXIMUM] += r1;
        break;
    case SKILL_NINJA:
        stamina_gain /= 21;
        health_gain = level * 2;
        maximum[STAT_STRENGTH][STAT_MAXIMUM] += r1;
        maximum[STAT_DEXTERITY][STAT_MAXIMUM] += r2;
        break;
    case SKILL_PRIEST:
    case SKILL_WIZARD:
        if (base == SKILL_PRIEST) {
            stamina_gain /= 25;
            c->maximum_mana += level;
            health_gain = level + ((level + 1) >> 1);
            maximum[STAT_WISDOM][STAT_MAXIMUM] += r1;
        } else {
            stamina_gain >>= 5;
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
    g_champion_skill_level_up[champion_index][base] = 1;
    print_champion_message(champion_index, TEXT_LEVEL_GAINED + 2 * base);
}

/* Experience counts more on difficult maps and in recent fights. A hidden skill also trains its base skill. */
void champion_add_skill_experience(uint16_t champion_index, uint16_t skill, uint16_t experience)
{
    if (skill >= SKILL_SWING && skill <= SKILL_SHOOT && g_game_time - OLD_ATTACK_TICKS > (uint32_t)g_last_creature_attack_time)
        experience >>= 1;
    if (!experience)
        return;
    uint16_t difficulty = map_current_difficulty();
    if (difficulty)
        experience *= difficulty;
    Champion *c = &g_champions[champion_index];
    uint16_t base = base_skill(skill), flags = SKILL_IGNORE_OBJECT_MODIFIERS | SKILL_IGNORE_TEMPORARY_EXPERIENCE;
    uint16_t old_level = champion_get_skill_level(champion_index, base | flags);
    if (skill >= SKILL_BASE_COUNT && g_game_time - RECENT_ATTACK_TICKS < (uint32_t)g_last_creature_attack_time)
        experience *= 2;
    Skill *s = &c->skills[skill];
    s->experience = (uint32_t)s->experience + experience; /* 32-bit wrap */
    if ((uint16_t)s->temporary_experience < TEMPORARY_EXPERIENCE_LIMIT)
        s->temporary_experience += bounded_value(1, experience >> 3, 100);
    if (skill >= SKILL_BASE_COUNT)
        c->skills[base].experience = (uint32_t)c->skills[base].experience + experience;
    uint16_t level = champion_get_skill_level(champion_index, base | flags);
    if (level > old_level)
        gain_level(champion_index, base, level);
}

/* Tenths of a kilogram, rounded up to whole kilograms. */
uint16_t champion_get_maximum_load(const Champion *c)
{
    uint16_t load = game_get_stamina_adjusted_value(c, c->statistics[STAT_STRENGTH][STAT_CURRENT] * 8 + 100);
    if (c->wounds)
        load -= load >> (c->wounds & WOUND_LEGS ? 2 : 3);
    if (object_get_icon_index(c->slots[SLOT_FEET]) == ICON_ELVEN_BOOTS)
        load += load >> 4;
    load += 9;
    return load - load % 10;
}

/* Ticks between two party moves for this champion: more when the load is heavy. */
int16_t champion_get_movement_ticks(const Champion *c)
{
    uint16_t maximum = champion_get_maximum_load(c), load = c->load;
    int16_t ticks, wounded_feet;
    if (load < maximum) {
        ticks = (int32_t)maximum * 5 < (int32_t)load * 8 ? 3 : 2;
        wounded_feet = 1;
    } else {
        ticks = (uint16_t)((load - maximum) * 4) / maximum + 4;
        wounded_feet = 2;
    }
    if (c->wounds & WOUND_FEET)
        ticks += wounded_feet;
    if (object_get_icon_index(c->slots[SLOT_FEET]) == ICON_BOOTS_OF_SPEED)
        ticks--;
    return ticks;
}

/* Dexterity for hit and dodge rolls, lower with a heavy load, between 1 and 100. */
int16_t champion_get_dexterity(const Champion *c)
{
    int16_t dexterity = c->statistics[STAT_DEXTERITY][STAT_CURRENT] + (random16() & 7);
    int32_t product = (int32_t)((uint32_t)c->load * (uint32_t)(dexterity >> 1)); /* 32-bit signed product */
    dexterity -= product / champion_get_maximum_load(c);
    dexterity = max16(2, dexterity);
    if (g_party_is_sleeping)
        dexterity >>= 1;
    int16_t high = 100 - (random16() & 7);
    int16_t low = (random16() & 7) + 1;
    return bounded_value(low, dexterity >> 1, high);
}

/* Strength with the object in a hand: weight, weapon strength, and weapon skills. 0 to 100. */
int16_t champion_get_strength(int16_t champion_index, int16_t slot)
{
    const Champion *c = &g_champions[champion_index];
    int16_t strength = c->statistics[STAT_STRENGTH][STAT_CURRENT] + (random16() & 15);
    uint16_t thing = c->slots[slot], weight = get_object_weight(thing);
    uint16_t light = champion_get_maximum_load(c) >> 4;
    if (light >= weight) {
        strength += weight - 12;
    } else {
        uint16_t heavy = light + ((uint16_t)(light - 12) >> 1);
        if (heavy >= weight)
            strength += (uint16_t)(weight - light) >> 1;
        else
            strength -= (weight - heavy) * 2;
    }
    if (THING_TYPE(thing) == THING_WEAPON) {
        const WeaponInfo *w = tables_weapon_info(weapon_type(thing));
        int16_t skill = 0;
        strength += w->strength;
        if (w->weapon_class == CLASS_SWING_WEAPON || w->weapon_class == CLASS_DAGGER_AND_AXES)
            skill = champion_get_skill_level(champion_index, SKILL_SWING);
        if (w->weapon_class != CLASS_SWING_WEAPON && w->weapon_class < CLASS_FIRST_AMMUNITION)
            skill += champion_get_skill_level(champion_index, SKILL_THROW);
        if (w->weapon_class >= CLASS_FIRST_AMMUNITION && w->weapon_class < CLASS_AMMUNITION_END)
            skill += champion_get_skill_level(champion_index, SKILL_SHOOT);
        strength += skill * 2;
    }
    strength = game_get_stamina_adjusted_value(c, strength);
    if (c->wounds & (slot == SLOT_HAND_READY ? WOUND_READY_HAND : WOUND_ACTION_HAND))
        strength >>= 1;
    return bounded_value(0, strength >> 1, 100);
}

static const ArmourInfo *armour_info(uint16_t thing)
{
    return tables_armour_info(armour_type(thing));
}

/* Defense of one body part (slot 0-5). Shields in the hands count for every part. */
int16_t champion_get_wound_defense(int16_t champion_index, uint16_t slot_and_flags)
{
    const Champion *c = &g_champions[champion_index];
    int16_t sharp = slot_and_flags & WOUND_DEFENSE_USE_SHARP;
    uint16_t slot = slot_and_flags & ~WOUND_DEFENSE_USE_SHARP, shields = 0;
    for (int16_t hand = SLOT_HAND_READY; hand <= SLOT_HAND_ACTION; hand++) {
        uint16_t thing = c->slots[hand];
        if (THING_TYPE(thing) != THING_ARMOUR || !(armour_info(thing)->attributes & ARMOUR_SHIELD))
            continue;
        uint16_t sum = champion_get_strength(champion_index, hand) + get_armour_defense(armour_info(thing), sharp);
        shields += (uint16_t)(sum * *tables_wound_defense_factor(slot)) >> (hand == slot ? 4 : 5);
    }
    uint16_t luck = random16() % ((c->statistics[STAT_VITALITY][STAT_CURRENT] >> 3) + 1);
    if (sharp)
        luck >>= 1;
    int16_t defense = luck + c->action_defense + c->shield_defense + g_party.shield_defense + shields;
    if (slot > SLOT_HAND_ACTION && THING_TYPE(c->slots[slot]) == THING_ARMOUR)
        defense += get_armour_defense(armour_info(c->slots[slot]), sharp);
    if (c->wounds & 1 << slot)
        defense -= random_2bits() + 8;
    if (g_party_is_sleeping)
        defense >>= 1;
    return bounded_value(0, defense >> 1, 100);
}

/* Attack after the champion's resistances; armour (wound defense) reduces normal attacks. */
static int16_t resisted_attack(int16_t champion_index, int16_t attack, uint16_t allowed_wounds, int16_t attack_type)
{
    const Champion *c = &g_champions[champion_index];
    uint16_t count = 0, defense = 0;
    for (int16_t slot = SLOT_HAND_READY; slot <= SLOT_FEET; slot++)
        if (allowed_wounds & 1 << slot) {
            count++;
            defense += champion_get_wound_defense(champion_index, slot | (attack_type == ATTACK_SHARP ? WOUND_DEFENSE_USE_SHARP : 0));
        }
    if (count)
        defense /= count;
    switch (attack_type) {
    case ATTACK_FIRE:
        attack = game_get_statistic_adjusted_attack(c, STAT_ANTIFIRE, attack) - g_party.fire_shield_defense;
        break;
    case ATTACK_SELF:
        defense = (defense >> 1) + champion_get_skill_level(champion_index, SKILL_NINJA);
        break;
    case ATTACK_MAGIC:
        return game_get_statistic_adjusted_attack(c, STAT_ANTIMAGIC, attack) - g_party.spell_shield_defense;
    case ATTACK_PSYCHIC: {
        int16_t wisdom = 115 - c->statistics[STAT_WISDOM][STAT_CURRENT];
        return wisdom <= 0 ? 0 : (int16_t)scale_value(attack, 6, wisdom);
    }
    }
    return attack <= 0 ? 0 : (int16_t)scale_value(attack, 6, 130 - defense);
}

/* Damage arrives at the end of the tick. Attack type ATTACK_RAW skips defense and wounds. */
int16_t champion_add_pending_damage_and_wounds(int16_t champion_index, int16_t attack, uint16_t allowed_wounds, int16_t attack_type)
{
    if (champion_index == -1 || champion_index + 1 == g_candidate_champion_ordinal || attack <= 0)
        return 0;
    const Champion *c = &g_champions[champion_index];
    if (!c->current_health)
        return 0;
    if (attack_type != ATTACK_RAW) {
        attack = resisted_attack(champion_index, attack, allowed_wounds, attack_type);
        if (attack <= 0)
            return 0;
        int16_t threshold = game_get_statistic_adjusted_attack(c, STAT_VITALITY, (random16() & 127) + 10);
        if (threshold < attack)
            do {
                g_champion_pending_wounds[champion_index] |= allowed_wounds & 1 << (random16() & 7);
                threshold *= 2;
            } while (threshold < attack && threshold);
        if (g_party_is_sleeping)
            champion_wake_up();
    }
    g_champion_pending_damage[champion_index] += attack;
    return attack;
}

/* The main loop calls this every tick. A champion at 0 health dies. */
void champion_apply_and_draw_pending_damage_and_wounds(void)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *c = &g_champions[i];
        uint16_t wounds = g_champion_pending_wounds[i];
        c->wounds |= wounds;
        g_champion_pending_wounds[i] = 0;
        int16_t damage = g_champion_pending_damage[i];
        if (!damage)
            continue;
        g_champion_pending_damage[i] = 0;
        if (!c->current_health)
            continue;
        int16_t health = c->current_health - damage;
        if (health <= 0 && g_cheat_immortal) health = 1;
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
            g_timeline_events[e].map_time = map_time;
            timeline_fix_chronology(timeline_get_index(e));
        }
    }
}

static void drop_on_party_square(uint16_t thing, uint16_t cell)
{
    move_get_move_result(with_cell(thing, cell), -1, 0, g_party_map_x, g_party_map_y);
}

/* All objects fall to the floor in the champion's cell. */
void champion_drop_all_objects(uint16_t champion_index)
{
    uint16_t cell = g_champions[champion_index].cell;
    for (uint16_t i = 0; i < SLOT_COUNT; i++) {
        uint16_t thing = inventory_get_object_removed_from_slot(champion_index, kaos_u16(SLOT_DROP_ORDER + i * 2));
        if (thing != THING_NONE)
            drop_on_party_square(thing, cell);
    }
}

/* Closes the champion's inventory and action menu, drops all objects and the bones on the party
   square, and passes the leader and the spell casting to the first living champion. The party dies
   with its last champion. */
void champion_kill(uint16_t champion_index)
{
    Champion *c = &g_champions[champion_index];
    if (champion_index + 1 == g_acting_champion_ordinal)
        action_clear_acting_champion();
    c->current_health = 0;
    c->attributes |= CHAMPION_ATTR_STATUS_BOX;
    g_dead_champions |= 1 << champion_index;
    champion_drop_all_objects(champion_index);
    uint16_t bones = get_unused_thing(THING_JUNK | NO_JUNK_RESERVE);
    if (bones != THING_NONE) {
        junk_set_type(bones, JUNK_BONES);
        object_set_important(bones);
        junk_set_charges(bones, champion_index);
        drop_on_party_square(bones, c->cell);
    }
    c->symbol_step = 0;
    c->symbols[0] = 0;
    c->direction = g_party_direction;
    c->maximum_damage_received = 0;
    if (c->poison_event_count)
        champion_unpoison(champion_index);
    uint16_t alive = 0;
    while (alive < g_party_champion_count && !g_champions[alive].current_health)
        alive++;
    if (alive == g_party_champion_count) {
        g_party_dead = 1;
        return;
    }
    if (champion_index == g_leader_index)
        command_set_leader(alive);
    if (champion_index == g_magic_caster_champion_index) {
        g_magic_caster_champion_index = (int16_t)alive;
        g_redraw |= REDRAW_SPELL_AREA;
    } else {
        g_redraw |= REDRAW_SPELL_TABS;
    }
}

/* Each poison step deals attack / 64 (at least 1) and schedules the next step with attack - 1. */
void champion_poison(int16_t champion_index, uint16_t attack)
{
    if (champion_index == -1 || champion_index + 1 == g_candidate_champion_ordinal)
        return;
    Champion *c = &g_champions[champion_index];
    champion_add_pending_damage_and_wounds(champion_index, max16(1, attack >> 6), 0, ATTACK_RAW);
    c->attributes |= CHAMPION_ATTR_STATISTICS;
    if (--attack) {
        c->poison_event_count++;
        TimelineEvent e = {EVENT_MAP_TIME(g_party_map_index, g_game_time + POISON_STEP_TICKS), EVENT_POISON, champion_index, attack, 0};
        timeline_add_event(&e);
    }
}

/* Removes all poison events of the champion. */
void champion_unpoison(int16_t champion_index)
{
    if (champion_index == -1)
        return;
    for (uint16_t i = 0; i < g_event_high_water; i++)
        if (g_timeline_events[i].type == EVENT_POISON && g_timeline_events[i].priority == champion_index)
            timeline_delete_event(i);
    g_champions[champion_index].poison_event_count = 0;
}

/* Each champion takes attack +- attack / 8. Returns the number of champions that took damage. */
int16_t champion_damage_all(uint16_t attack, uint16_t allowed_wounds, int16_t attack_type)
{
    if (!attack)
        return 0;
    uint16_t spread = (attack >> 3) + 1;
    attack -= spread;
    spread *= 2;
    int16_t count = 0;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (champion_add_pending_damage_and_wounds(i, max16(1, attack + random16() % spread), allowed_wounds, attack_type))
            count++;
    return count;
}

/* Stamina below 0 becomes damage (half the deficit). */
void champion_decrement_stamina(int16_t champion_index, int16_t decrement)
{
    if (champion_index == -1)
        return;
    Champion *c = &g_champions[champion_index];
    c->current_stamina -= decrement;
    int16_t stamina = c->current_stamina;
    if (stamina <= 0) {
        c->current_stamina = 0;
        champion_add_pending_damage_and_wounds(champion_index, (int16_t)-stamina >> 1, 0, ATTACK_RAW);
    } else if (stamina > c->maximum_stamina) {
        c->current_stamina = c->maximum_stamina;
    }
    c->attributes |= CHAMPION_ATTR_STATISTICS | CHAMPION_ATTR_LOAD;
}

/* The party falls into a pit. With a rope it loses stamina instead. */
void champion_fall_damage(int16_t use_rope)
{
    if (!g_party_champion_count)
        return;
    if (!use_rope) {
        if (champion_damage_all(PIT_ATTACK, WOUND_LEGS | WOUND_FEET, ATTACK_SELF))
            sound_request_play(SOUND_SCREAM, g_party_map_x, g_party_map_y, 0);
        return;
    }
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *c = &g_champions[i];
        if (c->current_health)
            champion_decrement_stamina(i, (uint16_t)(c->load * 25) / champion_get_maximum_load(c) + 1);
    }
}

/* Removes the oldest scents first; the footprint range moves with the list. */
void champion_delete_scent(uint16_t scent_index)
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

/* Scents away from the party fade; a faded first scent is removed. */
static void fade_scents(void)
{
    uint16_t here = (g_party_map_x & 31) << 11 | (g_party_map_y & 31) << 6 | (g_party_map_index & 63);
    for (int16_t i = 0; i < g_party.scent_count - 1;) {
        if (g_party.scents[i] != here) {
            g_party.scent_strengths[i] = max16(SCENT_STRENGTH_MINIMUM, g_party.scent_strengths[i] - 1);
            if (!g_party.scent_strengths[i] && !i) {
                champion_delete_scent(0);
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
        int16_t levels = champion_get_skill_level(i, SKILL_WIZARD) + champion_get_skill_level(i, SKILL_PRIEST);
        if ((uint16_t)(levels + c->statistics[STAT_WISDOM][STAT_CURRENT]) <= chance)
            return;
        int16_t gain = c->maximum_mana / 40;
        if (g_party_is_sleeping)
            gain *= 2;
        gain++;
        champion_decrement_stamina(i, max16(7, 16 - levels) * gain);
        c->current_mana += min16(gain, c->maximum_mana - c->current_mana);
    } else if (c->current_mana > c->maximum_mana) {
        c->current_mana--;
    }
}

/* Food and water go down. Stamina comes back while fed, and goes down while starving. */
static void consume_food_and_water(uint16_t i, uint16_t time)
{
    Champion *c = &g_champions[i];
    int16_t food = c->food, water = c->water;
    uint16_t steps = 4;
    for (int16_t half = c->maximum_stamina >> 1; half > c->current_stamina; half >>= 1)
        steps += 2;
    int16_t loss = 0;
    int16_t rate = bounded_value(1, (c->maximum_stamina >> 8) - 1, 6);
    if (g_party_is_sleeping)
        rate *= 2;
    uint16_t idle = time - (uint16_t)g_last_party_movement_time;
    if (idle > 80)
        rate += idle > 250 ? 2 : 1;
    do {
        int last_steps = steps <= 4;
        if (c->food < FOOD_WATER_STARVING) {
            if (last_steps)
                loss += rate, c->food -= 2;
        } else {
            if (c->food >= 0)
                loss -= rate;
            c->food -= last_steps ? 2 : steps >> 1;
        }
        if (c->water < FOOD_WATER_STARVING) {
            if (last_steps)
                loss += rate, c->water -= 1;
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
    if (g_cheat_food)
        c->food = food, c->water = water;
}

/* Health comes back while stamina is above a quarter; vitality makes it more likely. */
static void regenerate_health(Champion *c, uint16_t chance)
{
    if (c->current_health >= c->maximum_health || c->maximum_stamina >> 2 > c->current_stamina ||
        c->statistics[STAT_VITALITY][STAT_CURRENT] + 12 <= chance)
        return;
    int16_t gain = (c->maximum_health >> 7) + 1;
    if (g_party_is_sleeping)
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

/* The main loop calls this every 64 ticks (16 while the party sleeps). */
void champion_apply_time_effects(void)
{
    if (!g_party_champion_count)
        return;
    fade_scents();
    uint16_t time = g_game_time;
    uint16_t chance = ((time & 128) + ((time & 256) >> 2) + ((time & 64) << 2)) >> 2; /* time bits 6-8 shuffled: 0-112 */
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        Champion *c = &g_champions[i];
        if (!c->current_health || i + 1 == g_candidate_champion_ordinal)
            continue;
        regenerate_mana(i, chance);
        for (int16_t s = SKILL_COUNT - 1; s >= 0; s--)
            if (c->skills[s].temporary_experience)
                c->skills[s].temporary_experience--;
        consume_food_and_water(i, time);
        regenerate_health(c, chance);
        if (!(time & (g_party_is_sleeping ? 63 : 255)))
            restore_statistics(c);
        if (!g_party_is_sleeping && c->direction != g_party_direction && g_game_time - TURN_BACK_TICKS > (uint32_t)g_last_creature_attack_time) {
            c->direction = g_party_direction;
            c->maximum_damage_received = 0;
            c->attributes |= CHAMPION_ATTR_ICON;
        }
        c->attributes |= CHAMPION_ATTR_STATISTICS;
    }
}
