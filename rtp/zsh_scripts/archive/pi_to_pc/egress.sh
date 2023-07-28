# !/bin/zsh 

config_files=$HDTN_RTP_DIR/config_files/pi_to_pc
# source_config=$config_files/ltp_media_source.json
source_config=$config_files/mediasource_stcp.json
# source_config=$config_files/mediasource_tcpcl.json

incoming_rtp_port=40002
raspberrypi_ip=127.0.0.1

cd $HDTN_RTP_DIR

ffmpeg -y -sdp_file HDTN.sdp -re -i $file -c copy -an -f rtp "rtp://$raspberrypi_ip:$incoming_rtp_port" 

./build/bpsend_stream  --bundle-size=2000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
        --max-incoming-udp-packet-size-bytes=1600 --incoming-rtp-stream-port=$incoming_rtp_port --num-circular-buffer-vectors=500 \
        --enable-rtp-concatenation=false --sdp-filepath="HDTN.sdp" &
media_source_process=$!