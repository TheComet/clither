#pragma once

#include "clither/game/q.h"
#include "clither/platform/asm_optimizations.h"

CLITHER_ASM_OPTIMIZATION1(uint64_t, morton_encode_qwpos, struct qwpos)
CLITHER_ASM_OPTIMIZATION1(struct qwpos, morton_decode_qwpos, uint64_t)
