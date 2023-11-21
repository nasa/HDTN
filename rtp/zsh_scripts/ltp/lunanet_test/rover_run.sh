# run this on rover 
# path variables
config_files=$HDTN_RTP_DIR/config_files/ltp/test_6_luna_net
bpsendstream_config=$config_files/bpsendstream_stcp.json
test_media_folder=/home/$USER/test_media/official_test_media
# file=$test_media_folder/lucia_crf18.mp4
# file=$test_media_folder/lucia_cbr21.mp4
file=$test_media_folder/water_bubble_crf18.mp4

hdtn_config=$config_files/hdtn_one_process_node_1.json
bpsec_config=$HDTN_SOURCE_ROOT/config_files/bpsec/ipn8.1_con.json
contact_plan=$HDTN_RTP_DIR/config_files/contact_plans/LunaNetContactPlanNodeIDs.json

incoming_rtp_port=6565
port_to_gstreamer=5656

pkill -9 HdtnOneProcessM


cd $HDTN_SOURCE_ROOT
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
one_process_pid=$!
sleep 9

cd $HDTN_RTP_DIR
export GST_DEBUG=3
./build/bpsend_stream  --bundle-size=65535 --bundle-rate=0 --use-bp-version-7 --bpsec-config-file=$bpsec_config \
        --my-uri-eid=ipn:8.1 --dest-uri-eid=ipn:7.1 --outducts-config-file=$bpsendstream_config \
        --num-circular-buffer-vectors=10000 --rtp-packets-per-bundle=20 --max-incoming-udp-packet-size-bytes=1460\
        --induct-type="udp" --incoming-rtp-stream-port=$incoming_rtp_port &  
        # --induct-type="appsink" --file-to-stream=$file  &
bpsend_stream_process=$!

gst-launch-1.0 udpsrc port=$port_to_gstreamer ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
    ! queue ! rtph264depay ! h264parse config-interval=3 ! rtph264pay ! udpsink port=$incoming_rtp_port host=127.0.0.1   &

sleep 2

# generate video stream using ffmpeg
# gstreamer theoretically supports RPi4 camera but I havent gotten it to work. 
# for now, generate stream and send to local host gstreamer
ffmpeg  -v verbose -f v4l2 -input_format h264 -video_size 1920x1080 -framerate 30 -fflags +genpts \
    -i /dev/video0 -c:v copy -an -f rtp udp://localhost:$port_to_gstreamer  &

sleep 10000


echo "\nkilling media source ..." && kill -2 $bpsend_stream_process
echo "\nkilling oneprocess ..." && kill -2 $one_process_pid
