# !/bin/zsh 

config_files=$HDTN_RTP_DIR/config_files/udp/test_3_point_to_point
source_config=$config_files/mediasource_udp.json

test_files=/home/$USER/test_media/official_test_media
file=$test_files/lucia_crf18_g_15.mp4
# file=$test_files/ammonia_trimmed.wav

cd $HDTN_RTP_DIR

incoming_rtp_port=29999

./build/bpsend_stream  --bundle-size=2000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
        --max-incoming-udp-packet-size-bytes=1800 --incoming-rtp-stream-port=$incoming_rtp_port --num-circular-buffer-vectors=3000 \
        --enable-rtp-concatenation=false  --rtp-packets-per-bundle=1 &
media_source_process=$!

sleep 3

####################################################### gst 
gst-launch-1.0 filesrc location=$file ! qtdemux ! h264parse ! rtph264pay config-interval=4 ! udpsink host=127.0.0.1 port=$incoming_rtp_port
####################################################### gst 


sleep 3



sleep 400

echo "\nkilling video process ..." && kill -2 $media_source_process
kill -2 $ffmpeg_process
