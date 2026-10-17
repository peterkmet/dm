/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_CONTROL_H
#define DM_CONTROL_CONTROL_H
/* Prefixes: control_ */

/* The turn-based protocol of a program player: one JSON request line in, one JSON reply out
   (docs/llm/llm-mode.md). A client of ui/ and engine/ like a player at the keyboard; no socket, no SDL. */

#include "control/json.h"
#include <stdbool.h>

/* Setup */
/* The client is gone: a pending turn ends without a reply and a batch stops. */
void control_drop_client(void);

/* Queries */
/* True while a turn runs: the frame loop then runs game frames and ticks; else the game time stands still. */
bool control_is_turn_pending(void);

/* Commands */
/* Answers one request line: true when reply holds the answer, false when a turn started (control_end_turn answers). */
bool control_handle_line(const char *line, Json *reply);
/* Advances the pending turn by one frame: true when the request ended and reply holds the answer. */
bool control_end_turn(Json *reply);

#endif
