/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "turn.h"
#include "champion.h"
#include "game.h"
#include "map.h"
#include "message.h"
#include "move.h"
#include "party.h"
#include "record.h"
#include "gameloop.h"
#include "request.h"
#include "state_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Limits of a turn. */
#define FRAMES_PER_TICK_LIMIT 16 /* a frozen game ends a turn after this many frames per tick */

/* Sizes of a batch. */
#define BATCH_ITEM_SIZE  16384 /* a quarter of the longest request line */
#define BATCH_DAMAGE_MAX 64

/* Counts of the things the party saw during a request. */
#define WITNESSED_EXPLOSIONS_MAX 32
#define WITNESSED_GROUPS_MAX     16

/* What the party saw during a request, also when it was gone at the end: the explosions, and the creature
   groups with their count when first seen and when last seen. */
typedef struct {
    StateSeenThing at;
    uint8_t type;
} WitnessedExplosion;
typedef struct {
    StateSeenThing at;
    uint16_t creature;
    int16_t before, after;
    bool in_view, killed;
} WitnessedGroup;

static struct {
    struct {
        bool pending, wait_only;
        uint32_t start_time;
        uint32_t frames;
        uint16_t min_ticks, max_ticks;
        bool has_damage;
        int16_t damage;
        bool moving; /* the action was a step forward and to the right */
        int16_t step_forward, step_right;
        int16_t rotation;             /* the action was a turn: 1 right, -1 left */
        int16_t map, x, y, direction; /* the party before the action */
        int16_t teleports;            /* g_party_teleport_count before the action */
        const char *stop;             /* wait: the news that ended it early, or NULL */
    } now;
    /* A batch: act_many runs a list of actions; each step is one turn. */
    struct {
        bool active;
        int ran;
        int16_t damages[BATCH_DAMAGE_MAX]; /* the action damage of each choose_action, in order */
        int damage_count;
        char *actions; /* a copy of the request; next is the rest of its actions array */
        const char *next;
        TurnActionFn run;
        uint32_t start_time;
        const char *stop, *error;
    } batch;
    /* What the party knew before a turn: act_many and wait stop on news against it. */
    struct {
        int16_t health[CHAMPION_MAX_COUNT];
        uint16_t groups[STATE_VISIBLE_GROUPS_MAX];
        int group_count;
        uint32_t message_expiry;
    } seen;
    struct {
        int explosion_count, group_count;
        WitnessedExplosion explosions[WITNESSED_EXPLOSIONS_MAX];
        WitnessedGroup groups[WITNESSED_GROUPS_MAX];
        int16_t health[CHAMPION_MAX_COUNT]; /* the health of each champion at the start of the request */
        uint16_t champion_count;
    } witnessed;
    /* The eye panel text of a look action, for the turn result. */
    struct {
        bool shown;
        int16_t count;
        char name[PANEL_LINE_SIZE], lines[PANEL_LINE_COUNT + 1][PANEL_LINE_SIZE];
    } look;
    bool client_gone; /* the client left during the pending turn */
} turn;

/* Ends the batch and releases its copy of the request. */
static void reset_batch(void)
{
    free(turn.batch.actions);
    memset(&turn.batch, 0, sizeof turn.batch);
}

/* Setup */

void turn_reset(void)
{
    free(turn.batch.actions);
    memset(&turn, 0, sizeof turn);
}

void turn_drop_client(void)
{
    if (turn.now.pending)
        turn.client_gone = true;
    else
        reset_batch();
}

/* Queries */

bool turn_is_pending(void)
{
    return turn.now.pending;
}

bool turn_is_game_over(void)
{
    return g_session.party_dead || g_session.won;
}

/* Actions */

void turn_begin_action(void)
{
    turn.now.has_damage = false;
    turn.look.shown = false;
    turn.now.moving = false;
    turn.now.rotation = 0;
    turn.now.map = g_party_map_index;
    turn.now.x = g_party_map_x;
    turn.now.y = g_party_map_y;
    turn.now.direction = g_party_direction;
    turn.now.teleports = g_party_teleport_count;
}

void turn_set_step(int16_t forward, int16_t right)
{
    turn.now.moving = true;
    turn.now.step_forward = forward;
    turn.now.step_right = right;
}

void turn_set_rotation(int16_t rotation)
{
    turn.now.rotation = rotation;
}

void turn_add_damage(int16_t damage)
{
    turn.now.has_damage = true;
    turn.now.damage = damage;
    if (turn.batch.active && turn.batch.damage_count < BATCH_DAMAGE_MAX)
        turn.batch.damages[turn.batch.damage_count++] = damage;
}

