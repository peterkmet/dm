/* Windows has no control socket: the turn-based LLM mode cannot start there. control.c is the real module. */
#ifdef _WIN32
#include "control.h"

int control_open(const char *path)
{
    (void)path;
    return 0;
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
#endif
