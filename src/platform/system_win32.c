#include "clither/platform/system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

int
system_cpu_count(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}

int system_cpuid_bmi2(void)
{
    int cpu_info[4];
    __cpuidex(cpu_info, 7, 0); /* EAX=7, ECX=0 */

    /* BMI2 is bit 8 of EBX */
    return (cpu_info[1] >> 8) & 1;
}
