#!/bin/sh
cd build/bin || echo "Error: Please run this script from the project's root directory as ./scripts/valgrind-host.sh"

echo "Started valgrind..."
valgrind --num-callers=50 \
	--leak-resolution=high \
	--leak-check=full \
	--track-origins=yes \
	--time-stamp=yes \
	./clither -- --host 2>&1 | tee ../../clither.grind
cd .. && cd ..
