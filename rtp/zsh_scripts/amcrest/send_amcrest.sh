
# from the user manual. if the ip of the camera changes, this must be changed. 
    # port matches the rtp port on the webcam settings
rtsp_stream_location="rtsp://10.1.1.250:554/cam/realmonitor?channel=1&subtype=0&unicast=false"

export GST_DEBUG_DUMP_DOT_DIR=/tmp

gst-launch-1.0 rtspsrc location=$rtsp_stream_location user-id=admin user-pw=hdtn4nasa \
! "application/x-rtp,media=(string)video,payload=(int)96,clockrate=(int)90000" \
! queue \
! rtph264depay ! h264parse \
! avdec_h264 ! videoconvert \
! videoscale ! video/x-raw,width=3840,height=2160 ! glimagesink &
# ! udpsink host="127.0.0.1" port=45555 &

#  "application/x-rtp media=video,payload=96,clockrate=90000,encoding-name=H264,packetization-mode=1,profile-level-id=640033,a-packetization-support=DH,a-framerate=30.0"
sleep 60

