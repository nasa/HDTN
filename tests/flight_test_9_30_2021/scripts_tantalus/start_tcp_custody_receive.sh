#!/bin/bash

NAME="TANTALUS"
DEST="NUC3"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")
CL="TCP_CUSTODY"

#kill existing HDTN
./kill.sh



./run_hdtn_oneprocess_tcpcl_custody > "./log/log_${NAME}_${CL}_recv_${TIME}" 2>&1 &
sleep 6
./release_all
./run_scheduler  > "./log/pingLog_${NAME}_${CL}_recv_${TIME}" 2>&1 &
sleep 3

echo "Receive"
./rcv_files_bidirectional_tcp > "./log/log_${NAME}_${CL}_recv_files_${TIME}" 2>&1 &

./wait.sh
echo "Done"
echo "Starting checksums"

./sha.sh ./received/flightdata $NAME $CL $TIME

rm -rf ./received/flightdata/*

echo "Test done"

