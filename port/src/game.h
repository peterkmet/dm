#ifndef GAME_H
#define GAME_H
#include <stdint.h>

#define NORMALIZE(direction) ((direction) & 3)
#define CHAMPION_MAX_COUNT 4
#define STAT_LIMIT 170                 /* no statistic grows above this value */
#define PARTY_SCENT_COUNT 24
#define ACTIVE_GROUP_MAX_COUNT 110     /* creature groups that can act at the same time */
#define CSB_ACTIVE_GROUP_MAX_COUNT 60  /* active group slots of a new CSB game */
#define WATCHDOG_INTERVAL 300          /* game ticks between two watchdog events */

enum { STAT_LUCK, STAT_STRENGTH, STAT_DEXTERITY, STAT_WISDOM, STAT_VITALITY, STAT_ANTIMAGIC, STAT_ANTIFIRE, STAT_COUNT };
enum { STAT_MAXIMUM, STAT_CURRENT, STAT_MINIMUM, STAT_VALUE_COUNT };

typedef struct {
    int16_t temporary_experience;
    int32_t experience;
} Skill;

typedef struct {
    char name[8];
    char title[20];
    uint8_t direction, cell;
    uint8_t action_index;                      /* 255: none */
    uint8_t symbol_step;
    char symbols[5];
    uint8_t direction_maximum_damage_received, maximum_damage_received;
    uint8_t poison_event_count;
    int16_t enable_action_event_index;         /* event 11, or -1 */
    int16_t hide_damage_received_event_index;  /* event 12, or -1 */
    uint16_t attributes;
    uint16_t wounds;
    int16_t current_health, maximum_health;
    int16_t current_stamina, maximum_stamina;
    int16_t current_mana, maximum_mana;
    int16_t action_defense;
    int16_t food, water;
    uint8_t statistics[STAT_COUNT][STAT_VALUE_COUNT];
    Skill skills[20];                          /* SKILL_COUNT */
    uint16_t slots[30];                        /* SLOT_COUNT */
    uint16_t load;
    int16_t shield_defense;
    uint8_t magic_map[6];                      /* CSB record bytes 0x118-0x11d, kept as they are */
} Champion;

typedef struct {
    int16_t magical_light_amount;
    uint8_t event73_count_thieves_eye, event79_count_footprints;
    int16_t shield_defense, fire_shield_defense, spell_shield_defense;
    uint8_t scent_count, freeze_life_ticks, first_scent_index, last_scent_index;
    uint16_t scents[PARTY_SCENT_COUNT];                       /* x bits 11-15, y bits 6-10, map bits 0-5 */
    uint8_t scent_strengths[PARTY_SCENT_COUNT];
    uint8_t event71_count_invisibility;
} Party;

typedef struct {
    uint32_t map_time;                         /* map bits 24-31, time bits 0-23; free slot: next free index */
    uint8_t type;                              /* 0: free slot */
    uint8_t priority;                          /* champion events: champion index */
    uint16_t b;                                /* square events: x << 8 | y */
    uint16_t c;                                /* square events: cell << 8 | effect */
} TimelineEvent;

typedef struct {
    int16_t group_index;                       /* -1: free entry */
    uint8_t directions, cells;
    uint8_t last_move_time, delay_fleeing;
    uint8_t target_x, target_y, prior_x, prior_y, home_x, home_y;
    uint8_t aspect[4];
} ActiveGroup;

extern Party g_party;
extern Champion g_champions[CHAMPION_MAX_COUNT];
extern uint16_t g_party_champion_count;
extern int16_t g_leader_index, g_magic_caster_champion_index;
extern uint16_t g_leader_hand_object;
extern int16_t g_party_is_sleeping, g_party_dead;
extern uint32_t g_game_time, g_game_id;
extern int32_t g_last_creature_attack_time, g_last_party_movement_time, g_watchdog_time;
extern int16_t g_disabled_movement_ticks, g_projectile_disabled_movement_ticks;
extern int16_t g_last_projectile_disabled_movement_direction;

extern TimelineEvent *g_timeline_events;       /* g_event_max_count slots */
extern uint16_t *g_timeline;                   /* min-heap of slot indices */
extern uint16_t g_timeline_count, g_event_high_water;
extern int16_t g_first_unused_event_index;

extern ActiveGroup *g_active_groups;
extern uint16_t g_active_group_count, g_max_active_group_count;

void game_new(void);

int16_t min16(int16_t a, int16_t b);
int16_t max16(int16_t a, int16_t b);
int16_t bounded_value(int16_t min, int16_t value, int16_t max);
int32_t scale_value(uint16_t a, int16_t shift, uint16_t b);
void update_map_coordinates_after_relative_movement(int16_t direction, int16_t steps_forward, int16_t steps_right, int16_t *map_x, int16_t *map_y);

void timeline_init(void);
void timeline_reset(void);
int16_t timeline_is_event_a_before_b(const TimelineEvent *a, const TimelineEvent *b);
uint16_t timeline_get_index(uint16_t event_index);
void timeline_fix_chronology(uint16_t timeline_index);
void timeline_delete_event(uint16_t event_index);
int16_t timeline_find_mergeable_event(TimelineEvent *event);
int16_t timeline_add_event(TimelineEvent *event);
void timeline_extract_first_event(TimelineEvent *event);
int16_t timeline_is_first_event_expired(void);
void timeline_add_watchdog_event(void);

void group_init_active_groups(void);

int16_t champion_get_stamina_adjusted_value(const Champion *champion, int16_t value);
uint16_t champion_get_statistic_adjusted_attack(const Champion *champion, uint16_t statistic, uint16_t attack);
void adjust_statistic_current_value(Champion *champion, uint16_t statistic, int16_t delta);
int16_t champion_is_lucky(Champion *champion, uint16_t percentage);

#endif
