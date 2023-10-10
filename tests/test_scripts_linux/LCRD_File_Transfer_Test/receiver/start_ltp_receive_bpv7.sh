#!/bin/bash


NAME="HDTN_RECEIVER"
DEST="HDTN_SENDER"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")


#BPSec ( disabled= 0, enabled = 1)
BPSEC=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -s)
      if [[ $2 -eq 0 ]]; then
              BPSEC=0
      else
              if (( $2 >= 0 && $1 <=2 )); then
                      BPSEC="$2"
              else
                      BPSEC=0
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

if (( BPSEC == 0 )); then
        CL="BPSEC_DISABLED"

        ./run_hdtn_oneprocess_ltp &
	sleep 6

	echo "Receive"
	./rcv_files &
	./wait.sh
	echo "Done"
	echo "Starting checksums"

	./sha.sh ./received/ 

	rm -rf ./received/*
	echo "Test done"

else
	CL="BPSEC_ENABLED"

        ./run_hdtn_oneprocess_ltp  &
	sleep 6

	echo "Receive"
	./rcv_files_bpsec_confidentialityIntegrity &
	
	./wait.sh
	echo "Done"
	echo "Starting checksums"
	
	./sha.sh ./received/

	rm -rf ./received/*

	echo "Test done"

fi

