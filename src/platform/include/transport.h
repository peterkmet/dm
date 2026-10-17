/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_PLATFORM_TRANSPORT_H
#define DM_PLATFORM_TRANSPORT_H
/* Prefixes: transport_ */

/* The control socket: a Unix domain socket with one request line in and one reply line out (POSIX).
   transport_none.c is the build without a socket (Windows). The protocol is control/control.h. */

#include <stdbool.h>

/* Setup */
/* True where the control socket exists (macOS); false on Windows, which has no turn-based mode. */
bool transport_is_available(void);
/* Listens on the socket path (an old socket file is replaced). */
bool transport_open(const char *path);
/* Closes the sockets and removes the socket file. */
void transport_close(void);

/* Commands */
/* One frame: advances a pending turn and sends its answer, then accepts a client and answers its request lines. */
void transport_poll(void);

#endif
