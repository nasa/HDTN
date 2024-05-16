#!/bin/sh
config_files=$HDTN_RTP_DIR/tests/test_scripts_linux/one_node_ltp/config_files


# Launch HDTN
hdtn-one-process --contact-plan-file=$config_files/contact_plan.json --hdtn-config-file=$config_files/hdtn.json &


# Launch bprecv_stream to receive bundles and forward as RTP stream
$HDTN_STREAMING_DIR/build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$config_files/mediasink_ltp.json --max-rx-bundle-size-bytes 70000 \
        --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1460 --outduct-type="udp" --outgoing-rtp-port=8554 --outgoing-rtp-hostname="127.0.0.1" &
bprecv_stream_pid=$!


sleep 1000
kill -2 $bprecv_stream_pid
