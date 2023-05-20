#include "benchmark/benchmark.h"
#include "clither/benchmarks.h"
#include "cstructures/memory.h"

using namespace benchmark;

int benchmarks_run(int argc, char** argv) {
    Initialize(&argc, argv);
    if (ReportUnrecognizedArguments(argc, argv))
        return 1;

    memory_init_thread();
    RunSpecifiedBenchmarks();
    memory_deinit_thread();

    return 0;
}

