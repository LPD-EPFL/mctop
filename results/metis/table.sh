#!/bin/bash

. ./scripts/aux.sh

# Usage   : table_emtpy tbl
function table_empty()
{
    eval "$1=\"\"";  # Assign new value.
}

# Usage   : table_append cur_table val
# Example : table_append tbl $val
function table_append()
{
    __tbl_r=\$"$1";
    __tbl_v=$(eval "expr \"$__tbl_r\"");
    __val=$2;

    eval "$1=\"$__tbl_v $__val\"";  # Assign new value.
}

# Usage   : table_get table num_columns x y ret
# Reqs    : (x, y >= 0)
# Example : table_get $tbl 3 1 2 ret
function table_get()
{
    __tbl=( $1 );
    __nc=$2;
    __x=$3;
    __y=$4;
    __offs=$(echo "(($__x) * $__nc) + $__y" | bc);
    __val=${__tbl[$__offs]};
    eval "$5=\"$__val\"";  # Assign new value.
}

# Usage   : table_row_get table num_columns x ret
# Reqs    : (x >= 0)
# Example : table_row_get $tbl 3 1 ret
function table_row_get()
{
    __tbl=( $1 );
    __nc=$2;
    __x=$3;
    __offs=$(echo "(($__x) * $__nc)" | bc);
    __val="";
    for ((i = 0; i < $__nc; i++))
    do
	__val="$__val ${__tbl[$(($__offs + $i))]}";
    done;
    eval "$4=\"$__val\"";  # Assign new value.
}

# Usage   : table_print table num_columns
# Example : table_print $tbl 3
function table_print()
{
    __tbl="$1";
    __nc=$2;

    __break=0;
    for __it in $__tbl;
    do
	printf "%-8s" $__it;
	__break=$((__break + 1));
	if [ $__break -eq $__nc ];
	then
	    echo "";
	    __break=0;
	fi;
    done;
}

# tbl="1 2 3";
# echo $tbl;

# table_append tbl 4
# echo $tbl;

# table_print "$tbl" 2;

# table_get "$tbl" 2 0 0 v1;
# pdebug v1;
# table_get "$tbl" 2 1 0 v1;
# pdebug v1;
# table_get "$tbl" 2 0 1 v1;
# pdebug v1;
# table_get "$tbl" 2 1 1 v1;
# pdebug v1;

# table_row_get "$tbl" 1 0 v1;
# pdebug v1;
# table_row_get "$tbl" 2 1 v1;
# pdebug v1;
