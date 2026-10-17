/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_CONTROL_INTERNAL_H
#define DM_CONTROL_CONTROL_INTERNAL_H
/* Prefixes: control_ */

/* Shared declarations of the control module: the action handlers of control_actions.c for the dispatch of control.c. */

/* Gives the input of action name with the args in request: NULL, or the reason why it cannot. */
const char *control_run_action(const char *name, const char *request);

#endif
