#!/bin/bash

ccbench_folder=./external/lib/ccbench;
ccbench_git="https://github.com/trigonak/ccbench.git"
ccbench=${ccbench_folder}/ccbench

if [ ! -d $ccbench_folder ];
then
    echo "#WARN: ccbnech does not exist: Installing!";
    echo "#WARN: ccbnech clone";
    git clone $ccbench_git $ccbench_folder;
    cd $ccbench_folder;
    echo "#WARN: ccbnech make";
    make
    if [ ! $? -eq 0 ];
    then
	echo "#ERROR: ccbnech make FAILED";
    fi;
    cd -;
fi;

${ccbench} -t12 -s1 $@;





