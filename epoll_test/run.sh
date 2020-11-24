#!/bin/bash

server_ip='10.0.0.2'
server_port=80

echo -n "buffer size: "
read buff_size

echo -n "duration: "
read duration

echo -n "number of CPU cores: "
read num_core

echo -n "number of threads per core: "
read num_thread

for i in `seq 1 $num_core`
do
    ./linux_test --server_ip=$server_ip --server_port=$server_port --size=$buff_size --time=$duration --num_thread=$num_thread --core_id=$i &
done

wait

total=`expr $num_core \* $num_thread`

python merge_file.py $total