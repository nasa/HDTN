#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json
sink1_config=$config_files/inducts/bpsink_one_tcpcl_port4557.json
sink2_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556.json
HDTN_SOURCE_ROOT=$(dirname "$0")

cd $HDTN_SOURCE_ROOT

# bpsink1
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:1.1 --inducts-config-file=$sink1_config &
bpsink1_PID=$!
sleep 3

# bpsink2
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink2_config &
bpsink2_PID=$!
sleep 3

#HDTN One Process
./build/module/hdtn_one_process/hdtn_one_process &
hdtn_one_process_PID=$!
sleep 1

#Scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlan.json --hdtn-config-file=$hdtn_config &
scheduler_PID=$!
sleep 1

# bpgen1
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --my-uri-eid=ipn:101.1 --dest-uri-eid=ipn:1.1 --duration=40 --outducts-config-file=$gen_config &
bpgen1_PID=$!
sleep 1

#bpgen2
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --my-uri-eid=ipn:102.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &
bpgen2_PID=$!
sleep 8


