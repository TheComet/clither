#include "clither/platform/asm_optimizations.h"
#include "clither/platform/system.h"
#include "clither/util/log.h"
#include "clither/util/morton.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ------------------------------------------------------------------------- */
static int trampoline_hotpatch(void* trampoline, void* target, int page_size)
{
    DWORD          flOldProtect;
    unsigned char* code = (unsigned char*)trampoline;
    int32_t        rel_off = (intptr_t)target - ((intptr_t)trampoline + 5);

    uintptr_t page_start = (uintptr_t)trampoline & ~(page_size - 1);
    if (VirtualProtect(
            (void*)page_start,
            page_size,
            PAGE_EXECUTE_READWRITE,
            &flOldProtect) == 0)
    {
        return log_err_win32(
            "VirtualProtect() failed: Cannot patch trampoline\n");
    }

    code[0] = 0xE9; /* JMP rel32 */
    memcpy(code + 1, &rel_off, sizeof(rel_off));

    if (VirtualProtect(
            (void*)page_start, page_size, flOldProtect, &flOldProtect) == 0)
    {
        return log_err_win32(
            "VirtualProtect() failed, but trampoline patch succeeded.\n");
    }

    FlushInstructionCache(GetCurrentProcess(), (char*)trampoline, 5);
    return 0;
}

/* ------------------------------------------------------------------------- */
int asm_optimizations_init(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
#if defined(CLITHER_ASM_OPTIMIZATIONS_HOTPATCH)
#    define X(cpuid_feature, func_name)                                        \
        if (trampoline_hotpatch(                                               \
                (void*)func_name,                                              \
                system_cpuid_##cpuid_feature() ? (void*)func_name##_asm        \
                                               : (void*)func_name##_generic,   \
                si.dwPageSize) != 0)                                           \
        {                                                                      \
            return -1;                                                         \
        }
    CLITHER_ASM_OPTIMIZATIONS_LIST
#    undef X
#elif defined(CLITHER_ASM_OPTIMIZATIONS_FUNCPTR)
#    define X(cpuid_feature, func_name)                                        \
        func_name = system_cpuid_##cpuid_feature() ? func_name##_asm           \
                                                   : func_name##_generic;
    CLITHER_ASM_OPTIMIZATIONS_LIST
#    undef X
#elif defined(CLITHER_ASM_OPTIMIZATIONS_HARDCODE)
#else
#endif
    return 0;
}
