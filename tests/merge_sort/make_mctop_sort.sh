#!/bin/bash

un=$(uname -n);
outf=./bin/${un};
mkdir -p $outf;

MAKE=make;
if [ ${un} = "ol-collab1" ];
then
    MAKE=gmake;
fi;

${MAKE} clean ; SSE=0 SSE_HYPERTHREAD_RATIO=3 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_no_sse

${MAKE} clean ; SSE=1 SSE_HYPERTHREAD_RATIO=3 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_sse

${MAKE} clean ; SSE=2 SSE_HYPERTHREAD_RATIO=1 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_sse_hyperthreads_1

${MAKE} clean ; SSE=2 SSE_HYPERTHREAD_RATIO=2 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_sse_hyperthreads_2

${MAKE} clean ; SSE=2 SSE_HYPERTHREAD_RATIO=3 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_sse_hyperthreads_3

${MAKE} clean ; SSE=2 SSE_HYPERTHREAD_RATIO=4 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_sse_hyperthreads_4

${MAKE} clean ; SSE=3 SSE_HYPERTHREAD_RATIO=3 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_no_sse_all_sockets

${MAKE} clean ; SSE=4 SSE_HYPERTHREAD_RATIO=3 ${MAKE} -kj mctop_sort >> /dev/null;
mv ./mctop_sort ${outf}/mctop_sort_no_sse_no_smt_all_sockets

${MAKE} clean ; ${MAKE} -kj merge_sort_std_parallel
mv merge_sort_std_parallel ${outf}/merge_sort_std_parallel

${MAKE} clean ; ${MAKE} -kj merge_sort_tbb_parallel
mv merge_sort_tbb_parallel ${outf}/merge_sort_tbb_parallel
