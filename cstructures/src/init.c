#include "cstructures/backtrace.h"
#include "cstructures/memory.h"
#include "cstructures/init.h"

/* ------------------------------------------------------------------------- */
CSTRUCTURES_PUBLIC_API int
cs_init(void)
{
    if (cs_threadlocal_init() < 0)
        return -1;
    if (backtrace_init() < 0)
    {
        cs_threadlocal_deinit();
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
CSTRUCTURES_PUBLIC_API void
cs_deinit(void)
{
    backtrace_deinit();
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
