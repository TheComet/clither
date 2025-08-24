#include "benchmark/benchmark.h"

extern "C" {
#include "clither/config.h"
#include "clither/util/morton.h"
}

using namespace benchmark;

#if defined(CLITHER_ASM_OPTIMIZATIONS)
static void bm_morton_qwpos_generic(State& state)
{
    int32_t      x = state.range(0);
    int32_t      y = state.range(1);
    struct qwpos pos = {x, y};

    for (auto _ : state)
    {
        morton       m = morton_encode_qwpos_generic(pos);
        struct qwpos p2 = morton_decode_qwpos_generic(m);
        DoNotOptimize(p2);
    }
}
BENCHMARK(bm_morton_qwpos_generic)->Args({26, 26});

static void bm_morton_qwpos_asm(State& state)
{
    int32_t      x = state.range(0);
    int32_t      y = state.range(1);
    struct qwpos pos = {x, y};

    for (auto _ : state)
    {
        morton       m = morton_encode_qwpos_asm(pos);
        struct qwpos p2 = morton_decode_qwpos_asm(m);
        DoNotOptimize(p2);
    }
}
BENCHMARK(bm_morton_qwpos_asm)->Args({26, 26});
#endif
