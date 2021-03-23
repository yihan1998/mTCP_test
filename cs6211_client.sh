#!/bin/bash
trap 'exit' TERM

server_ip='10.0.0.2'
server_port=80

echo -n "Buffer size(B): "
read buff_size
#buff_size=1024

#echo -n "number of CPU cores: "
#read num_core
max_cores=8

echo -n "Total test time(s): "
read test_time

echo -n "Benchmark type[open/close loop]?: "
read test_mode

echo -n "Record Round Trip Time[yes/no]?: "
read record_rtt

echo -n "Number of jobs: "
read num_jobs

if [[ "$record_rtt" == *"yes"* ]];then
    echo " >> evaluting Round Trip Time"
    eval_rtt=1
else
    eval_rtt=0
fi

rm rtt_*.txt

make clean && make client RTT=$eval_rtt

#echo -n "number of connections: "
#read num_connection

for j in $(seq 1 $num_jobs)
do
    num_flow=1
    num_cores=$max_cores
    total_conn=$( expr "$num_cores" '*' "$num_flow" )

    echo "Testing for $total_conn connections on $num_cores core(s), each have $num_flow connection(s) ..."

    ./client    --num_cores=$num_cores \
                --num_flow=$num_flow \
                --size=$buff_size \
                --time=$test_time \
                --server_ip=$server_ip \
                --server_port=$server_port \
                --test_mode=$test_mode
    
    wait

    echo "Test done"

    sleep 5

    if [ $eval_rtt -eq 1 ]
    then
        total=`expr $num_cores \* $num_flow`
        python merge_file.py $total
    fi
done