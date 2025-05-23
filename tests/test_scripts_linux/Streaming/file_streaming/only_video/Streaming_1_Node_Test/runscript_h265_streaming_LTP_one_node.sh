#!/bin/sh

# HDTN Path Variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1ltp_port4556_egress1ltp_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_ltp_port4558.json
gen_config=$config_files/outducts/bpgen_one_ltp_port4556_thisengineid200.json

cd $HDTN_SOURCE_ROOT

# GStreamer Pipeline to Save RTP Stream to Disk
outgoing_rtp_port=8554
gst-launch-1.0 -e udpsrc address=127.0.0.1 port=8900 ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H265, payload=(int)96" \
        ! rtph265depay ! queue ! h265parse config-interval=1 ! qtmux ! filesink location=received.mp4 &

sleep 2

gst-launch-1.0 -e udpsrc address=127.0.0.1 port=$outgoing_rtp_port ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H265, payload=(int)96" \
        ! tee name=t \
        t. ! queue ! udpsink host=127.0.0.1 port=8989 sync=false \
        t. ! queue ! udpsink host=127.0.0.1 port=8900 sync=false &

sleep 2

# BpReceiveStream
./build/common/bpcodec/apps/bprecv_stream --my-uri-eid=ipn:2.1 --max-rx-bundle-size-bytes=70000 --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1460 --outduct-type=udp --outgoing-rtp-port=$outgoing_rtp_port --outgoing-rtp-hostname=127.0.0.1 --inducts-config-file=$sink_config &

sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=$HDTN_SOURCE_ROOT/module/router/contact_plans/contactPlanCutThroughMode_unlimitedRate.json --hdtn-config-file=$hdtn_config &

sleep 6

# BpSendStream
incoming_rtp_port=6565
./build/common/bpcodec/apps/bpsend_stream  --use-bp-version-7 --bundle-rate=0 --bundle-size=200000  --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --induct-type="udp" --max-incoming-udp-packet-size-bytes=1800 --num-circular-buffer-vectors=3000 --rtp-packets-per-bundle=20 --incoming-rtp-stream-port=$incoming_rtp_port --outducts-config-file=$gen_config &

sleep 3

# GStreamer Pipeline to Start Streaming File
VIDEO_TO_STREAM_FILEPATH=$HDTN_SOURCE_ROOT/launch.mp4
gst-launch-1.0 -e filesrc location=$VIDEO_TO_STREAM_FILEPATH ! qtdemux ! h265parse ! rtph265pay config-interval=1 ! udpsink host=127.0.0.1 port=$incoming_rtp_port &

sleep infinity
