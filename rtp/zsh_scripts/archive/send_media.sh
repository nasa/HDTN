
# !/bin/sh 

# path variables
config_files=$HDTN_RTP_DIR/config_files
hdtn_config=$config_files/hdtn/hdtn_node1.json
source_config=$config_files/outducts/mediasource_stcp.json

cd $HDTN_RTP_DIR

# ./hdtn_build_files/hdtn-scheduler --contact-plan-file=contactPlanCutThroughMode.json --hdtn-config-file=$hdtn_config &
# scheduler_PID=$!
# sleep 5

# media source
./build/dtn_rtp_stream_source  --frame-width=1280 --frame-height=960 --frames-per-second=30  --local-frame-queue-size=1 --video-device=/dev/video0 --bundle-size=200000 --bundle-rate=0 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --duration=400 --outducts-config-file=$source_config &
media_source_process=$!
sleep 8
# # # HDTN one process
# ./hdtn_build_files/hdtn-one-process  --hdtn-config-file=$hdtn_config &
# one_process_PID=$!
# sleep 6


sleep 400

echo "\nkilling HDTN one process ..." && kill -2 $one_process_PID
sleep 2

echo "\nkilling media source ..." && kill -2 $media_source_process
