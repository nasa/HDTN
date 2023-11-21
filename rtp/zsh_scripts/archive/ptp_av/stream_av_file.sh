# !/bin/sh 

# path variables
config_files=$HDTN_RTP_DIR/config_files

video_source_config=$config_files/ptp_av_file/mediasource_stcp.json
audio_source_config=$config_files/ptp_av_file/mediasource_stcp_5004.json

test_media_folder=/home/jetson/test_media/official_test_media

file=$test_media_folder/lucia.mp4

video_port=50000
audio_port=50002
 

cd $HDTN_RTP_DIR

# video sender
./build/bpsend_stream  --bundle-size=100000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$video_source_config \
        --max-incoming-udp-packet-size-bytes=1600 --incoming-rtp-stream-port=$video_port --num-circular-buffer-vectors=750\
        --enable-rtp-concatenation=false & 
bp_video_process=$!

# audio sender
./build/bpsend_stream  --bundle-size=100000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$audio_source_config \
        --max-incoming-udp-packet-size-bytes=1600 --incoming-rtp-stream-port=$audio_port --num-circular-buffer-vectors=750\
        --enable-rtp-concatenation=false & 
bp_audio_process=$!


# audio and video stream stream
ffmpeg -sdp_file HDTN.sdp -re -i $file -vcodec copy -an -f rtp "rtp://127.0.0.1:$video_port" \
        -vn -acodec copy -f rtp "rtp://127.0.0.1:$audio_port" &



sleep 400

echo "\nkilling video process ..." && kill -2 $bp_video_process
echo "\nkilling audio process ..." && kill -2 $bp_audio_process
