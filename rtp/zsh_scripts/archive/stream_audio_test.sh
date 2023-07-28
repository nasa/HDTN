# !/bin/sh 

# path variables
config_files=$HDTN_RTP_DIR/config_files
source_config=$config_files/outducts/streamsource_stcp.json
test_media_folder=/home/kyle/nasa/dev/test_media

file=$test_media_folder/waterslide.mp3


port=60075

cd $HDTN_RTP_DIR


# stream send
./build/bpsend_stream  --bundle-size=100000 --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$source_config \
        --max-incoming-udp-packet-size-bytes=1600 --incoming-rtp-stream-port=$port --num-circular-buffer-vectors=250\
        --enable-rtp-concatenation=false & 
media_source_process=$!

sleep 1

#test signal
# ffmpeg -re -f lavfi -i aevalsrc="sin(400*2*PI*t)" -ar 8000 -f mulaw -f rtp "rtp://127.0.0.1:$port" &
ffmpeg -re -i $file -vn -c:a aac -b:a 96k -f flv -f rtp "rtp://127.0.0.1:$port" &



sleep 400

echo "\nkilling media source ..." && kill -2 $media_source_process
