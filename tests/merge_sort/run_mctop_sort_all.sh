#!/bin/sh

out_folder=results/merge_sort/final;
gmkdir $out_folder 2> /dev/null;
out=${out_folder}/"$(uname -n)".txt

./tests/merge_sort/run_mctop_sort_final_small.sh | tee $out;
./tests/merge_sort/run_mctop_sort_final_large.sh | tee -a $out;

./tests/merge_sort/run_mctop_sort_scalability_small.sh | tee -a $out;
./tests/merge_sort/run_mctop_sort_scalability_large.sh | tee -a $out; 	
