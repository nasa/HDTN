# !/bin/zsh 
test_files=/home/$USER/nasa/dev/test_media/official_test_media
file=$test_files/lucia_crf18.mp4

pkill -9 gst-launch-1.0 
#recv
#decodebin ! videoconvert !  fpsdisplaysink text-overlay=true &
# gst-launch-1.0 -e -v udpsrc  port=5000 \
# ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96"\
# !  rtph264depay ! h264parse ! queue ! mp4mux ! filesink location=lucia_crf18.mp4 -e & 
export GST_DEBUG=3,mp4mux:8

gst-launch-1.0 -v udpsrc port=5000 \
! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
! rtpjitterbuffer ! rtph264depay ! h264parse ! queue ! mp4mux ! filesink location=lucia_crf18.mp4 -e & 
#  decodebin ! videoconvert ! autovideosink sync=false & # fakesink silent=false & #filesink location=xyz.mp4 -e \

# gst-launch-1.0 -v udpsrc port=5000 \
# ! fakesink silent=false &
# ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96"\

recv_pid=$!

sleep 2

#send

sleep 65 
kill -2 $recv_pid

