#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_Node1_ltp.json
gen_config=$config_files/outducts/bpgen_one_stcp_port4556_2NodesTest.json
bpsec_config=$config_files/bpsec/ipn1.1_con.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=contactPlan2Nodes.json --hdtn-config-file=$hdtn_config &
sleep 10

#bpgen
./build/common/bpcodec/apps/bpgen-async --use-bp-version-7 --bundle-size=100000 --bundle-rate=0 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config --bpsec-config-file=$bpsec_config &
sleep 8


