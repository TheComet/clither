#pragma once

#include "clither/game/q.h"
#include "clither/platform/asm_optimizations.h"

typedef uint64_t morton;

CLITHER_ASM_OPTIMIZATION1(morton, morton_encode_qwpos, struct qwpos)
CLITHER_ASM_OPTIMIZATION1(struct qwpos, morton_decode_qwpos, morton)
