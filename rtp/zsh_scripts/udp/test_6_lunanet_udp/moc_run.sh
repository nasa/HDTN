#!/bin/sh
#run this on moc 
# path variables
config_files=$HDTN_RTP_DIR/config_files/luna_net
bprecvstream_config=$config_files/bprecvstream_from_cso.json
contact_plan=$HDTN_RTP_DIR/config_files/contact_plans/LunaNetContactPlanNodeIDs.json

outgoing_rtp_port=60000

cd $HDTN_RTP_DIR 
# # # receive stream and save to file
# ffmpeg -y -protocol_whitelist file,udp,rtp \
#         -strict experimental \
#         -fflags +genpts \
#         -seek2any 1 \
#         -avoid_negative_ts +make_zero \
#         -max_delay 500 \
#         -reorder_queue_size 5 \
#         -loglevel verbose \
#         -i stream_file.sdp -vcodec copy -acodec copy -f mp4 test_output_LTP.mp4 & 


./build/bprecv_stream  --my-uri-eid=ipn:7.1 --inducts-config-file=$sink_config  --outgoing-rtp-hostname=127.0.0.1 \
        --outgoing-rtp-port=$outgoing_rtp_port --num-circular-buffer-vectors=500 --max-outgoing-rtp-packet-size-bytes=1472 \
        --ffmpeg-command="\
        ffmpeg -y -protocol_whitelist file,udp,rtp,data \
        -strict experimental \
        -fflags +genpts \
        -reorder_queue_size 0 \
        -loglevel verbose \
        -vcodec copy -acodec copy \
        -f sap sap://224.0.0.255?same_port=1" &
       
sleep 5

# ffmpeg -y -protocol_whitelist file,udp,rtp \
#         -strict experimental \
#         -fflags +genpts \
#         -seek2any 1 \
#         -avoid_negative_ts +make_zero \
#         -reorder_queue_size 0 \
#         -loglevel verbose \
#         -fflags nobuffer+fastseek+flush_packets -flags low_delay \
#         -re -i HDTN_TO_IN_SDP.sdp \
#         -vcodec copy -acodec copy \
#         -f sap sap://224.0.0.255?same_port=1" &



# --ffmpeg-command="\
#         ffplay -i  -protocol_whitelist data,file,udp,rtp  -reorder_queue_size 0  -fflags nobuffer+fastseek+flush_packets -sync ext -flags low_delay " &
        # ffmpeg -y -protocol_whitelist data,file,udp,rtp \
        # -strict experimental \
        # -fflags +genpts \
        # -seek2any 1 \
        # -avoid_negative_ts +make_zero \
        # -reorder_queue_size 0 \
        # -max_delay 0 \
        # -loglevel verbose \
        # -i  -vcodec copy -acodec copy -f mp4 test_water_stcp.mp4"
# stream_recv_id=$!               

# ffmpeg -y -protocol_whitelist data,file,udp,rtp -reorder_queue_size 0 -i HDTN.sdp -f rawvideo -pixel_format yuv422p10le -video_size 3840x2160 output.raw #- | ffplay -protocol_whitelist data,file,udp,rtp,fd -f rawvideo -pixel_format yuv422p10le -video_size 3840x2160 -i -


#  -i water_raw.raw

# ffmpeg -hwaccel cuda -hwaccel_output_format cuda  -protocol_whitelist data,file,udp,rtp -re -listen_timeout -1  -i  HDTN.sdp -c:v copy -reorder_queue_size 0  -fflags nobuffer+fastseek+flush_packets -f matroska - | ffplay -i -   -sync ext -flags low_delay -framedrop

# cleanup
sleep 360
# echo "\nk

echo "\nkilling HDTN bp receive stream process ..." && kill -2 $stream_recv_id
echo "\nkillingffplay process ..." && kill -2 $ffplay_id