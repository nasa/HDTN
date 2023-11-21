# !/bin/zsh 

config_files=$HDTN_RTP_DIR/config_files/udp/test_2_hdtn_loopback
# config_files=$HDTN_RTP_DIR/config_files/ltp/test_2_hdtn_loopback
# source_config=$config_files/mediasource_ltp.json
source_config=$config_files/mediasource_udp.json
# source_config=$config_files/mediasource_stcp.json

test_files=/home/$USER/test_media/official_test_media
# filename=water_bubble_cbr21
filename=water_bubble_crf18
# filename=lucia_cbr21
# filename=lucia_crf18
file=$test_files/$filename.mp4

incoming_rtp_port=30000

shm_socket_path=/tmp/hdtn_gst_shm_induct_socket

cd $HDTN_RTP_DIR

rm $shm_socket_path*
pkill -9 BpSendStream

# start gstreamer first if running shared memory interface
gst-launch-1.0 filesrc location=$file  ! qtdemux ! "video/x-h264" \
        ! queue ! h264parse config-interval=-1 ! progressreport update-freq=1 ! rtph264pay \
        ! queue ! shmsink socket-path=$shm_socket_path wait-for-connection=true &

sleep 2

./build/bpsend_stream  --bundle-size=65535 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
        --num-circular-buffer-vectors=15000 --rtp-packets-per-bundle=5 --max-incoming-udp-packet-size-bytes=1460\
        --induct-type="shm" --file-to-stream=$shm_socket_path &
        # --induct-type="appsink" --file-to-stream=$file  &
        # --induct-type="udp" --incoming-rtp-stream-port=$incoming_rtp_port &  
bpsend_stream_process=$!


sleep 10000

echo "\nkilling video process ..." && pkill -15 BpSendStream

