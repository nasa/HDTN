#!/bin/bash

NAME="TANTALUS"
DEST="NUC3"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")
CL="TCP"

#kill existing HDTN
./kill.sh


./run_hdtn_oneprocess_tcpcl > "./log/log_$NAME_$CL_send_$TIME" 2>&1 &
sleep 6
./release_all
scheduler --dest-addr=$DEST > "./log/pingLog_$NAME_$CL_send_$TIME" 2>&1 &
./send_files


