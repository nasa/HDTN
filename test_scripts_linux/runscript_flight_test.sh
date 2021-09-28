#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556.json

cd $HDTN_SOURCE_ROOT

# registration server
python3 ./common/regsvr/main.py &
sleep 3

# bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
bpsink_PID=$!
sleep 3

#Egress
./build/module/egress/hdtn-egress-async --hdtn-config-file=$hdtn_config &
egress_PID=$!
sleep 3

#Scheduler
./build/module/scheduler/hdtn-scheduler --ping-test --dest-addr=127.0.0.1 --dest-uri-eid=ipn:2.1 --hdtn-config-file=$hdtn_config &
scheduler_PID=$!
sleep 1

#Ingress
./build/module/ingress/hdtn-ingress --hdtn-config-file=$hdtn_config  &
ingress_PID=$!
sleep 3

#storage 
./build/module/storage/hdtn-storage --hdtn-config-file=$hdtn_config &
storage_PID=$!
sleep 3

#bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &
bpgen_PID=$!
sleep 8

# cleanup
sleep 30
echo "\nkilling bpgen..." && kill -2 $bpgen_PID
sleep 2
echo "\nkilling HDTN storage..." && kill -2 $storage_PID
sleep 2
echo "\nkilling HDTN ingress..." && kill -2 $ingress_PID
sleep 2
echo "\nkilling scheduler..." && kill -9 $scheduler_PID
sleep 2
echo "\nkilling egress..." && kill -2 $egress_PID
sleep 2
echo "\nkilling bpsink..." && kill -2 $bpsink_PID
sleep 2
echo "\nkilling registration server..." && pkill -9 -f main.py



