#!/bin/sh

# HDTN Path Variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_Node1_ltp.json
gen_config=$config_files/outducts/bpgen_one_stcp_port4556_2NodesTest.json


cd $HDTN_SOURCE_ROOT


# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=$HDTN_SOURCE_ROOT/module/router/contact_plans/contactPlan2Nodes.json --hdtn-config-file=$hdtn_config &

sleep 6

# BpSendStream
incoming_rtp_port=6565
./build/common/bpcodec/apps/bpsend_stream  --use-bp-version-7 --bundle-rate=0 --bundle-size=200000  --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --induct-type="udp" --max-incoming-udp-packet-size-bytes=1800 --num-circular-buffer-vectors=3000 --rtp-packets-per-bundle=20 --incoming-rtp-stream-port=$incoming_rtp_port --outducts-config-file=$gen_config &

sleep 3

# GStreamer Pipeline to Start Streaming Video File with Audio
VIDEO_TO_STREAM_FILEPATH=$HDTN_SOURCE_ROOT/launch.mp4
gst-launch-1.0 -e filesrc location=$VIDEO_TO_STREAM_FILEPATH \
    ! qtdemux name=demux \
    demux.video_0 ! queue ! h265parse ! mpegtsmux name=mux ! rtpmp2tpay ! udpsink host=127.0.0.1 port=$incoming_rtp_port \
    demux.audio_0 ! queue ! aacparse ! mux. &

sleep infinity
