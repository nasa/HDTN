#!/bin/bash

export HDTN_BUILD_ROOT=/home/btomko/hdtn/build
export HDTN_SOURCE_ROOT=/home/btomko/hdtn


NAME="TANTALUS"
DEST="NUC3"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")


#get custody type (none = 0, old = 1, new = 2)
CUSTODY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c)
      if [[ $2 -eq 0 ]]; then
              CUSTODY=0
      else
              if (( $2 >= 0 && $1 <=2 )); then
                      CUSTODY="$2"
              else
                      CUSTODY=0
              fi
      fi
      shift
      shift
      ;;
    *)
      echo "Unknown argument $1 - use"
      echo "./$0"
      echo "./$0 0"
      echo "./$0 1"
      exit
      ;;
  esac
done

#kill existing HDTN
./kill.sh


if (( CUSTODY == 0 )); then
        CL="LTP_FRAG_NO_CUSTODY"

        ./run_hdtn_oneprocess_ltp > "./log/log_${NAME}_${CL}_recv_${TIME}" 2>&1 &
	sleep 6
        ./release_all
        #scheduler --dest-addr=$DEST > "./log/pingLog_${NAME}_${CL}_recv_${TIME}" 2>&1 &

	echo "Receive"
	./rcv_files > "./log/log_${NAME}_${CL}_recv_files_${TIME}" 2>&1 &

	./wait.sh
	echo "Done"
	echo "Starting checksums"

	./sha.sh ./received/flightdata $NAME $CL $TIME

	rm -rf ./received/flightdata/*
	echo "Test done"

else
	CL="LTP_FRAG_CUSTODY"

        ./run_hdtn_oneprocess_ltp_custody > "./log/log_${NAME}_${CL}_recv_${TIME}" 2>&1 &
	sleep 6
        ./release_all
        ./run_scheduler > "./log/pingLog_${NAME}_${CL}_recv_${TIME}" 2>&1 &
        sleep 3

	echo "Receive"
	./rcv_files_custody > "./log/log_${NAME}_${CL}_recv_files_${TIME}" 2>&1 &
	
	./wait.sh
	echo "Done"
	echo "Starting checksums"
	
	./sha.sh ./received/flightdata $NAME $CL $TIME

	rm -rf ./received/flightdata/*

	echo "Test done"

fi

