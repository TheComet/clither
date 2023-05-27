#include "clither/mcd_wifi.h"
#include "clither/log.h"
#include "clither/cli_colors.h"
#include "clither/signals.h"
#include "clither/net.h"
#include "clither/tick.h"

#include "cstructures/memory.h"

/* ------------------------------------------------------------------------- */
void*
run_mcd_wifi(void* args)
{
    struct tick tick;

    /* Change log prefix and color for server log messages */
    log_set_prefix("McD WiFi: ");
    log_set_colors(COL_B_MAGENTA, COL_RESET);

    memory_init_thread();

    tick_cfg(&tick, 20);

    log_info("Hosting McDonald's WiFi on %s:%s\n", "", "");
    while (signals_exit_requested() == 0)
    {
        if (tick_wait(&tick) > 20 * 3)  /* 3 seconds */
            tick_skip(&tick);
    }
    log_info("Stopping McDonald's WiFi\n");

    memory_deinit_thread();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;
}
