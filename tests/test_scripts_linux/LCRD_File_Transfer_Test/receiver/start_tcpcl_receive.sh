#!/bin/bash


NAME="HDTN_RECEIVER"
DEST="HDTN_SENDER"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")


#default (no arguments) without custody
#With argument "-c 1" with custody 

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

mkdir received
mkdir checksums

if (( CUSTODY == 0 )); then
        CL="TCPCL_NO_CUSTODY"

        ./run_hdtn_oneprocess_tcpcl &
	sleep 6

	echo "Receive"
	./rcv_files_tcpcl  &

	./wait.sh
	echo "Done"
	echo "Starting checksums"

	./sha.sh ./received/ 

	rm -rf ./received/*
	echo "Test done"

else
	CL="TCPCL_CUSTODY"

	echo "Exiting: there's no test yet for TCPCL with custody "
        exit  
fi
