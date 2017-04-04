#!/bin/bash

ccbench=./scripts/ccbench.sh;

$ccbench -h &> /dev/null;

numactl_out=$(numactl -H);

nodes=$(echo "$numactl_out" | awk '/node .* cpus/ { print $2 }');

for n0 in $nodes;
do
    c0=$(echo "$numactl_out" | awk -v node=$n0 '/node .* cpus/ {if ($2 == node) print $4 }');
    for n1 in $nodes;
    do
	c1=$(echo "$numactl_out" | awk -v node=$n1 '/node .* cpus/ {if ($2 == node) print $5 }');
	printf "########## %3d (core %3d)  <--> %3d (core %3d) #######################################################\n" $n0 $c0 $n1 $c1;
	${ccbench} -x$c0 -y$c1 | grep -A7 "statistics:";
    done;
done;
