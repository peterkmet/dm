/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The build without a control socket (Windows): the turn-based mode cannot start. */
#include "transport.h"

bool transport_is_available(void)
{
    return false;
}

bool transport_open(const char *path)
{
    (void)path;
    return false;
}

void transport_close(void)
{
}

void transport_poll(void)
{
}
