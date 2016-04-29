#!/bin/sh

echo "## Compiling.."
./tests/merge_sort/make_mctop_sort.sh > /dev/null;
echo "## Done!"

out_folder=results/merge_sort/final;
gmkdir $out_folder 2> /dev/null;

rsetting=1;
if [ $rsetting -eq 0 ];
then
    out=${out_folder}/"$(uname -n)".txt
else
    out=${out_folder}/"$(uname -n)".r${rsetting}.txt
fi;

echo "## Using -r${rsetting}! Output: "$out;

./tests/merge_sort/run_mctop_sort_final_small.sh ${rsetting} | tee $out;
./tests/merge_sort/run_mctop_sort_final_large.sh ${rsetting} | tee -a $out;

./tests/merge_sort/run_mctop_sort_scalability_small.sh ${rsetting} | tee -a $out;
./tests/merge_sort/run_mctop_sort_scalability_large.sh ${rsetting} | tee -a $out; 	
