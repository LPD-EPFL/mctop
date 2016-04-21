#!/bin/bash

REPS=11

SIZE=1024
ALLOCATOR=9


UNAME=$(uname -n)
binf=bin/${UNAME};

if [[ $UNAME == "lpdxeon2680" ]]
then
  VERSIONS=("merge_sort_std_parallel" "mctop_sort_sse" "mctop_sort_no_sse_all_sockets");
  THREADS=("16" "16" "16");
elif [[ $UNAME == "lpd48core" ]]
then
  THREADS=("16" "16");
  VERSIONS=("merge_sort_std_parallel" "mctop_sort_no_sse_no_smt_all_sockets");
elif [[ $UNAME == "lpdquad" ]]
then
  VERSIONS=("merge_sort_std_parallel" "mctop_sort_sse" "mctop_sort_sse_hyperthreads_3" "mctop_sort_no_sse_all_sockets" "mctop_sort_no_sse_no_smt_all_sockets");
  THREADS=("16" "16" "16" "16" "16");
elif [[ $UNAME  == "diassrv8" ]]
then
    VERSIONS=("merge_sort_std_parallel" "mctop_sort_sse" "mctop_sort_sse_hyperthreads_3" "mctop_sort_no_sse_no_smt_all_sockets");
  THREADS=("16" "16" "16" "16");
elif [[ $UNAME  == "ol-collab1" ]]
then
  VERSIONS=("merge_sort_std_parallel" "mctop_sort_no_sse_all_sockets");
  THREADS=("16" "16");
fi


for (( index=0; index< ${#VERSIONS[@]}; index++ ));
do
  version=${VERSIONS[index]};
  threads=${THREADS[index]};
  echo "${version} ${threads} ${ALLOCATOR} ${SIZE}";
  for (( rep=0; rep<$REPS; rep++ ));
  do
    text=$(./${binf}/${version} -n${threads} -s${SIZE} -p${ALLOCATOR} 2>&1);
    res=$(echo "$text" | awk '/Sorted/ {printf"%.3f", $9;}');
    error=$(echo "$text" | awk '/is sorted/ {printf"%d", $9;}');
    if [[ $res ]]
    then
      printf "%s " ${res}
    else
      printf "%s " "n/a"
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
