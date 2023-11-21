# run this on rover (jetson)
# path variables
config_files=$HDTN_RTP_DIR/config_files/luna_net
bpsendstream_config=$config_files/bpsendstream_udp.json
test_media_folder=/home/jetson/test_media/official_test_media
file=$test_media_folder/lucia.mp4

hdtn_config=$config_files/hdtn_one_process_node_1.json
contact_plan=$HDTN_RTP_DIR/config_files/contact_plans/LunaNetContactPlanNodeIDs.json

incoming_rtp_port=20000

cd $HDTN_RTP_DIR

ffmpeg -y -sdp_file HDTN.sdp -re -i $file -c copy -an -f rtp "rtp://127.0.0.1:$rtp_port" &
ffmpeg_process=$!

# pause ffmpeg to let HDTN start
kill -s STOP $ffmpeg_process

cd $HDTN_SOURCE_ROOT
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
one_process_pid=$!
sleep 6

cd $HDTN_RTP_DIR


./build/bpsend_stream --bundle-size=2000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.2 --outducts-config-file=$bpsendstream_config \
        --max-incoming-udp-packet-size-bytes=1600 --incoming-rtp-stream-port=$incoming_rtp_port --num-circular-buffer-vectors=500 \
        --enable-rtp-concatenation=false --sdp-filepath="HDTN.sdp" &
media_source_process=$!

sleep 3

# resume ffmpeg
kill -s CONT $ffmpeg_process


sleep 400
echo "\nkilling media source ..." && kill -2 $media_source_process