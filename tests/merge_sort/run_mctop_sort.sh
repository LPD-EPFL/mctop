#!/bin/bash


VERSIONS="merge_sort_std_parallel merge_sort_tbb_parallel mctop_sort_no_sse mctop_sort_sse mctop_sort_sse_hyperthreads_1 mctop_sort_sse_hyperthreads_2 mctop_sort_sse_hyperthreads_3 mctop_sort_sse_hyperthreads_4 mctop_sort_no_sse_all_sockets mctop_sort_no_sse_no_smt_all_sockets";

SIZES="1 2 4 8 16 32 64 128 256 512 1024 2048"
ALLOCATORS="4 6 9"


UNAME=$(uname -n)
binf=bin/${UNAME};

if [[ $UNAME == "lpdxeon2680" ]]
then
  THREADS="10 20 30 40" 
elif [[ $UNAME == "lpd48core" ]]
then
  THREADS="6 12 24 48"
  VERSIONS="merge_sort_std_parallel merge_sort_tbb_parallel mctop_sort_no_sse mctop_sort_no_sse_no_smt_all_sockets"
elif [[ $UNAME == "lpdquad" ]]
then
  THREADS="12 24 36 48 60 72 84 96"
elif [[ $UNAME  == "diassrv8" ]]
then
    VERSIONS="merge_sort_std_parallel mctop_sort_no_sse mctop_sort_sse mctop_sort_sse_hyperthreads_3 mctop_sort_no_sse_all_sockets mctop_sort_no_sse_no_smt_all_sockets"
  THREADS=$(seq 20 20 160)
elif [[ $UNAME  == "ol-collab1" ]]
then
  VERSIONS="merge_sort_std_parallel mctop_sort_no_sse mctop_sort_no_sse_all_sockets mctop_sort_no_sse_no_smt_all_sockets"
  THREADS=$(seq 32 32 256)
fi

for size in $SIZES
do
  printf "\n\n ======================================================================================================== \n"
  printf " =============================================== "
  printf "%5d MB" ${size}
  printf " =============================================== \n"
  printf " ======================================================================================================== \n"

  printf "%36s :" "threads"
  for thread in $THREADS
  do
    printf " %-9d" ${thread}
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
      printf "%36s :" ${version}
      for thread in $THREADS
      do
          text=$(./${binf}/${version} -n${thread} -s${size} -p${allocator} 2>&1);
	  res=$(echo "$text" | awk '/Sorted/ {printf"%.3f", $9;}');
	  error=$(echo "$text" | awk '/is sorted/ {printf"%d", $9;}');
        if [[ $res ]]
        then
          printf " %8s" ${res}
        else
          printf " %8s" "n/a"
        fi
        if [[ $error ]]
        then
          printf "*"
        else
          printf " "
        fi
      done
      printf "\n"
    done
  done
done


