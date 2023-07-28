#!/bin/sh
#run this on terminal
# path variables
config_files=$HDTN_RTP_DIR/config_files/ltp_egress_node_sink_test
sink1_config=$config_files/bpsink_node_3.json

cd $HDTN_SOURCE_ROOT

# bpsink1
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:3.1 --inducts-config-file=$sink1_config &
bpsink_PID=$!
sleep 3


sleep 360

pkill -2 $bpsink_PID