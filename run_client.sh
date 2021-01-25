#!/bin/bash

server_ip='10.0.0.2'
server_port=80

echo -n "buffer size: "
read buff_size
#buff_size=1024

#echo -n "number of CPU cores: "
#read num_core
max_cores=4

test_time=60

#echo -n "number of connections: "
#read num_connection

cd build


for j in $(seq 0 10)
do
    total_conn=`echo "2^$j" | bc `

    if [ $total_conn -gt $max_cores ]
    then
        num_cores=$max_cores
    else
        num_cores=$total_conn
    fi

    num_flow=`expr $total_conn / $num_cores`

    echo "Testing RTT for $total_conn connections on $num_cores core(s), each have $num_flow connection(s) ..."

    ./client    --num_cores=$num_cores \
                --num_flow=$num_flow \
                --size=$buff_size \
                --time=$test_time \
                --server_ip=$server_ip \
                --server_port=$server_port
    
    wait

    echo "Test done"

    sleep 10
done