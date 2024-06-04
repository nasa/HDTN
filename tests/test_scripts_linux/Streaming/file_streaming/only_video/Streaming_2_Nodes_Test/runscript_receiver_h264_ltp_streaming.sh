#!/bin/sh

# HDTN Path Variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_Node2_ltp.json
sink_config=$config_files/inducts/bpsink_one_stcp_port4558_2NodesTest.json


cd $HDTN_SOURCE_ROOT

# GStreamer Pipeline to Save RTP Stream to Disk
outgoing_rtp_port=8554
gst-launch-1.0 -e udpsrc address=127.0.0.1 port=$outgoing_rtp_port ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
        ! tee name=t \
        t. ! rtph264depay ! queue ! h264parse ! qtmux ! filesink location=received.mp4 sync=false \
        t. ! queue ! udpsink host=127.0.0.1 port=8989 sync=false &

sleep 2

# BpReceiveStream
./build/common/bpcodec/apps/bprecv_stream --my-uri-eid=ipn:2.1 --max-rx-bundle-size-bytes=70000 --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1460 --outduct-type=udp --outgoing-rtp-port=$outgoing_rtp_port --outgoing-rtp-hostname=127.0.0.1 --inducts-config-file=$sink_config &

sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=$HDTN_SOURCE_ROOT/module/router/contact_plans/contactPlan2Nodes.json --hdtn-config-file=$hdtn_config &

sleep infinity

