# !/bin/zsh 

config_files=$HDTN_RTP_DIR/config_files/test_3_point_to_point
source_config=$config_files/mediasource_ltp.json

test_files=/home/$USER/test_media/official_test_media
# file=$test_files/lucia_crf18.mp4
file=$test_files/ammonia_trimmed.wav

cd $HDTN_RTP_DIR


incoming_rtp_port=29999
####################################################### FFMPEG 

# change this if sending video or audio
audio_only="-c:a aac -b:a 96k -vn -f flv"
video_only="-c copy -an"
ffmpeg_command_slice=$audio_only
ffmpeg -y -sdp_file HDTN.sdp -re -i $file $ffmpeg_command_slice -f rtp "rtp://127.0.0.1:$incoming_rtp_port" &
ffmpeg_process=$!

# pause ffmpeg to let HDTN start
kill -s STOP $ffmpeg_process
####################################################### FFMPEG 

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
