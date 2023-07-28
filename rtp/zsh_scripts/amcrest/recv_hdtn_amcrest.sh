config_files=$HDTN_RTP_DIR/config_files/udp/test_2_hdtn_loopback
sink_config=$config_files/mediasink_udp.json

output_file_path="/home/$USER/test_outputs/amcrest"
filename=amcrest_stream

file=$output_file_path/$filename

shm_socket_path_display=/tmp/hdtn_gst_shm_outduct_display
shm_socket_path_filesink=/tmp/hdtn_gst_shm_outduct_filesink

export GST_DEBUG_FILE=/tmp/gst_log.log

mkdir -p  $output_file_path/$filename

cd $HDTN_RTP_DIR 

pkill -9 BpRecvStream
pkill -9 gst-launch-1.0



echo "Deleting old socket file: "
echo $shm_socket_path_display
rm $shm_socket_path_display
echo $shm_socket_path_filesink
rm $shm_socket_path_filesink


echo "Deleting old output file: "
echo $file/$filename.mp4
rm $file/$filename.mp4
sleep .5


./build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config --max-rx-bundle-size-bytes 14000 \
        --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1460 --outduct-type="appsrc" --shm-socket-path=$shm_socket_path_display &
bprecv_stream_pid=$!

sleep 3

# if we are using appsrc, launch a separate gstreamer instance to save the video stream to file 
gst-launch-1.0 shmsrc socket-path=$shm_socket_path_display  is-live=true do-timestamp=true \
        ! queue max-size-time=0 max-size-bytes=0 !  "video/x-raw, format=(string)I420, width=(int)3840, height=(int)2160, framerate=(fraction)30000/1001" \
        !  timeoverlay halignment=left valignment=top text="recv stream time:" shaded-background=false font-desc="Sans, 10" \
        ! glupload ! glimagesink &
display_pid=$! 

gst-launch-1.0 shmsrc socket-path=$shm_socket_path_filesink is-live=true do-timestamp=true \
        ! queue ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
        ! queue ! rtpjitterbuffer ! rtph264depay ! h264parse config-interval=1 \
        ! h264timestamper \
        ! qtmux faststart=true \
        ! filesink location=$file/$filename.mp4 -e &
filesink_pid=$!

sleep 10000

kill -2 $bprecv_stream_pid
kill -2 $display_pid
kill -2 $filesink_pid