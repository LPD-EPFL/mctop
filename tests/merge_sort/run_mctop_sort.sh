#!/bin/bash


VERSIONS="merge_sort_std_parallel merge_sort_tbb_parallel mctop_sort_no_sse mctop_sort_sse mctop_sort_sse_hyperthreads_1 mctop_sort_sse_hyperthreads_2 mctop_sort_sse_hyperthreads_3 mctop_sort_sse_hyperthreads_4 mctop_sort_no_sse_all_sockets"

SIZES="1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192"
ALLOCATORS="3 4 7 9"


NAME=$(uname -n)

if [[ $NAME == "lpdxeon2680" ]]
then
  THREADS="10 20 30 40" 
elif [[ $NAME == "lpd48core" ]]
then
  THREADS="6 12 24 48"
  VERSIONS="merge_sort_std_parallel merge_sort_tbb_parallel mctop_sort_no_sse mctop_sort_no_sse_all_sockets"
elif [[ $NAME == "lpdquad" ]]
then
  THREADS="12 24 36 48 60 72 84 96"
elif [[ $NAME  == "ol-collab1" ]]
then
  VERSIONS="merge_sort_std_parallel merge_sort_tbb_parallel mctop_sort_no_sse mctop_sort_no_sse_all_sockets"
  THREADS=$(seq 32 32 256)
fi

for size in $SIZES
do
  printf "\n\n ======================================================================================================== \n"
  printf " =============================================== "
  printf "%5d MB" ${size}
  printf " =============================================== \n"
  printf " ======================================================================================================== \n"

  printf "%29s:" "threads"
  for thread in $THREADS
  do
    printf " %8d" ${thread}
  done
  printf "\n"
  for allocator in $ALLOCATORS
  do
    printf " --------------------------------------------- "
    printf "ALLOCATOR: %d" ${allocator}
    printf " --------------------------------------------- "
    printf "\n"
    for version in $VERSIONS
    do
      printf "%29s:" ${version}
      for thread in $THREADS
      do
        res=$(./${version} -n${thread} -s${size} -p${allocator} 2>&1 | grep Sorted | awk '{printf"%.3f", $9;}')
        if [[ $res ]]
        then
          printf " %8s" ${res}
        else
          printf " %8s" "n/a"
        fi
      done
      printf "\n"
    done
  done
done


