#!/bin/bash

echo -n "buffer size: "
read buff_size
#buff_size=1024

#echo -n "number of CPU cores: "
#read num_core
num_core=1

echo -n "number of connections: "
read num_connection

echo "Testing RTT for $num_connection connections..."

./server    --num_core=$num_core \
            --size=$buff_size \
            --num_client=$num_connection

echo "Test done"
