#include "cstructures/backtrace.h"
#include "cstructures/memory.h"
#include "cstructures/init.h"

/* ------------------------------------------------------------------------- */
CSTRUCTURES_PUBLIC_API int
cs_init(void)
{
    if (backtrace_init() < 0)
        goto backtrace_init_failed;
    if (cs_threadlocal_init() < 0)
        goto threadlocal_init_failed;

    return 0;

    threadlocal_init_failed : backtrace_deinit();
    backtrace_init_failed   : return -1;
}

/* ------------------------------------------------------------------------- */
CSTRUCTURES_PUBLIC_API void
cs_deinit(void)
{
    backtrace_deinit();
    cs_threadlocal_deinit();
}

/* ------------------------------------------------------------------------- */
CSTRUCTURES_PUBLIC_API int
cs_threadlocal_init(void)
{
    return memory_threadlocal_init();
}

/* ------------------------------------------------------------------------- */
CSTRUCTURES_PUBLIC_API void
cs_threadlocal_deinit(void)
{
    memory_threadlocal_deinit();
}
