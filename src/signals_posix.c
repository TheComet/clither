#include "clither/signals.h"
#include <signal.h>
#include <stddef.h>

static volatile char ctrl_c_pressed = 0;
static struct sigaction old_act;

/* ------------------------------------------------------------------------- */
static void handle_ctrl_c(int sig)
{
    (void)sig;
    ctrl_c_pressed = 1;
}

/* ------------------------------------------------------------------------- */
void
signals_install(void)
{
    struct sigaction act;
    act.sa_handler = handle_ctrl_c;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, &old_act);

    ctrl_c_pressed = 0;
}

/* ------------------------------------------------------------------------- */
void
signals_remove(void)
{
    sigaction(SIGINT, &old_act, NULL);
}

/* ------------------------------------------------------------------------- */
char
signals_exit_requested(void)
{
    return ctrl_c_pressed;
}
