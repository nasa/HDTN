#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556_allowrxcustodysignals.json

cd $HDTN_SOURCE_ROOT

# BP Receive File
./build/common/bpcodec/apps/bpreceivefile --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlanCutThroughMode_unlimitedRate.json --hdtn-config-file=$hdtn_config &
sleep 6

# BP Send File 
./build/common/bpcodec/apps/bping  --use-bp-version-7 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.2047 --outducts-config-file=$gen_config &
