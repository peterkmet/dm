#ifndef STATE_JSON_H
#define STATE_JSON_H
#include <stddef.h>
#include <stdint.h>

/* The game as JSON for the turn-based mode: what a player sees, plus the explored map
   (docs/llm-mode.md). No SDL. */

/* A growing JSON text. Start with {0}; free s when done. */
typedef struct {
    char *s;
    size_t len, cap;
} Json;

/* Writer: a value or key gets its comma before it when it is not the first in its object or array. */
void json_raw(Json *j, const char *text);
void json_key(Json *j, const char *key);
void json_int(Json *j, long value);
void json_bool(Json *j, int value);
void json_null(Json *j);
void json_str(Json *j, const char *text);
void json_open(Json *j, char bracket);         /* '{' or '[' */
void json_close(Json *j, char bracket);        /* '}' or ']' */

/* Each appends one JSON object. */
void state_json_state(Json *j);   /* party, champions, messages, actions, runes, panels */
void state_json_view(Json *j);    /* the squares in front of the party, 4 deep and the sides */
void state_json_map(Json *j);     /* the explored squares of the party map as text rows */

/* The creature groups on the squares the party sees now. */
int state_json_visible_groups(void);
/* The shortest walk on the party map to (x, y) over explored squares the party can pass, as directions
   (north, east, south, west), at most max. Returns the length (0 at the goal), or -1 without a path. */
int state_json_path(int16_t x, int16_t y, int16_t *directions, int max);

enum { STATE_JSON_STATE, STATE_JSON_VIEW, STATE_JSON_MAP };
/* One part as a new string (tests); release it with free. */
char *state_json_string(int part);

#endif
