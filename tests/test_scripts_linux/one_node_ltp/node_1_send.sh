# !/bin/sh 

config_files=$HDTN_RTP_DIR/tests/test_scripts_linux/one_node_ltp/config_files
video_url=http://images-assets.nasa.gov/video/A1Launch/A1Launch~orig.mp4
video_file=/media/nasa_video.mp4
incoming_rtp_port=30000

# Download video file if it doesn't exist
if [ ! -f $video_file ]; then
    wget -O $video_file $video_url
fi

# Launch bpsend_stream to process RTP packets and send to HDTN as bundles
$HDTN_STREAMING_DIR/build/bpsend_stream  --bundle-size=200000  --bundle-rate=0 --use-bp-version-7 \
        --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$config_files/mediasource_ltp.json \
        --induct-type="appsink" --file-to-stream=$video_file \
        --max-incoming-udp-packet-size-bytes=1800 --num-circular-buffer-vectors=3000 \
        --rtp-packets-per-bundle=20 &
bpsend_process=$!

sleep 1000
kill -2 $gst_process
kill -2 $bpsend_process
