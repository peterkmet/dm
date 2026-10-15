/* Windows has no control socket: the turn-based LLM mode cannot start there. control.c is the real module. */
#include "control.h"

bool control_is_available(void)
{
    return false;
}

bool control_open(const char *path)
{
    (void)path;
    return false;
}

void control_poll(void)
{
}
bool control_is_turn_pending(void)
{
    return false;
}
void control_close(void)
{
}
