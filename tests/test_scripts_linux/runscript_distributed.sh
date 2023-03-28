#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
hdtn_distributed_config=$config_files/hdtn/hdtn_distributed_defaults.json
sink_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556.json

cd $HDTN_SOURCE_ROOT

# bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
bpsink_PID=$!
sleep 3

# Egress
./build/module/egress/hdtn-egress-async --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config &
sleep 3

# Ingress
./build/module/ingress/hdtn-ingress --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config &
sleep 3

#Storage
./build/module/storage/hdtn-storage --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config &
sleep 3

#Scheduler
./build/module/scheduler/hdtn-scheduler --hdtn-config-file=$hdtn_config --contact-plan-file=contactPlanCutThroughMode.json --hdtn-distributed-config-file=$hdtn_distributed_config &
sleep 4

#Router
./build/module/router/hdtn-router --hdtn-config-file=$hdtn_config --contact-plan-file=contactPlanCutThroughMode.json --hdtn-distributed-config-file=$hdtn_distributed_config &
sleep 4

#Telemetry
./build/module/telem_cmd_interface/telem_cmd_interface --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config &
sleep 3

#bpgen (configure bundle-rate=0 to send bundles at high rate)
./build/common/bpcodec/apps/bpgen-async --bundle-rate=0 --bundle-size=100000 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &

