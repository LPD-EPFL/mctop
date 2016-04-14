#!/bin/bash

print_lvl=0;

un=$(uname -n);
if [ $# -gt 0 ];
then
    un=$1;
    shift;
fi;

open=0;
if [ $# -gt 1 ];
then
    open=1;
fi;

echo "#### Graphs from $un";

inf=dot;
outf=graphs;
dot_format=ps;
out_format=pdf

target=intra_socket
echo "## Ploting ${target}";
dot -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
epstopdf ${outf}/${un}_${target}.${dot_format}
if [ $open -eq 1 ];
then
    evince ${outf}/${un}_${target}.${out_format} &
fi;
target=cross_socket
echo "## Ploting ${target}";
sfdp -x -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
# neato -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
# dot  -T${dot_format} ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.${dot_format}
epstopdf ${outf}/${un}_${target}.${dot_format}
if [ $open -eq 1 ];
then
    evince ${outf}/${un}_${target}.${out_format} &
fi;
