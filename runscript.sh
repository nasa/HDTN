cd $HDTN_SOURCE_ROOT/common/regsvr
`python3 main.py` &
sleep 3
cd $HDTN_SOURCE_ROOT/build/common/bpcodec/apps
./bpsink-async "--inducts-config-file=$HDTN_SOURCE_ROOT/tests/config_files/inducts/bpsink_one_tcpcl_port4557.json" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/common/bpcodec/apps
./bpsink-async "--inducts-config-file=$HDTN_SOURCE_ROOT/tests/config_files/inducts/bpsink_one_tcpcl_port4558.json" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/module/egress
./hdtn-egress-async "--hdtn-config-file=$HDTN_SOURCE_ROOT/tests/config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/module/scheduler
./hdtn-scheduler "--contact-plan-file=contactPlan.json" "--hdtn-config-file=$HDTN_SOURCE_ROOT/tests/config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json" &
sleep 1
cd $HDTN_SOURCE_ROOT/build/module/ingress
./hdtn-ingress "--hdtn-config-file=$HDTN_SOURCE_ROOT/tests/config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/module/storage
./hdtn-storage "--hdtn-config-file=$HDTN_SOURCE_ROOT/tests/config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json" &
sleep 3
cd $HDTN_SOURCE_ROOT/build/common/bpcodec/apps
./bpgen-async "--bundle-rate=0" "--flow-id=2" "--duration=20" "--bundle-size=100000" "--outducts-config-file=$HDTN_SOURCE_ROOT/tests/config_files/outducts/bpgen_one_tcpcl_port4556.json" &
sleep 1
cd $HDTN_SOURCE_ROOT/build/common/bpcodec/apps
./bpgen-async "--bundle-rate=0" "--flow-id=1" "--duration=20" "--bundle-size=100000" "--outducts-config-file=$HDTN_SOURCE_ROOT/tests/config_files/outducts/bpgen_one_tcpcl_port4556.json" &
sleep 8

PID1=`pgrep bpgen-async`
PID2=`pgrep hdtn-ingress`
PID3=`pgrep hdtn-egress-async`
PID4=`pgrep bpsink-async`
PID5=`pgrep hdtn-storage`
PID6=`pgrep hdtn-scheduler`
sleep 60
(kill  $PID1)
(kill -INT $PID2)
(kill -INT $PID3)
(kill -INT $PID4)
(kill -INT $PID5)
(kill -INT $PID6)

kill $(pgrep -f 'python3 main.py')

