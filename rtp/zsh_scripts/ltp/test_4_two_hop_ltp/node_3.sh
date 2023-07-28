#!/bin/sh
# path variables
config_files=$HDTN_RTP_DIR/config_files
video_sink_config=$config_files/two_hop_ltp/node_3_video_sink.json
hdtn_node_3_config=$config_files/two_hop_ltp/node_3.json
contact_plan=$HDTN_RTP_DIR/config_files/two_hop_ltp/contact_plan.json


output_path=/home/$USER/test_outputs/test_4
filename=lucia_crf18
mkdir $output_path



outgoing_rtp_port=60000




# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_node_3_config &
pid_hdtn=$!

sleep 3


cd $HDTN_RTP_DIR 

./build/bprecv_stream  --my-uri-eid=ipn:3.2 --inducts-config-file=$sink_config  --outgoing-rtp-hostname=127.0.0.1 \
        --outgoing-rtp-port=$outgoing_rtp_port --num-circular-buffer-vectors=10000 --max-outgoing-rtp-packet-size-bytes=1560 \
        --ffmpeg-command="\
        ffmpeg -y -protocol_whitelist file,udp,rtp,data \
        -strict experimental \
        -fflags +genpts \
        -reorder_queue_size 0 \
        -loglevel verbose \
        -vcodec copy -acodec copy \
        -f mp4 $filename.mp4" &

recv_pid=$!


sleep 400

kill -2 $recv_pid
kill -2 $pid_hdtn

        # -f sap sap://224.0.0.255?same_port=1" &
       