#!/bin/sh

# path variables
config_files=$HDTN_RTP_DIR/config_files
sink_config=$config_files/inducts/mediasink_stcp.json

outgoing_rtp_port=60000
                                                     
#https://stackoverflow.com/questions/38367910/streaming-webcam-via-rtp-protocol
ffplay   -vcodec hevc_cuvid  webcam_sdp.sdp  -protocol_whitelist file,udp,rtp -analyzeduration 30000000 -probesize 1000000 \
        -flags low_delay -framedrop  -fflags nobuffer  -loglevel verbose -stats -fast & 
sleep 5

cd $HDTN_RTP_DIR

# Media app start. Media app inherits from BpSinkPattern and functions very similarly to bpreceive file
./build/bprecv_stream  --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config  --outgoing-rtp-hostname=192.168.1.132 \
        --outgoing-rtp-port=$outgoing_rtp_port --num-circular-buffer-vectors=50 --max-outgoing-rtp-packet-size-bytes=1472
stream_recv_id=$!
sleep 3                

# cleanup
sleep 360

echo "\nkilling HDTN bp receive stream process ..." && kill -2 $stream_recv_id
echo "\nkillingffplay process ..." && kill -2 $ffplay_id