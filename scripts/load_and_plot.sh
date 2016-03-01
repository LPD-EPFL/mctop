#!/bin/bash

un=$(uname -n);
if [ $# -gt 0 ];
then
    un=$1;
fi;

echo "#### Graphs fro $un";

tool=mct_load;
inf=dot;
outf=graphs;

make ${tool} > /dev/null;
./${tool} -m desc/${un}.mct

target=intra_socket
echo "## Ploting ${target}";
dot -Tpdf ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.pdf
evince ${outf}/${un}_${target}.pdf &

# target=cross_socket
# echo "## Ploting ${target}";
# sfdp -x -Tpdf ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.pdf
# # neato -Tpdf ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.pdf
# # dot  -Tpdf ${inf}/dot_${target}.dot > ${outf}/${un}_${target}.pdf
# evince ${outf}/${un}_${target}.pdf &
