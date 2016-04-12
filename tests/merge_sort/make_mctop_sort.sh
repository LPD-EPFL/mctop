#!/bin/bash


make clean && SSE=0 SSE_HYPERTHREAD_RATIO=3 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_no_sse

make clean && SSE=1 SSE_HYPERTHREAD_RATIO=3 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_sse

make clean && SSE=2 SSE_HYPERTHREAD_RATIO=1 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_sse_hyperthreads_1

make clean && SSE=2 SSE_HYPERTHREAD_RATIO=2 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_sse_hyperthreads_2

make clean && SSE=2 SSE_HYPERTHREAD_RATIO=3 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_sse_hyperthreads_3

make clean && SSE=2 SSE_HYPERTHREAD_RATIO=4 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_sse_hyperthreads_4

make clean && SSE=3 SSE_HYPERTHREAD_RATIO=3 make -kj mctop_sort
mv ./mctop_sort ./mctop_sort_no_sse_all_sockets

make clean && make -kj merge_sort_std_parallel

make clean && make -kj merge_sort_tbb_parallel

