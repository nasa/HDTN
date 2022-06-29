#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
sink_config=$config_files/inducts/bpsink_one_tcpclv4_port4558.json

cd $HDTN_SOURCE_ROOT


# BP Sink File
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config 