void turn_set_look(const char *name, const char lines[][PANEL_LINE_SIZE], int16_t count)
{
    if (count > PANEL_LINE_COUNT + 1)
        count = PANEL_LINE_COUNT + 1;
    turn.look.shown = true;
    turn.look.count = count;
    snprintf(turn.look.name, sizeof turn.look.name, "%s", name);
    for (int16_t i = 0; i < count; i++)
        snprintf(turn.look.lines[i], PANEL_LINE_SIZE, "%s", lines[i]);
}

/* Turns */

/* Adds the explosions and the creature counts that the party sees now to the record of the request. */
static void record_witnessed(void)
{
    StateSeenThing things[WITNESSED_EXPLOSIONS_MAX];
    int count = state_get_visible_things(THING_EXPLOSION, things, WITNESSED_EXPLOSIONS_MAX);
    for (int i = 0; i < count; i++) {
        uint8_t type = rec_explosion_type(things[i].thing);
        int known = 0;
        for (int k = 0; k < turn.witnessed.explosion_count; k++)
            known |= turn.witnessed.explosions[k].type == type && turn.witnessed.explosions[k].at.x == things[i].x &&
                     turn.witnessed.explosions[k].at.y == things[i].y;
        if (!known && turn.witnessed.explosion_count < WITNESSED_EXPLOSIONS_MAX)
            turn.witnessed.explosions[turn.witnessed.explosion_count++] = (WitnessedExplosion){things[i], type};
    }
    for (int k = 0; k < turn.witnessed.group_count; k++)
        turn.witnessed.groups[k].in_view = false;
    count = state_get_visible_things(THING_GROUP, things, STATE_VISIBLE_GROUPS_MAX);
    for (int i = 0; i < count; i++) {
        int k = 0;
        while (k < turn.witnessed.group_count && turn.witnessed.groups[k].at.thing != things[i].thing)
            k++;
        if (k == WITNESSED_GROUPS_MAX)
            continue;
        int16_t creatures = rec_group_last_creature(things[i].thing) + 1;
        if (k == turn.witnessed.group_count) { /* a group seen first: count it once, not on each tick */
            turn.witnessed.groups[k] =
                (WitnessedGroup){.creature = rec_group_creature_type(things[i].thing), .before = creatures};
            turn.witnessed.group_count++;
        }
        turn.witnessed.groups[k].at = things[i];
        turn.witnessed.groups[k].after = creatures;
        turn.witnessed.groups[k].in_view = true;
    }
    for (int k = 0; k < turn.witnessed.group_count; k++) /* a killed group frees its record */
        turn.witnessed.groups[k].killed |=
            !turn.witnessed.groups[k].in_view && rec_thing_next(turn.witnessed.groups[k].at.thing) == THING_NONE;
}

void turn_start_witnessing(void)
{
    memset(&turn.witnessed, 0, sizeof turn.witnessed);
    turn.witnessed.champion_count = g_party_champion_count;
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        turn.witnessed.health[i] = g_champions[i].current_health;
    record_witnessed();
}

/* The latest expiry time of a message row: it grows when the game prints a message. */
static uint32_t latest_message_expiry(void)
{
    uint32_t latest = 0;
    for (int r = 0; r < MESSAGE_ROWS; r++)
        if (g_message.expiry[r] != MESSAGE_NO_EXPIRY && g_message.expiry[r] > latest)
            latest = g_message.expiry[r];
    return latest;
}

void turn_take_snapshot(void)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        turn.seen.health[i] = g_champions[i].current_health;
    turn.seen.group_count = state_get_visible_groups(turn.seen.groups);
    turn.seen.message_expiry = latest_message_expiry();
}

void turn_start(uint16_t min_ticks, uint16_t max_ticks, bool wait_only)
{
    turn.now.pending = true;
    turn.now.wait_only = wait_only;
    turn.now.stop = NULL;
    turn.now.start_time = g_game_time;
    turn.now.frames = 0;
    turn.now.min_ticks = min_ticks;
    turn.now.max_ticks = max_ticks;
}

/* Appends the map square, the view depth and the view side of a seen thing. */
static void seen_place_json(Json *j, const StateSeenThing *at)
{
    json_key(j, "x");
    json_int(j, at->x);
    json_key(j, "y");
    json_int(j, at->y);
    json_key(j, "depth");
    json_int(j, at->depth);
    json_key(j, "side");
    json_str(j, at->side);
}

