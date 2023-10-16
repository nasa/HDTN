#!/bin/sh
#run this on moc 
# path variables
config_files=$HDTN_RTP_DIR/config_files/ltp/test_6_luna_net
contact_plan=$HDTN_RTP_DIR/config_files/contact_plans/LunaNetContactPlanNodeIDs.json
hdtn_config=$config_files/hdtn_one_process_node_6.json
bpsec_config=$HDTN_SOURCE_ROOT/config_files/bpsec/ipn7.1_con.json
bprecvstream_config=$config_files/bprecvstream_sctp.json

output_file_path="/home/$USER/test_outputs/lunanet"
# filename=water_bubble_cbr21
# filename=water_bubble_crf18
# filename=lucia_cbr21                 # change this for whatever file you want to name
#filename=lucia_crf18
filename=live_rover
file=$output_file_path/$filename

shm_socket_path_display=/tmp/hdtn_gst_shm_outduct_display
shm_socket_path_filesink=/tmp/hdtn_gst_shm_outduct_filesink

mkdir -p  $output_file_path/$filename


export GST_DEBUG_FILE=/tmp/gst_log.log
#################################################################################
pkill -9 HdtnOneProcessM
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
#################################################################################

# HDTN one process
cd $HDTN_SOURCE_ROOT
#cd /home/$USER/HDTN
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
one_process_pid=$!

sleep 5

#################################################################################
export GST_DEBUG=3,filesink:7,mp4mux:7
export GST_DEBUG_FILE=/tmp/hdtn_gst_log.log
cd $HDTN_RTP_DIR
./build/bprecv_stream  --my-uri-eid=ipn:7.1 --inducts-config-file=$bprecvstream_config --max-rx-bundle-size-bytes 63000 --bpsec-config-file=$bpsec_config \
        --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1460 --outduct-type="appsrc" --shm-socket-path=$shm_socket_path_display &
bprecv_stream_pid=$!
sleep 3
#################################################################################
# if we are using appsrc, launch a separate gstreamer instance to save the video stream to file 
export GST_DEBUG=4,shmsrc:3,filesink:6,rtpjitterbuffer:4
export GST_DEBUG_FILE=/tmp/gst_display_log.log
gst-launch-1.0 shmsrc socket-path=$shm_socket_path_display  is-live=true do-timestamp=true \
        ! queue max-size-time=0 max-size-bytes=0 \
        ! "video/x-raw, format=(string)I420, width=(int)1920, height=(int)1080, framerate=(fraction)30000/1001" \
        ! queue \
        ! timeoverlay halignment=right valignment=bottom text="Stream time:" shaded-background=true font-desc="Sans, 14" \
        ! clockoverlay halignment=left valignment=bottom text="Clock time:" shaded-background=true font-desc="Sans, 14" \
        ! glupload ! glimagesink &
display_pid=$! 

export GST_DEBUG=3,filesink:9,qtmux:3,shmsrc:9
export GST_DEBUG_FILE=/tmp/gst_filesink_log.log
gst-launch-1.0 shmsrc socket-path=$shm_socket_path_filesink is-live=true do-timestamp=true \
        ! queue max-size-time=0 max-size-bytes=0  \
        ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
        ! queue max-size-time=0 max-size-bytes=0 \
        ! rtpjitterbuffer \
        ! rtph264depay \
        ! h264parse \
        ! h264timestamper \
        ! qtmux latency=200000 min-upstream-latency=200000000 moov-recovery-file=/tmp/moov.mrf \
        ! queue max-size-time=0 max-size-bytes=0 \
        ! filesink location=$file/$filename.mp4 -e &
filesink_pid=$!
echo $filesink_pid

sleep 1000
# cleanup
echo "\nkilling HDTN bp receive stream process ..." && kill -2 $bprecv_stream_pid
echo "\n killing both gstreamer processes ..." && kill -2 $filesink_pid && kill -2 $display_pid
echo "\nkilling HDTN one process ..." && kill -2 $one_process_pid

#gst-launch-1.0 qtmoovrecover recovery-input=/tmp/moov broken-input=lucia_crf18.mp4 fixed-output=test.mp4
