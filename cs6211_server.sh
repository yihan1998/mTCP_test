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

echo -n "Number of jobs: "
read num_jobs

#echo -n "number of connections: "
#read num_connection

make clean && make server 

rm throughput_*.txt

for j in $(seq 1 $num_jobs)
do
    sed -i '/rcvbuf/d' server.conf
    sed -i '/sndbuf/d' server.conf

    queue_size=`expr 1024 \* $j`

    sed -i '$a rcvbuf = '${queue_size}'' server.conf
    sed -i '$a sndbuf = '${queue_size}'' server.conf

    num_connection=`echo "2^$j" | bc `

    echo "Testing for $j queue jobs..."

    ./server    --num_cores=$num_cores \
                --size=$buff_size \
                --time=$test_time \
                --test_mode=$test_mode 

    wait

    echo "Test done"
done