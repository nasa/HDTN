#!/bin/bash

NAME="HDTN_SENDER"
DEST="HDTN_RECEIVER"
#get timestamp for logs
TIME=$(date "+%Y.%m.%d-%H.%M.%S")


#BPSec (disabled= 0, enabled = 1)
BPSEC=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -s)
      if [[ $2 -eq 0 ]]; then
	      BPSEC=0
      else
	      if (( $2 >= 0 && $2 <=1 )); then
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


if (( BPSEC == 0 )); then
	CL="BPSEC_DISABLED"

	./run_hdtn_oneprocess_ltp &
	sleep 6
	./send_files_bpv7

else
	CL="BPSEC_ENABLED"

	./run_hdtn_oneprocess_ltp &
	sleep 6
	./send_files_bpv7_bpsec_confidentiality

fi


