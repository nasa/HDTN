#!/bin/bash

NAME="NUC3"
DEST="TANTALUS"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")
CL="STCP"

#kill existing HDTN
./kill.sh



./run_hdtn_oneprocess_stcp > "./log/log_${NAME}_${CL}_recv_${TIME}" 2>&1 &
sleep 6
./release_all
scheduler --dest-addr=$DEST > "./log/pingLog_${NAME}_${CL}_recv_${TIME}" 2>&1 &

./rcv_files
./sha.sh ./received/flightdata $NAME $CL $TIME
rm -rf ./received/flightdata/*

