#!/bin/bash

echo -n "Buffer size(B): "
read buff_size
#buff_size=1024

#echo -n "number of CPU cores: "
#read num_core
num_core=1

#echo -n "number of connections: "
#read num_connection

cd build

for j in $(seq 0 10)
do
    num_connection=`echo "2^$j" | bc `

    perf_file="perf_stat_$num_connection"

    echo "Testing RTT for $num_connection connections..."

    perf stat -d -C 0 -o $perf_file \
            ./server    --num_core=$num_core \
                        --size=$buff_size \
                        --num_client=$num_connection

    echo "Test done"
done
