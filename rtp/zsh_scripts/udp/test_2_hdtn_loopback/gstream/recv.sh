config_files=$HDTN_RTP_DIR/config_files/udp/test_2_hdtn_loopback
# config_files=$HDTN_RTP_DIR/config_files/ltp/test_2_hdtn_loopback
sink_config=$config_files/mediasink_udp.json
# sink_config=$config_files/mediasink_ltp.json
# sink_config=$config_files/mediasink_stcp.json

outgoing_rtp_port=40004 

output_file_path="/home/$USER/test_outputs/test_2"
# filename=water_bubble_cbr21
filename=water_bubble_crf18
# filename=lucia_cbr21                 # change this for whatever file you want to name
# filename=lucia_crf18
file=$output_file_path/$filename


shm_socket_path_display=/tmp/hdtn_gst_shm_outduct_display
shm_socket_path_filesink=/tmp/hdtn_gst_shm_outduct_filesink

mkdir -p  $output_file_path/$filename

cd $HDTN_RTP_DIR 

pkill -9 BpRecvStream
pkill -9 gst-launch-1.0

export GST_DEBUG_FILE=/tmp/gst_log.log
#################################################################################
echo "Deleting old socket file: "
echo $shm_socket_path_display
rm $shm_socket_path_display
echo $shm_socket_path_filesink
rm $shm_socket_path_filesink

#################################################################################
echo "Deleting old output file: "
echo $file/$filename.mp4
rm $file/$filename.mp4
sleep .5
#################################################################################

./build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config --max-rx-bundle-size-bytes 14000 \
        --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1460 --outduct-type="appsrc" --shm-socket-path=$shm_socket_path_display &
bprecv_stream_pid=$!

sleep 3

# if we are using appsrc, launch a separate gstreamer instance to save the video stream to file 
export GST_DEBUG=3,shmsrc:3,filesink:6,rtpjitterbuffer:3,avdec_h264:3
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


#https://gstreamer.freedesktop.org/documentation/codectimestamper/h264timestamper.html?gi-language=c