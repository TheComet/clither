#include "clither/platform/asm_optimizations.h"
#include "clither/platform/system.h"
#include "clither/util/log.h"
#include "clither/util/morton.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int trampoline_hotpatch(void* trampoline, void* target)
{
    unsigned char* code = (unsigned char*)trampoline;
    int32_t        rel_off = (intptr_t)target - ((intptr_t)trampoline + 5);

    uintptr_t page_start = (uintptr_t)trampoline & ~(getpagesize() - 1);
    if (mprotect(
            (void*)page_start,
            getpagesize(),
            PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
    {
        return log_err(
            "mprotect() failed: Cannot patch trampoline: %s\n",
            strerror(errno));
    }

    code[0] = 0xE9; /* JMP rel32 */
    memcpy(code + 1, &rel_off, sizeof(rel_off));

    if (mprotect((void*)page_start, getpagesize(), PROT_READ | PROT_EXEC) != 0)
    {
        return log_err(
            "mprotect() failed, but trampoline patch succeeded: %s\n",
            strerror(errno));
    }

    __builtin___clear_cache((char*)trampoline, (char*)trampoline + 5);
    return 0;
}

void asm_optimizations_init(void)
{
#if defined(CLITHER_ASM_OPTIMIZATIONS_HOTPATCH)
/* casting function to void* */
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"

#    define X(cpuid_feature, func_name)                                        \
        trampoline_hotpatch(                                                   \
            (void*)func_name,                                                  \
            system_cpuid_##cpuid_feature() ? (void*)func_name##_asm            \
                                           : (void*)func_name##_generic);
    CLITHER_ASM_OPTIMIZATIONS_LIST
#    undef X

#    pragma GCC diagnostic pop
#elif defined(CLITHER_ASM_OPTIMIZATIONS_FUNCPTR)
#    define X(cpuid_feature, func_name)                                        \
        func_name = system_cpuid_##cpuid_feature() ? func_name##_asm           \
                                                   : func_name##_generic;
    CLITHER_ASM_OPTIMIZATIONS_LIST
#    undef X
#elif defined(CLITHER_ASM_OPTIMIZATIONS_HARDCODE)
#else
#endif
}
