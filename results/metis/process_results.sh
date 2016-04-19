#!/bin/bash

. ./scripts/aux.sh
. ./scripts/table.sh

file=$1;
shift;

# echo "-- Processing $file";

if [ ! -f$file ];
then
    echo "---- $file does not exist";
    exit;
fi;

i1=$(grep -n "policy" $file | sed -n 1p | cut -d':' -f1);
i2=$(grep -n "policy" $file | sed -n 2p | cut -d':' -f1);
chunk_n=$((i2 - i1));

# echo "---- Will process chunks of $chunk_n lines";

i1=1;
i2=$chunk_n;

while [ 1 ];
do
    data=$(sed -n ${i1},${i2}p $file);
    if [ "$data" = "" ];
    then
	break;
    fi;

    echo "$data";

    ex=$(echo "$data" | head -n1 | awk '{print $1}');
    cores=( $(echo "$data" | awk '/policy/ {$1="";$2=""; print}') );
    nc=${#cores[@]};
    policies=( $(echo "$data" | tail -n+3 | awk '{print $1}') );
    np=${#policies[@]};

    numbers=$(echo "$data" | tail -n+3 | awk '{$1=""; print}');

    i=0;
    for n in $numbers;
    do
	i=$((i + 1));
	if [ $i -eq 1 ];
	then
	    table_append time_tbl $n;
	elif [ $i -eq 2 ];
	then
	    table_append pow_tbl $n;
	else
	    table_append ene_tbl $n;
	    i=0;
	fi
    done;

    # table_print "$time_tbl" $nc;
    # table_print "$pow_tbl" $nc;
    # table_print "$ene_tbl" $nc;

    for ((seq_pi=0; seq_pi < $np; seq_pi++));
    do
	if [ ${policies[$seq_pi]} = "MCTOP_ALLOC_SEQUENTIAL" ];
	then
	    break;
	fi;
    done;

    printf "%-15s -- VS. seq\n" $ex;

    table_row_get "$time_tbl" $nc $seq_pi seq_time;
    seq_time_a=( $seq_time );
    table_row_get "$pow_tbl" $nc $seq_pi seq_pow;
    seq_pow_a=( $seq_pow );
    table_row_get "$ene_tbl" $nc $seq_pi seq_ene;
    seq_ene_a=( $seq_ene );

    time_best=${seq_time_a};
    pow_best=${seq_pow_a};
    ene_best=${seq_ene_a};

    for ((p=0; p < $np; p++));
    do
	printf "%-35s" ${policies[$p]};

	table_row_get "$time_tbl" $nc $p pol_time;
	pol_time_a=( $pol_time );
	table_row_get "$pow_tbl" $nc $p pol_pow;
	pol_pow_a=( $pol_pow );
	table_row_get "$ene_tbl" $nc $p pol_ene;
	pol_ene_a=( $pol_ene );

	for ((i=0; i < $nc; i++))
	do
	    time_s=${seq_time_a[$i]};
	    time_c=${pol_time_a[$i]};
	    one_if_zero time_s;
	    time_r=$(echo "$time_c/$time_s" | bc -l);
	    pow_s=${seq_pow_a[$i]};
	    pow_c=${pol_pow_a[$i]};
	    one_if_zero pow_s;
	    pow_r=$(echo "$pow_c/$pow_s" | bc -l);
	    ene_s=${seq_ene_a[$i]};
	    ene_c=${pol_ene_a[$i]};
	    one_if_zero ene_s;
	    ene_r=$(echo "$ene_c/$ene_s" | bc -l);
	    # cast_ratio_to_percentage time_r;
	    # cast_ratio_to_percentage pow_r;
	    # cast_ratio_to_percentage ene_r;
	    # printf " %-6.1f %-6.1f %-8.1f" $time_r $pow_r $ene_r;
	    printf " %-6.3f %-6.3f %-8.3f" $time_r $pow_r $ene_r;

	    swap_if_less time_best $time_c;
	    swaped=$?;
	    if [ $swaped -eq 1 ];
	    then
		pow_best=$pow_c;
		ene_best=$ene_c;
	    fi
	    # swap_if_less pow_best $pow_c;
	    # swap_if_less ene_best $ene_c;
	done;
	echo "";
    done;

    one_if_zero time_best;
    one_if_zero pow_best;
    one_if_zero ene_best;

    printf "%-15s -- VS. best time\n" $ex;

    for ((p=0; p < $np; p++));
    do
	printf "%-35s" ${policies[$p]};

	table_row_get "$time_tbl" $nc $p pol_time;
	pol_time_a=( $pol_time );
	table_row_get "$pow_tbl" $nc $p pol_pow;
	pol_pow_a=( $pol_pow );
	table_row_get "$ene_tbl" $nc $p pol_ene;
	pol_ene_a=( $pol_ene );

	for ((i=0; i < $nc; i++))
	do
	    time_c=${pol_time_a[$i]};
	    time_r=$(echo "$time_c/$time_best" | bc -l);
	    pow_c=${pol_pow_a[$i]};
	    pow_r=$(echo "$pow_c/$pow_best" | bc -l);
	    ene_c=${pol_ene_a[$i]};
	    ene_r=$(echo "$ene_c/$ene_best" | bc -l);
	    # cast_ratio_to_percentage time_r;
	    # cast_ratio_to_percentage pow_r;
	    # cast_ratio_to_percentage ene_r;
	    # printf " %-6.1f %-6.1f %-8.1f" $time_r $pow_r $ene_r;
	    printf " %-6.3f %-6.3f %-8.3f" $time_r $pow_r $ene_r;
	done;
	echo "";
    done;

    table_empty time_tbl;
    table_empty pow_tbl;
    table_empty ene_tbl;

    i1=$((i2 + 1));
    i2=$((i2 + chunk_n));
done;

# echo "-- Done!";
      


