/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_TURN_H
#define DM_CONTROL_TURN_H
/* Prefixes: turn_ */

/* The turns of the program player: an action or a wait runs game ticks until the party can move again, a batch
   runs its actions as turns until news, and the reply tells what the party saw. No socket, no input. */

#include "ui/panel.h"
#include "control/json.h"
#include <stdbool.h>
#include <stdint.h>

/* Turn lengths in game ticks. */
#define TURN_MIN_TICKS      2 /* an action shows its result, then the action area returns */
#define TURN_MAX_TICKS      64
#define TURN_WAIT_MAX_TICKS 5000

/* Gives the input of action name with the args in request: NULL, or the reason why it cannot. */
typedef const char *(*TurnActionFn)(const char *name, const char *request);

/* Setup */
/* Forgets the turn, the batch and what the party saw. */
void turn_reset(void);
/* The client is gone: the pending turn ends without a reply and the batch stops. */
void turn_drop_client(void);

/* Queries */
/* True while a turn runs: the frame loop then runs game frames and ticks. */
bool turn_is_pending(void);
/* True when the party died or the game is won: no turn can follow until load or new_game. */
bool turn_is_game_over(void);

/* Actions */
/* Remembers the party place before an action and forgets the facts of the last action. */
void turn_begin_action(void);
/* The action is a step: forward and right squares from the party direction. */
void turn_set_step(int16_t forward, int16_t right);
/* The action is a turn of the party: 1 right, -1 left. */
void turn_set_rotation(int16_t rotation);
/* The damage of a chosen action; a batch keeps each one. */
void turn_add_damage(int16_t damage);
/* The eye panel text of a look action. */
void turn_set_look(const char *name, const char lines[][PANEL_LINE_SIZE], int16_t count);

/* Turns */
/* Starts the record of what the party sees during the request. */
void turn_start_witnessing(void);
/* Remembers what the news of a wait or a batch compares. */
void turn_take_snapshot(void);
/* Starts a turn of min to max ticks; a wait ends early on news. */
void turn_start(uint16_t min_ticks, uint16_t max_ticks, bool wait_only);
/* Starts a batch over the list body of an act_many request; true when reply already holds the answer. */
bool turn_start_batch(const char *list, TurnActionFn run, Json *reply);
/* One frame of the pending turn: true when the request ended and reply holds its result. */
bool turn_run_frame(Json *reply);

#endif
