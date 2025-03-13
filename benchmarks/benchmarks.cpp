#include "benchmark/benchmark.h"
#include "clither/benchmarks.h"
#include "clither/mem.h"

using namespace benchmark;

int benchmarks_run(int argc, char** argv)
{
    Initialize(&argc, argv);
    if (ReportUnrecognizedArguments(argc, argv))
        return 1;

    mem_init_threadlocal();
    RunSpecifiedBenchmarks();
    mem_deinit_threadlocal();

    return 0;
}
