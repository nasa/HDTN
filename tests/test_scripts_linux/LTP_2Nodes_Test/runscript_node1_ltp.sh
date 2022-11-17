#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_Node1_ltp.json
gen_config=$config_files/outducts/bpgen_one_ltp_port4556_2NodesTest.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --hdtn-config-file=$hdtn_config &
sleep 10

#Scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlan2Nodes.json --hdtn-config-file=$hdtn_config &
sleep 1

#bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-size=100000 --bundle-rate=0 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &
sleep 8


