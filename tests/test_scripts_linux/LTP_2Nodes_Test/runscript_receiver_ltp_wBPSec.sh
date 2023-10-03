#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_Node2_ltp.json
sink_config=$config_files/inducts/bpsink_one_stcp_port4558_2NodesTest.json
bpsink_bpsec_config=$config_files/bpsec/ipn2.1_con.json

cd $HDTN_SOURCE_ROOT

# bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config --bpsec-config-file=$bpsink_bpsec_config&
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlan2Nodes.json  --hdtn-config-file=$hdtn_config &
sleep 10




