
gst-launch-1.0 udpsrc port=45555 \
! queue \
! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
! rtph264depay ! h264parse \
! avdec_h264 ! videoconvert \
! videoscale ! video/x-raw,width=3840,height=2160 ! glimagesink &

sleep 60

