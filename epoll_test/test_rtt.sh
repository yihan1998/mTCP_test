#!/bin/bash

server_ip='10.0.0.2'
server_port=80

max_core=16

echo -n "buffer size: "
read buff_size

#buff_size=1024

#echo -n "duration: "
#read duration

duration=60

#echo -n "number of CPU cores: "
#read num_core

#echo -n "number of threads per core: "
#read num_thread

#echo -n "number of server CPU cores: "
#read num_server_core

for j in $(seq 0 10)
do
    total_conn=`echo "2^$j" | bc `
    
    if [ $total_conn -gt $max_core ]
    then
        num_core=$max_core
    else
        num_core=$total_conn
    fi

    conn_per_core=`expr $total_conn / $num_core`

    echo "Testing RTT for $total_conn connections on $num_core core(s), each have $conn_per_core connection(s) ..."
    
    for i in `seq 1 $num_core`
    do
        ./linux_test    --server_ip=$server_ip \
                        --server_port=$server_port \
                        --size=$buff_size \
                        --time=$duration \
                        --num_thread=$conn_per_core \
                        --core_id=$i &
    done

    wait
    
    echo "Test done"

    python merge_file.py $total_conn
done