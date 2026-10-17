/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform/app.h"
#include "platform/options.h"
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    Options options;
    OptionsResult result = options_parse(argc, argv, &options);
    if (result == OPTIONS_HELP) {
        options_print_help(stdout);
        return 0;
    }
    if (result == OPTIONS_ERROR || !app_open(&options))
        return 1;
    for (int64_t frame = 0; options.frames < 0 || frame < options.frames; frame++) {
        AppFrame step = app_run_frame();
        if (step == APP_FRAME_FAILED)
            return 1;
        if (step == APP_FRAME_QUIT)
            break;
    }
    app_close();
    return 0;
}
