#!/bin/bash
trap 'exit' TERM

echo -n "Buffer size(B): "
read buff_size
#buff_size=1024

echo -n "number of CPU cores: "
read num_cores
#num_cores=1

echo -n "Total test time(s): "
read test_time

echo -n "benchmark type[open/close loop]?: "
read test_mode

#echo -n "number of connections: "
#read num_connection

make clean && make server 

sar_file="sar_result"

if [ ! -d "$sar_file" ];then
    echo making sar_result directory
    mkdir sar_result
else
    echo cleaning sar_result directory
    rm -fr sar_result/*
fi
for j in $(seq 0 10)
do
    num_connection=`echo "2^$j" | bc `

    echo "Testing RTT for $num_connection connections..."

    sar_output="$sar_file/sar-mtcp-$num_cores-$num_connection.txt"

    exec_time=`expr $test_time \* 2`

    sar -A 1 $exec_time > $sar_output &

    ./server    --num_cores=$num_cores \
                --size=$buff_size \
                --time=$test_time \
                --test_mode=$test_mode 

    wait $!

    pkill -9 sar
    
    wait

    echo "Test done"
done