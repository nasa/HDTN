config_files=$HDTN_RTP_DIR/config_files/udp/rover_demo
source_config=$config_files/mediasource_udp.json

port_to_gstreamer=5656
port_to_hdtn=5800

# start hdtn to initialize shared memory interface
cd $HDTN_RTP_DIR
./build/bpsend_stream  --bundle-size=65535 --bundle-rate=0 --use-bp-version-7 \
    --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
    --num-circular-buffer-vectors=10000 --rtp-packets-per-bundle=5 --max-incoming-udp-packet-size-bytes=1460 \
    --induct-type="udp" --incoming-rtp-stream-port=$port_to_hdtn &  

sleep 3

gst-launch-1.0 udpsrc port=$port_to_gstreamer ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
    ! queue ! rtph264depay ! h264parse config-interval=3 ! rtph264pay ! udpsink port=$port_to_hdtn host=127.0.0.1   &

sleep 2

# generate video stream using ffmpeg
# gstreamer theoretically supports RPi4 camera but I havent gotten it to work. 
# for now, generate stream and send to local host gstreamer
ffmpeg  -v verbose -f v4l2 -input_format h264 -video_size 1920x1080 -framerate 30 -fflags +genpts \
    -i /dev/video0 -c:v copy -an -f rtp udp://10.1.1.80:$port_to_gstreamer  &


sleep 500


