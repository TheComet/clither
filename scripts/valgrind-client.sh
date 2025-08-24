#!/bin/sh
valgrind --num-callers=50 \
	--leak-resolution=high \
	--leak-check=full \
	--track-origins=yes \
	--time-stamp=yes \
	./mechasnek -- 2>&1 | tee client.grind
