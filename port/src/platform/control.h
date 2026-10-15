#ifndef CONTROL_H
#define CONTROL_H
/* Prefixes: control_ */
#include <stdbool.h>

/* Turn-based control over a Unix domain socket: one JSON request per line, one JSON response per
   line (docs/llm-mode.md). Plain POSIX, no SDL. */

/* True where the control socket exists (macOS); false on Windows, which has no turn-based mode. */
bool control_is_available(void);
/* Listens on the socket path (an old socket file is replaced). Returns 1 on success. */
int control_open(const char *path);
/* Call once per frame: accepts a client, reads requests, answers them, and ends a finished turn. */
void control_poll(void);
/* True while a turn runs: the main loop then runs game frames and ticks; else the game time stands still. */
bool control_is_turn_pending(void);
/* Closes the sockets and removes the socket file. */
void control_close(void);

#endif
