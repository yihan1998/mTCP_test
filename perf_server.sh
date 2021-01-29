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

for j in $(seq 0 8)
do
    num_connection=`echo "2^$j" | bc `

    echo "Testing RTT for $num_connection connections..."

    perf_output="perf-mtcp-$num_cores-$num_connection.txt"

    perf stat -a -o $perf_output -d -d --per-core --no-aggr -- ./server \
                --num_cores=$num_cores \
                --size=$buff_size \
                --time=$test_time \
                --num_client=$num_connection \
                --test_mode=$test_mode 

    wait

    echo "Test done"
done