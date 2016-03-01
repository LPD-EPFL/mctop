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

echo "## Ploting socket";
dot -Tpdf ${inf}/dot_socket.dot > ${outf}/${un}_socket.pdf
evince ${outf}/${un}_socket.pdf &
