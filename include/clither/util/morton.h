#pragma once

#include "clither/game/q.h"

uint64_t morton_encode_qwpos(struct qwpos p);
struct qwpos morton_decode_qwpos(uint64_t m);
