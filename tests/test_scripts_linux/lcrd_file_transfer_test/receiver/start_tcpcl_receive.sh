#!/bin/bash


NAME="HDTN_RECEIVER"
DEST="HDTN_SENDER"
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

<<<<<<< HEAD:tests/test_scripts_linux/lcrd_file_transfer_test2/receiver/start_tcpcl_receive.sh
	echo "Exiting: there's no test yet for TCPCL with custody "
        exit  
=======
	echo "Exiting, cannot do a tcpcl custody transfer yet"
        exit
>>>>>>> 4cf10f96b67cc83ad97781397f4af949d75a1e2f:tests/test_scripts_linux/lcrd_file_transfer_test2/lcrd_file_transfer_test2/receiver/start_tcpcl_receive.sh
fi

