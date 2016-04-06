#!/bin/bash

working_set=512
num_threads=10


printf "               %5d %5d %5d %5d %5d %5d\n" 1 2 4 6 8 10
for partitions in "2" "4" "8" "16" "32" "64" "128" "256"
do
   printf "%3d partitions" ${partitions}
   for threads_per_partition in "1" "2" "4" "6" "8" "10"
   do
      ./merge_sort_merge_level ${working_set} ${num_threads} 1 1 ${partitions} ${threads_per_partition} | grep duration | awk '{printf " %5d", $3;}'
   done;
   printf "\n"
done;


