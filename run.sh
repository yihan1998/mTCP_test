#!/bin/bash

server='server'
client='client'

server_ip='172.18.13.14'
server_port=12345

if test $1 = $server
then
    rm record_core_*.txt
    make clean
    make server
    ./server -N 1 -p service -f server.conf
else
    ./client $2 $server_ip $server_port $3
fi