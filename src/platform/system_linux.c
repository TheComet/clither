#include "clither/platform/system.h"
#include <unistd.h>

int system_cpu_count(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

int system_cpuid_bmi2(void)
{
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(7), "c"(0));
    return (ebx >> 8) & 1;
}
