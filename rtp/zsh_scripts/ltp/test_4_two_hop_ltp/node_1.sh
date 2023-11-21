# !/bin/sh 

# path variables
config_files=$HDTN_RTP_DIR/config_files

source_config=$config_files/two_hop_ltp/node_1_video_source.json
node_1_hdtn_config=$config_files/two_hop_ltp/node_1.json

contact_plan=$HDTN_RTP_DIR/config_files/two_hop_ltp/contact_plan.json

test_media_folder=/home/jetson/test_media/official_test_media
file=$test_media_folder/lucia_crf18.mp4


incoming_rtp_port=20000

# hdtn one process
cd $HDTN_SOURCE_ROOT
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
one_process_pid=$!
sleep 6


cd $HDTN_RTP_DIR


ffmpeg -y -sdp_file HDTN.sdp -re -i $file -c copy -an -f rtp "rtp://127.0.0.1:$incoming_rtp_port" &
ffmpeg_process=$!

# pause ffmpeg to let HDTN start
kill -s STOP $ffmpeg_process

./build/bpsend_stream  --bundle-size=2000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
        --max-incoming-udp-packet-size-bytes=1800 --incoming-rtp-stream-port=$incoming_rtp_port --num-circular-buffer-vectors=3000 \
        --enable-rtp-concatenation=false --sdp-filepath="HDTN.sdp" --sdp-sending-interval-ms=5000 --rtp-packets-per-bundle=1 &
media_source_process=$!

sleep 3

# resume ffmpeg
kill -s CONT $ffmpeg_process


sleep 400

echo "\nkilling video process ..." && kill -2 $media_source_process
kill -2 $ffmpeg_process
kill -2 $one_process_pid