#!/bin/bash

NAME="NUC3"
DEST="TANTALUS"
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
	      if (( $2 >= 0 && $2 <=1 )); then
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

	./run_hdtn_oneprocess_ltp_frag > "./log/log_${NAME}_${CL}_send_${TIME}" 2>&1 &
	sleep 6
	./release_all
        ./run_scheduler  > "./log/pingLog_${NAME}_${CL}_send_${TIME}" 2>&1 &
        sleep 3	
        ./send_files  > "./log/log_${NAME}_${CL}_send_files_${TIME}" 2>&1 


else
	CL="LTP_FRAG_CUSTODY"

	./run_hdtn_oneprocess_ltp_custody_frag > "./log/log_${NAME}_${CL}_send_${TIME}" 2>&1 &
	sleep 6
	./release_all
        ./run_scheduler  > "./log/pingLog_${NAME}_${CL}_send_${TIME}" 2>&1 &
         sleep 3
	./send_files_custody  > "./log/log_${NAME}_${CL}_send_files_${TIME}" 2>&1 


fi


