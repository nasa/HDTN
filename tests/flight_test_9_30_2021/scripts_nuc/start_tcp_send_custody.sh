#!/bin/bash

NAME="NUC3"
DEST="TANTALUS"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")
CL="TCP_CUSTODY"

#kill existing HDTN
./kill.sh


./run_hdtn_oneprocess_tcpcl_custody > "./log/log_${NAME}_${CL}_send_${TIME}" 2>&1 &
sleep 6
./release_all
./run_scheduler > "./log/pingLog_${NAME}_${CL}_send_${TIME}" 2>&1 &
sleep 3

./send_files_bidirectional_tcp  > "./log/log_${NAME}_${CL}_send_files_${TIME}" 2>&1 





