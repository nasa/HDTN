# !/bin/zsh 

config_files=$HDTN_RTP_DIR/config_files/udp/test_2_hdtn_loopback
source_config=$config_files/mediasource_udp.json

incoming_rtp_port=30000

shm_socket_path=/tmp/hdtn_gst_shm_induct_socket

cd $HDTN_RTP_DIR

rm $shm_socket_path*
pkill -9 BpSendStream

sleep 0.5
rtsp_stream_location="rtsp://10.1.1.250:554/cam/realmonitor?channel=1&subtype=0&unicast=false"
gst-launch-1.0 rtspsrc location=$rtsp_stream_location user-id=admin user-pw=hdtn4nasa \
! "application/x-rtp,media=(string)video,payload=(int)96,clockrate=(int)90000" \
! queue \
! udpsink host="127.0.0.1" port=$incoming_rtp_port &

sleep 2 

./build/bpsend_stream  --bundle-size=65535 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
        --num-circular-buffer-vectors=10000 --rtp-packets-per-bundle=1 --max-incoming-udp-packet-size-bytes=1460\
        --induct-type="udp" --incoming-rtp-stream-port=$incoming_rtp_port &  
bpsend_stream_process=$!


sleep 10000

echo "\nkilling video process ..." && pkill -15 BpSendStream

# OLD AMCREST
# gst-launch-1.0 rtspsrc location="rtsp://10.54.31.253:554/cam/realmonitor?channel=1&subtype=0&unicast=false" user-id=admin user-pw=nasa1234 ! queue ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! videoscale ! video/x-raw,width=3840,height=2160 ! autovideosink
#gst-launch-1.0 rtspsrc location="rtsp://10.54.31.253:554/cam/realmonitor?channel=1&subtype=0&unicast=false" user-id=admin user-pw=nasa1234 ! queue ! udpsink host=127.0.0.1 port=30000 
# NEW AMCREST
# gst-launch-1.0 rtspsrc location="rtsp://10.1.1.250:554/cam/realmonitor?channel=1&subtype=0&unicast=false" user-id=admin user-pw=hdtn4nasa ! queue ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! videoscale ! video/x-raw,width=3840,height=2160 ! autovideosink

