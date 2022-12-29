#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
sink1_config=$config_files/inducts/bpsink_one_tcpcl_port4557.json
sink2_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json

cd $HDTN_SOURCE_ROOT

# bpsink1
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:1.1 --inducts-config-file=$sink1_config &
bpsink1_PID=$!
sleep 3

# bpsink2
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink2_config &
bpsink2_PID=$!
sleep 3

