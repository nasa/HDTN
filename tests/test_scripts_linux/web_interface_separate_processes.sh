#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
hdtn_distributed_config=$config_files/hdtn/hdtn_distributed_defaults.json
sink_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json

cd $HDTN_SOURCE_ROOT

./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
bpsink1_PID=$!
sleep 3

#Egress
./build/module/egress/hdtn-egress-async --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config &
egress_PID=$!
sleep 3

#Ingress
./build/module/ingress/hdtn-ingress --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config  &
ingress_PID=$!
sleep 3

#storage
./build/module/storage/hdtn-storage --hdtn-config-file=$hdtn_config --hdtn-distributed-config-file=$hdtn_distributed_config &
storage_PID=$!
sleep 3

#Web Interface
./build/module/telem_cmd_interface/telem_cmd_interface
