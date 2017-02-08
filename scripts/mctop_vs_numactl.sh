#!/bin/bash

make mct_numactl_print;

out_numa=.tmp.numa.out;
out_mctop=.tmp.mctop.out;

numactl -H | grep -v MB > $out_numa;
./mct_numactl_print $@ | tail -n+2 > $out_mctop;

echo "|--> NUMA                                                     |--> MCTOP";

if [ ! $(which colordiff) = "" ];
then
    diff -w -s -y -d $out_numa $out_mctop | colordiff
else
    echo "Hint: install colordiff for more clear results"
    diff -w -s -y -d $out_numa $out_mctop 
fi;



echo "";
echo "Use ccbench (https://github.com/trigonak/ccbench) to explore potential differences!"


rm $out_numa $out_mctop &> /dev/null;
