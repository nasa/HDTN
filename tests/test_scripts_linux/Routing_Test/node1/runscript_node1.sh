#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_node1_cfg.json
gen_config=$config_files/outducts/bpgen_one_stcp_port4556_routing.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=contactPlan_RoutingTest.json --hdtn-config-file=$hdtn_config &
sleep 10

# bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --my-uri-eid=ipn:100.1 --dest-uri-eid=ipn:200.1 --duration=40 --outducts-config-file=$gen_config &
sleep 1

