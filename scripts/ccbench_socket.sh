#!/bin/bash

node=0;
if [ $# -gt 0 ];
then
    node=$1;
    shift;
fi;

ccbench=./scripts/ccbench.sh;

$ccbench -h &> /dev/null;

numactl_out=$(numactl -H);

cores=$(echo "$numactl_out" |
	       awk -v node=$node '/node .* cpus/ { if ( $2 == node ) { print $0; }}' |
	       cut -d':' -f2;
     );


printf "      ";
for c in $cores;
do
    printf "%-9d " $c;
done;
echo "";

out=ccbench_socket_node${node}.out;
printf "" > $out;

for c0 in $cores;
do
    printf "%-4d " $c0;
    for c1 in $cores;
    do
	if [ $c0 -eq $c1 ];
	then
	    printf " %-8d " 0;
	else
	    echo "------------------------------------------------------------------------>> ${c0} - ${c1}" >> $out;
	    res=$(${ccbench} -x$c0 -y$c1);
	    echo "$res" >> $out;

	    xr=$(echo "$res" | awk '/\[00\]   0-10\%/ { print $9 }');
	    yr=$(echo "$res" | awk '/\[01\]   0-10\%/ { print $9 }');
	    printf "%4.0f/%-4.0f " $xr $yr;
	fi;
    done;
    echo "";
done;
