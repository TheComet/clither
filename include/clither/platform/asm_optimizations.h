#pragma once

#include "clither/config.h"

#define CLITHER_ASM_OPTIMIZATIONS_LIST                                         \
    X(bmi2, morton_encode_qwpos)                                               \
    X(bmi2, morton_decode_qwpos)

/* Generate trampolines that are patched with a jmp rel32 instruction to jump to
 * the real function depending on CPU features */
#if defined(CLITHER_ASM_OPTIMIZATIONS_HOTPATCH)
#    define CLITHER_ASM_OPTIMIZATION1(ret, name, arg1)                         \
        CLITHER_HOTPATCH ret name(arg1);                                       \
        ret                  name##_asm(arg1);                                 \
        ret                  name##_generic(arg1);

/* Use a function pointer, which is initialized to point to the real function
 * depending on CPU features */
#elif defined(CLITHER_ASM_OPTIMIZATIONS_FUNCPTR)
#    define CLITHER_ASM_OPTIMIZATION1(ret, name, arg1)                         \
        extern ret (*name)(arg1);                                              \
        ret name##_asm(arg1);                                                  \
        ret name##_generic(arg1);

/* Call the asm routine directly */
#elif defined(CLITHER_ASM_OPTIMIZATIONS_HARDCODE)
#    define CLITHER_ASM_OPTIMIZATION1(ret, name, arg1)                         \
        static ret name(arg1 a1)                                               \
        {                                                                      \
            ret name##_asm(arg1);                                              \
            return name##_asm(a1);                                             \
        }

/* No optimizations, call the generic implementation */
#else
#    define CLITHER_ASM_OPTIMIZATION1(ret, name, arg1)                         \
        static ret name(arg1 a1)                                               \
        {                                                                      \
            ret name##_generic(arg1);                                          \
            return name##_generic(a1);                                         \
        }
#endif
