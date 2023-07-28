config_files=$HDTN_RTP_DIR/config_files/stcp/test_2_hdtn_loopback
sink_config=$config_files/mediasink_stcp.json

outgoing_rtp_port=40004 

output_file_path="/home/$USER/gstream/test_2"
filename=water_bubble_h264_vbr                 # change this for whatever file you want to name
file=$output_file_path/$filename

mkdir -p  $output_file_path/$filename

cd $HDTN_RTP_DIR 

export GST_DEBUG=3,rtph264depay:7,h264parse:9,mp4mux:8
./build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config  --outgoing-rtp-hostname=127.0.0.1 \
        --outgoing-rtp-port=$outgoing_rtp_port --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1800 & 
        # --ffmpeg-command="\
# gst-launch-1.0 -v  sdpsrc sdp="application/x-rtp, v=0 m=video $outgoing_rtp_port RTP/AVP 96 c=IN IP4 127.0.0.1 a=rtpmap:96 H264/90000"
# ! rtph264depay ! h264parse  ! mp4mux ! filesink location=$file/$filename.mp4 sync=false -e" &
# gst-launch-1.0 -v  sdpsrc sdp="application/x-rtp, v=0 m=video $outgoing_rtp_port RTP/AVP 96 c=IN IP4 127.0.0.1 a=rtpmap:96 H264/90000"\
# ! rtph264depay ! h264parse  ! mp4mux ! filesink location=$file/$filename.mp4 sync=false -e
#  gst-launch-1.0 -v udpsrc port=$outgoing_rtp_port ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
#  ! rtph264depay !  h264parse ! decodebin ! videoconvert ! autovideosink sync=false # fakesink silent=false # ! qtmux ! # filesink location=xyz.mp4 -e # mp4mux ! filesink location=$file.mp4 -e 
# ! decodebin ! videoconvert ! autovideosink 
recv_pid=$!


sleep 400

kill -2 $recv_pid
