#!/bin/sh
valgrind --num-callers=50 \
	--leak-resolution=high \
	--leak-check=full \
	--track-origins=yes \
	--time-stamp=yes \
	./mechasnek --server -- 2>&1 | tee server.grind
