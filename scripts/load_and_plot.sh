#!/bin/bash

un=$(uname -n);
if [ $# -gt 0 ];
then
    un=$1;
    re='^[0-9]+$'
    if [[ $un =~ $re ]];
    then
	print_lvl=$un
	un=$(uname -n);
    fi
    shift;
fi;

if [ $# -gt 0 ];
then
    print_lvl=$1;
    shift;
fi;

echo "#### Graphs from $un";

tool=mct_load;
inf=dot;
outf=graphs;
out_format=ps;

make ${tool} > /dev/null;
./${tool} -m desc/${un}.mct -l$print_lvl

target=intra_socket
echo "## Ploting ${target}";
dot -T${out_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${out_format}
evince ${outf}/${un}_${target}.${out_format} &

target=cross_socket
echo "## Ploting ${target}";
sfdp -x -T${out_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${out_format}
# neato -T${out_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${out_format}
# dot  -T${out_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${out_format}
evince ${outf}/${un}_${target}.${out_format} &
