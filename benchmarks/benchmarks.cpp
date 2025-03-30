#include "benchmark/benchmark.h"
#include "clither/benchmarks.h"
#include "clither/mem.h"

using namespace benchmark;

int benchmarks_run(int argc, char** argv)
{
    Initialize(&argc, argv);
    if (ReportUnrecognizedArguments(argc, argv))
        return 1;

    RunSpecifiedBenchmarks();

    return 0;
}
