cd $HDTN_SOURCE_ROOT/common/regsvr
`python3 main.py` &
sleep 3
cd $HDTN_SOURCE_ROOT/build/common/bpcodec/apps
./bpsink-async "--use-tcpcl" "--port=4558" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/module/egress
./hdtn-egress-async "--use-tcpcl" "--port1=0" "--port2=4558" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/module/ingress 
./hdtn-ingress "--always-send-to-storage" & 
sleep 3
cd $HDTN_SOURCE_ROOT/build/module/storage
./hdtn-release-message-sender "--release-message-type=stop" "--flow-id=2" "--delay-before-send=14" &
sleep 1
cd $HDTN_SOURCE_ROOT/build/module/storage
./hdtn-storage "--storage-config-json-file=$HDTN_SOURCE_ROOT/module/storage/unit_tests/storageConfigRelativePaths.json" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/common/bpcodec/apps
./bpgen-async "--bundle-rate=100" "--use-tcpcl" "--flow-id=2" "--duration=5" &
sleep 8

PID1=`pgrep bpgen-async`
PID2=`pgrep hdtn-storage`
PID3=`pgrep hdtn-release-message-sender`
PID4=`pgrep hdtn-ingress`
PID5=`pgrep hdtn-egress-async`
PID6=`pgrep bpsink-async`
sleep 60
(kill  $PID1)
(kill -INT $PID2)
(kill -INT $PID3)
(kill -INT $PID4)
(kill -INT $PID5)
(kill -INT $PID6)

kill $(pgrep -f 'python3 main.py')


