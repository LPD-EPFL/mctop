#!/bin/bash

step=$1;

n_hwcs=$(nproc);
mctop_policy=4;
mbs="128 256";

steps=$(seq $step $step $n_hwcs);


make -j merge_sort_parallel_merge >> /dev/null;

for mb in $mbs;
do
    printf "%-10s %-10s %s -- %d MB\n" "#Cores" "Dur (ms)" "Ratio to prev" $mb;

    prev=0;
    for s in $steps;
    do
	dur_ms=$(./merge_sort_parallel_merge $mb $s $mctop_policy | awk '/duration/ { print $4 }');
	if [ $prev -gt 0 ];
	then
	    ratio=$(echo "$dur_ms/$prev" | bc -l);
	else
	    ratio=1;
	fi;
	printf "%-10d %-10d %-f\n" $s $dur_ms $ratio;
	prev=$dur_ms
    done;
done;
