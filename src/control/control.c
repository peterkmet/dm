/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "control/control.h"
#include "data/file.h"
#include "engine/dungeon.h"
#include "engine/savegame.h"
#include "ui/gameloop.h"
#include "ui/startend.h"
#include "control/control_internal.h"
#include "control/png.h"
#include "control/request.h"
#include "control/state_json.h"
#include "control/turn.h"
#include <string.h>

/* Longest request command name, with its terminator. */
#define CMD_SIZE 32

static bool load_failed; /* a damaged save file left no game to play */

/* Replies. */

/* Writes a failed reply with the reason. */
static void write_error(Json *reply, const char *error)
{
    json_open(reply, '{');
    json_key(reply, "ok");
    json_bool(reply, false);
    json_key(reply, "error");
    json_str(reply, error);
    json_close(reply, '}');
}

/* Writes a good reply with one part of the state under key. */
static void write_part(Json *reply, const char *key, void (*part)(Json *))
{
    json_open(reply, '{');
    json_key(reply, "ok");
    json_bool(reply, true);
    json_key(reply, key);
    part(reply);
    json_close(reply, '}');
}

/* Requests. */

/* Gives the input of one action and starts its turn; a refused action is answered at once. */
static void handle_act(const char *request, Json *reply)
{
    char name[TURN_ACTION_NAME_SIZE];
    if (!request_get_string(request, "action", name, sizeof name)) {
        write_error(reply, "missing action");
        return;
    }
    turn_start_witnessing();
    const char *error = control_run_action(name, request);
    if (error)
        write_error(reply, error);
    else
        turn_start(TURN_MIN_TICKS, TURN_MAX_TICKS, false);
}

/* Starts a batch over the actions list of the request. */
static void handle_act_many(const char *request, Json *reply)
{
    const char *list = request_find_value(request, "actions");
    if (!list || *list != '[') {
        write_error(reply, "actions must be a list");
        return;
    }
    turn_start_witnessing();
    turn_start_batch(list + 1, control_run_action, reply);
}

/* Loads the save file of the adventure (DMGAME.DAT or CSBGAME.DAT) from the save directory, as RESUME does, and answers with the state. */
static void handle_load(Json *reply)
{
    SavegameResult result = savegame_load(startend_save_dir());
    if (result == SAVEGAME_MISSING) {
        write_error(reply, "no saved game (use save first)");
        return;
    }
    load_failed = result == SAVEGAME_DAMAGED;
    if (load_failed) {
        write_error(reply, "the saved game is damaged: use new_game");
        return;
    }
    gameloop_start_loaded_game();
    write_part(reply, "state", state_write_state);
}

/* Starts a new game on the start square, as ENTER at the entrance does, and answers with the state. */
static void handle_new_game(Json *reply)
{
    if (!gameloop_start_new_game(gameloop_get_data_dir())) {
        write_error(reply, "cannot start the new game");
        return;
    }
    load_failed = false;
    write_part(reply, "state", state_write_state);
}

/* Writes the save file of the adventure, as the SAVE button does. */
static void handle_save(Json *reply)
{
    const char *reason;
    int saved = startend_save_current_game(&reason);
    if (!saved)
        write_error(reply, reason ? reason : "cannot write the save file");
    else {
        if (saved == SAVED_NEW_ADVENTURE)
            gameloop_start_loaded_game();
        json_raw(reply, "{\"ok\":true}");
    }
}

/* Starts a turn of the given ticks without an action; it ends early on news. */
static void handle_wait(const char *request, Json *reply)
{
    int64_t ticks = request_get_int(request, "ticks", 1);
    if (ticks < 1 || ticks > TURN_WAIT_MAX_TICKS)
        write_error(reply, "ticks must be 1-5000");
    else {
        turn_begin_action();
        turn_take_snapshot();
        turn_start_witnessing();
        turn_start((uint16_t)ticks, (uint16_t)ticks, true);
    }
}

/* Writes the screen to the PNG file of the request path. */
static void handle_screenshot(const char *request, Json *reply)
{
    char path[PATH_SIZE];
    if (!request_get_string(request, "path", path, sizeof path))
        write_error(reply, "missing path");
    else if (!png_write(path))
        write_error(reply, "cannot write the PNG file");
    else {
        json_raw(reply, "{\"ok\":true,\"path\":");
        json_str(reply, path);
        json_close(reply, '}');
    }
}

/* Answers one request by its cmd; a game must run for every cmd but load and new_game. */
static void handle_request(const char *request, Json *reply)
{
    char cmd[CMD_SIZE];
    if (!request_get_string(request, "cmd", cmd, sizeof cmd)) {
        write_error(reply, "missing cmd");
        return;
    }
    if (!strcmp(cmd, "load")) {
        handle_load(reply);
        return;
    }
    if (!strcmp(cmd, "new_game")) {
        handle_new_game(reply);
        return;
    }
    if (!g_dungeon_maps || load_failed) {
        write_error(reply, "no game is running: use load or new_game");
        return;
    }
    if (!strcmp(cmd, "get_state")) {
        write_part(reply, "state", state_write_state);
    } else if (!strcmp(cmd, "get_view")) {
        write_part(reply, "view", state_write_view);
    } else if (!strcmp(cmd, "get_map")) {
        write_part(reply, "map", state_write_map);
    } else if (turn_is_game_over() &&
               (!strcmp(cmd, "act") || !strcmp(cmd, "act_many") || !strcmp(cmd, "wait") || !strcmp(cmd, "save"))) {
        write_error(reply, "game over");
    } else if (!strcmp(cmd, "save")) {
        handle_save(reply);
    } else if (!strcmp(cmd, "act")) {
        handle_act(request, reply);
    } else if (!strcmp(cmd, "act_many")) {
        handle_act_many(request, reply);
    } else if (!strcmp(cmd, "wait")) {
        handle_wait(request, reply);
    } else if (!strcmp(cmd, "screenshot")) {
        handle_screenshot(request, reply);
    } else {
        write_error(reply, "unknown cmd");
    }
}

/* Setup */

void control_drop_client(void)
{
    turn_drop_client();
}

/* Queries */

bool control_is_turn_pending(void)
{
    return turn_is_pending();
}

/* Commands */

bool control_handle_line(const char *line, Json *reply)
{
    handle_request(line, reply);
    return reply->s != NULL;
}

bool control_end_turn(Json *reply)
{
    return turn_run_frame(reply);
}
