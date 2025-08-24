#include "benchmark/benchmark.h"

extern "C" {
#include "clither/benchmarks.h"
}

using namespace benchmark;

int benchmarks_run(int argc, char** argv)
{
    Initialize(&argc, argv);
    /*if (ReportUnrecognizedArguments(argc, argv))
        return -1;*/

    RunSpecifiedBenchmarks();

    return 0;
}