/* The explosions the party saw and the groups that lost creatures in view or were killed; nothing when none. */
static void seen_json(Json *j)
{
    int changes = 0;
    for (int k = 0; k < turn.witnessed.group_count; k++)
        changes += turn.witnessed.groups[k].killed || turn.witnessed.groups[k].after < turn.witnessed.groups[k].before;
    if (!changes && !turn.witnessed.explosion_count)
        return;
    json_key(j, "seen");
    json_open(j, '{');
    json_key(j, "explosions");
    json_open(j, '[');
    for (int k = 0; k < turn.witnessed.explosion_count; k++) {
        json_open(j, '{');
        json_key(j, "name");
        state_write_explosion_name(j, turn.witnessed.explosions[k].type);
        seen_place_json(j, &turn.witnessed.explosions[k].at);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_key(j, "creature_changes");
    json_open(j, '[');
    for (int k = 0; k < turn.witnessed.group_count; k++) {
        const WitnessedGroup *group = &turn.witnessed.groups[k];
        if (!group->killed && group->after >= group->before)
            continue;
        json_open(j, '{');
        json_key(j, "type");
        json_str(j, state_get_creature_name(group->creature));
        seen_place_json(j, &group->at);
        json_key(j, "before");
        json_int(j, group->before);
        json_key(j, "after");
        json_int(j, group->killed ? 0 : group->after);
        json_close(j, '}');
    }
    json_close(j, ']');
    json_close(j, '}');
}

/* The health that each champion lost since the start of the request, only when one lost health. */
static void health_lost_json(Json *j)
{
    int any = 0;
    for (uint16_t i = 0; i < turn.witnessed.champion_count; i++)
        any |= g_champions[i].current_health < turn.witnessed.health[i];
    if (!any)
        return;
    json_key(j, "health_lost");
    json_open(j, '[');
    for (uint16_t i = 0; i < turn.witnessed.champion_count; i++) {
        int lost = turn.witnessed.health[i] - g_champions[i].current_health;
        json_int(j, lost > 0 ? lost : 0);
    }
    json_close(j, ']');
}

/* The eye panel text of a look action, only when the turn looked. */
static void look_json(Json *j)
{
    if (!turn.look.shown)
        return;
    json_key(j, "look");
    json_open(j, '{');
    json_key(j, "name");
    json_str(j, turn.look.name);
    json_key(j, "lines");
    json_open(j, '[');
    for (int16_t i = 0; i < turn.look.count; i++)
        json_str(j, turn.look.lines[i]);
    json_close(j, ']');
    json_close(j, '}');
}

/* The step of the turn left the party on its square: a wall, a closed door, or a creature is in the way. */
static bool is_bumped(void)
{
    return turn.now.moving && g_party_map_index == turn.now.map && g_party_map_x == turn.now.x &&
           g_party_map_y == turn.now.y && g_party_teleport_count == turn.now.teleports;
}

/* The party was turned or moved by something else than its action: a spinner, a teleporter, a pit, stairs. */
static const char *displaced_reason(void)
{
    int16_t x = turn.now.x;
    int16_t y = turn.now.y;
    if (turn.now.moving)
        map_step(turn.now.direction, turn.now.step_forward, turn.now.step_right, &x, &y);
    bool stayed = g_party_map_x == turn.now.x && g_party_map_y == turn.now.y;
    if (g_party_map_index != turn.now.map || (!stayed && (g_party_map_x != x || g_party_map_y != y)))
        return "moved";
    if (stayed && g_party_teleport_count != turn.now.teleports)
        return "moved"; /* a teleporter sent the party back */
    if (g_party_direction != turn.now.direction &&
        g_party_direction != NORMALIZE(turn.now.direction + turn.now.rotation))
        return "turned";
    return NULL;
}

/* The end of a turn: the ticks it took, the action damage, the state, and the view. */
static void write_result(Json *reply)
{
    json_open(reply, '{');
    json_key(reply, "ok");
    json_bool(reply, true);
    json_key(reply, "ticks");
    json_int(reply, g_game_time - (turn.batch.active ? turn.batch.start_time : turn.now.start_time));
    if (turn.now.wait_only) {
        json_key(reply, "stopped");
        json_str(reply, turn.now.stop ? turn.now.stop : "done");
    }
    if (!turn.now.wait_only && !turn.batch.active && !turn_is_game_over() && displaced_reason()) {
        json_key(reply, "stopped");
        json_str(reply, displaced_reason());
    }
    if (turn.batch.active) {
        json_key(reply, "ran");
        json_int(reply, turn.batch.ran);
        json_key(reply, "stopped");
        json_str(reply, turn.batch.stop);
        if (turn.batch.error) {
            json_key(reply, "error");
            json_str(reply, turn.batch.error);
        }
    }
    json_key(reply, "game_over");
    json_bool(reply, turn_is_game_over());
    if (is_bumped()) {
        json_key(reply, "bumped");
        json_bool(reply, true);
    }
    if (turn.batch.active && turn.batch.damage_count) {
        json_key(reply, "action_damage");
        json_open(reply, '[');
        for (int i = 0; i < turn.batch.damage_count; i++)
            json_int(reply, turn.batch.damages[i]);
        json_close(reply, ']');
    } else if (!turn.batch.active && turn.now.has_damage) {
        json_key(reply, "action_damage");
        json_int(reply, turn.now.damage);
    }
    health_lost_json(reply);
    look_json(reply);
    seen_json(reply);
    json_key(reply, "state");
    state_write_state(reply);
    json_key(reply, "view");
    state_write_view(reply);
    json_close(reply, '}');
}

/* Writes the result of the batch into reply and ends the batch. */
static void batch_finish(const char *stop, const char *error, Json *reply)
{
    turn.batch.stop = stop;
    turn.batch.error = error;
    write_result(reply);
    reset_batch();
}

/* Starts the next step of the batch; true when the batch is done or the step cannot run (reply holds the result). */
static bool batch_next(Json *reply)
{
    const char *error;
    char element[BATCH_ITEM_SIZE], name[TURN_ACTION_NAME_SIZE];
    turn_take_snapshot();
    RequestItem found = request_next_item(&turn.batch.next, element, sizeof element);
    if (found != REQUEST_ITEM_FOUND) {
        bool bad = found == REQUEST_ITEM_BAD;
        batch_finish(bad ? "failed" : "done", bad ? "an item of actions is not an action name or object" : NULL, reply);
        return true;
    }
    if (element[0] == '"')
        snprintf(name, sizeof name, "%.*s", (int)strlen(element) - 2, element + 1);
    else if (!request_get_string(element, "action", name, sizeof name))
        name[0] = 0;
    error = turn.batch.run(name, element);
    if (error) {
        batch_finish("failed", error, reply);
        return true;
    }
    turn_start(TURN_MIN_TICKS, TURN_MAX_TICKS, false);
    return false;
}

bool turn_start_batch(const char *list, TurnActionFn run, Json *reply)
{
    turn.batch.actions = strdup(list);
    if (!turn.batch.actions)
        abort();
    turn.batch.next = turn.batch.actions;
    turn.batch.run = run;
    turn.batch.active = true;
    turn.batch.damage_count = 0;
    turn.batch.start_time = g_game_time;
    turn.now.has_damage = false;
    turn.now.moving = false;
    return batch_next(reply);
}

/* A creature group is in view that was not in view at the snapshot. */
static bool new_group_in_view(void)
{
    uint16_t groups[STATE_VISIBLE_GROUPS_MAX];
    int count = state_get_visible_groups(groups);
    for (int i = 0; i < count; i++) {
        int known = 0;
        for (int k = 0; k < turn.seen.group_count; k++)
            known |= groups[i] == turn.seen.groups[k];
        if (!known)
            return true;
    }
    return false;
}

/* What happened since the snapshot that the player must see, or NULL. */
static const char *get_news(void)
{
    if (turn_is_game_over())
        return "game_over";
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        if (g_champions[i].current_health < turn.seen.health[i])
            return "damage";
    if (new_group_in_view())
        return "creature";
    if (latest_message_expiry() > turn.seen.message_expiry)
        return "message";
    return NULL;
}

/* A turn ends when the party can move again after the minimum ticks, or at the maximum. A wait
   ends early on news. A game that does not tick ends the turn after a frame limit. */
static bool is_finished(void)
{
    uint32_t ticks = g_game_time - turn.now.start_time;
    if (turn.now.wait_only && (turn.now.stop = get_news()))
        return true;
    if (turn_is_game_over())
        return true;
    if (ticks >= turn.now.max_ticks || turn.now.frames > (uint32_t)turn.now.max_ticks * FRAMES_PER_TICK_LIMIT)
        return true;
    return !turn.now.wait_only && ticks >= turn.now.min_ticks && !g_disabled_movement_ticks;
}

/* Why a batch stops after a turn, or NULL to go on. */
static const char *batch_stop_reason(void)
{
    if (turn_is_game_over())
        return get_news();
    if (is_bumped())
        return "bumped";                        /* before damage: a bump into a wall hurts */
    const char *displaced = displaced_reason(); /* before damage: a pit fall hurts */
    return displaced ? displaced : get_news();
}

/* After a turn of the batch: count it, stop on news, else start the next step; true when the batch ended. */
static bool batch_after_turn(Json *reply)
{
    turn.batch.ran++;
    const char *stop = batch_stop_reason();
    if (!stop)
        return batch_next(reply);
    batch_finish(stop, NULL, reply);
    return true;
}

bool turn_run_frame(Json *reply)
{
    record_witnessed();
    if (!is_finished()) {
        turn.now.frames++;
        gameloop_run_commands();
        g_stop_waiting_for_input = true; /* every frame of a turn is a tick */
        return false;
    }
    turn.now.pending = false;
    if (turn.client_gone) {
        reset_batch();
        turn.client_gone = false;
        return false;
    }
    if (turn.batch.active)
        return batch_after_turn(reply);
    write_result(reply);
    return true;
}
