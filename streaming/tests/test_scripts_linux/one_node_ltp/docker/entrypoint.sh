#!/bin/sh

# Load Gstreamer env variables
eval $($GST_PATH/gst-env.py --only-environment)

# Start sender
$HDTN_RTP_DIR/tests/test_scripts_linux/one_node_ltp/node_1_recv.sh &
sleep 5

# Start receiver
$HDTN_RTP_DIR/tests/test_scripts_linux/one_node_ltp/node_1_send.sh &
sleep 1000
