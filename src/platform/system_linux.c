#include "clither/platform/system.h"
#include <unistd.h>
#include <cpuid.h>

int system_cpu_count(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

int system_cpuid_bmi2(void)
{
    unsigned int eax, ebx, ecx, edx;
    __cpuid_count(7, 0, eax, ebx, ecx, edx);  /* cpuid with eax=7, ecx=0 */
    return (ebx >> 8) & 1;  /* BMI2 is bit 8 of EBX */
}
