# !/bin/zsh 
test_files=/home/$USER/nasa/dev/test_media/official_test_media
file=$test_files/lucia_crf18.mp4

# echo $file
# gst-launch-1.0 -v videotestsrc ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast ! rtph264pay ! udpsink port=5000 host=127.0.0.1 &

gst-launch-1.0 -v filesrc location=$file !  progressreport update-freq=1 ! qtdemux ! h264parse ! rtph264pay config-interval=1  ! udpsink host="127.0.0.1" port=5000 &
# gst-launch-1.0 -v filesrc location=$file  ! progressreport update-freq=1 ! qtdemux ! h264parse ! rtph264pay  ! udpsink host=$HOST port=5000 

# test video to mp4 (WORKS)
# gst-launch-1.0 -v videotestsrc ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast \
# ! queue ! mp4mux ! filesink location=videotestsrc.mp4 -e

# test video rtp to x-rtp to mp4 (WORKS)
# gst-launch-1.0 -v videotestsrc ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast \
# ! rtph264pay ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" ! rtph264depay ! h264parse \
# ! queue ! mp4mux ! filesink location=videotestsrc.mp4 -e



send_pid=$!

sleep 65

kill -2 $send_pid