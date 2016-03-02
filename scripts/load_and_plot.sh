#!/bin/bash

print_lvl=0;

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
dot_format=ps;
out_format=pdf

make ${tool} > /dev/null;
./${tool} -m desc/${un}.mct -l$print_lvl

target=intra_socket
echo "## Ploting ${target}";
dot -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
epstopdf ${outf}/${un}_${target}.${dot_format}
evince ${outf}/${un}_${target}.${out_format} &

target=cross_socket
echo "## Ploting ${target}";
sfdp -x -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
# neato -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
# dot  -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
epstopdf ${outf}/${un}_${target}.${dot_format}
evince ${outf}/${un}_${target}.${out_format} &
