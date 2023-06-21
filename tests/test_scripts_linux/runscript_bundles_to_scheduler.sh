#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_stcp_port4558.json
gen_config=$config_files/outducts/bpgen_one_stcp_port4556.json

cd $HDTN_SOURCE_ROOT

# bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlanCutThroughMode_unlimitedRate.json --hdtn-config-file=$hdtn_config &
sleep 6

#bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-rate=1 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:10.100 --duration=10 --bundle-size=100000 --outducts-config-file=$gen_config &



