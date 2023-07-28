#!/bin/sh

# path variables
config_files=$HDTN_RTP_DIR/config_files

video_sink_config=$config_files/inducts/mediasink_stcp.json
# video_sink_config=$config_files/inducts/mediasink_stcp.json
# audio_sink_config=$config_files/inducts/mediasink_stcp_5004.json

outgoing_rtp_video=60004
outgoing_rtp_audio=60006


#play                                      
ffplay stream_av_file.sdp -protocol_whitelist file,udp,rtp -fflags nobuffer+fastseek+flush_packets -sync ext -flags low_delay -framedrop -i  &
ffplay_id=$!

sleep 1

cd $HDTN_RTP_DIR

# video recv
./build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$video_sink_config  --outgoing-rtp-hostname=192.168.1.132 \
        --outgoing-rtp-port=$outgoing_rtp_video --num-circular-buffer-vectors=50 --max-outgoing-rtp-packet-size-bytes=1472 &
video_recv_process=$!               

#audio recv
# ./build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$audio_sink_config  --remote-hostname=192.168.1.132 \
#         --outgoing-rtp-port=$outgoing_rtp_audio --num-circular-buffer-vectors=50 --max-outgoing-rtp-packet-size-bytes=1472 &
# audio_recv_process=$!   


# cleanup
sleep 360
# echo "\nk
echo "\nkilling HDTN bp receive video process ..." && kill -2 $video_recv_process
echo "\nkilling HDTN bp receive audio process ..." && kill -2 $audio_recv_process
echo "\nkilling ffplay process ..." && kill -2 $ffplay_id