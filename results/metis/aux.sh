#!/bin/bash 

function pdebug()
{
    __in_r=\$"$1";
    __in_v=$(eval "expr \"$__in_r\"");
    echo "[ $__in_r = $__in_v ]";
}

function swap_if_less()
{
    __tar_r=\$"$1";
    __tar_v=$(eval "expr \"$__tar_r\"");
    __val=$2;
    
    if [ $(echo "$__val < $__tar_v" | bc) -eq 1 ];
    then
	eval "$1=\"$__val\"";  # Assign new value.
	return 1;
    fi;
    return 0;
}

function cast_ratio_to_percentage()
{
    __tar_r=\$"$1";
    __tar_v=$(eval "expr \"$__tar_r\"");

    __val=$(echo "100.0 * ($__tar_v - 1)" | bc -l);

    eval "$1=\"$__val\"";  # Assign new value.
}

function one_if_zero()
{
    __tar_r=\$"$1";
    __tar_v=$(eval "expr \"$__tar_r\"");
    
    if [ $(echo "$__tar_v == 0" | bc) -eq 1 ];
    then
	eval "$1=\"1\"";  # Assign new value.
	return 1;
    fi;
    return 0;
}

# a=1;
# b=2;
# pdebug a;
# swap_if_less a $b;
# pdebug a;

# a=1;
# b=1;
# pdebug a;
# swap_if_less a $b;
# pdebug a;



# a=1;
# b=0.1;
# pdebug a;
# swap_if_less a $b;
# pdebug a;

# a=2;
# b=1;
# pdebug a;
# swap_if_less a $b;
# pdebug a;

