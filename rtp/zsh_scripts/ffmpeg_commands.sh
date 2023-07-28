# send a file using nvidia hardware  h265 encoding to an rtp stream. -re to enable real time mode
ffmpeg -hwaccel cuda -hwaccel_output_format cuda  -re  -i test_media/ISS_View_of_Planet_Earth_2160p.mp4 \
                -c:a copy -c:v hevc_nvenc -f rtp "rtp://127.0.0.1:50574"
#receive that file using ffplay -> copy the SDP text from the output of the above command into a file called file_sdp.sdp then run
ffplay file_sdp.sdp  -protocol_whitelist file,udp,rtp

# send webcam using nvidia hardware h265 encoding to an rtp stream
 ffmpeg -hwaccel cuda -hwaccel_output_format cuda  \
    -f v4l2 -framerate 30 -video_size 1280x720 -i /dev/video0 \
    -c:v hevc_nvenc -preset p1 -tune ull  -muxpreload 0 -muxdelay 0 -rc cbr -cbr true \
    -f rtp "rtp://127.0.0.1:60000"


##eceive that stream using ffplay -> copy the SDP text from the output of the above command into a file called webcame_sdp.sdp then run
ffplay webcam_sdp.sdp -protocol_whitelist file,udp,rtp



#  get all supported formats
v4l2-ctl --list-formats-ext 

#nvidia codec options
https://gist.github.com/nico-lab/c2d192cbb793dfd241c1eafeb52a21c3

# for psnr and ssim ensure that the videos are same framerate and scaled correctly

# psnr
ffmpeg \
    -i transmitted.mp4 \
    -i original.mp4 \
    psnr="psnr.log" \ 
    -f null -

# ssim
ffmpeg \
    -i transmitted.mp4 \
    -i original.mp4 \
    ssim="ssim.log" \ 
    -f null -

# get last 60 seconds of  file into new file
ffmpeg -sseof -60 -i A012C002H2201038S_CANON_01-Surface_Tension.MXF -c copy tesntion_out.MKV            
ffmpeg -ss 00:04:18 -i A012C002H2201038S_CANON_01-Surface_Tension.MXF -t 60 -c copy trimmed.MKV


# ffmpeg -y -hwaccel cuda -hwaccel_output_format cuda  \
# -r 60000/1001  -i water_bubble.mp4 -map 0 \
# -c:v h264_nvenc -rc cbr   -no-scenecut true  -vf format=yuv420p -c:a copy  water_bubbles_h264_cbr_hq.mp4
ffmpeg -y -i water_bubble.mp4 -map 0:v -c:v libx264 -crf 18 -c:a copy water_bubbles_h264_cbr_hq.mp4

# h265 to h264 constant bit rate 10 bit
ffmpeg -y -r 60000/1001  -threads 0 -i water_bubble.mp4 -map 0:v -c:v libx264 -slice-max-size 1460 -no-scenecut  true -g 60  -crf 18 -c:a copy water_bubble_h264_crf_18.mp4
ffmpeg -y -r 60000/1001  -threads 0 -i water_bubble.mp4 -c:v libx264 -b:v 70M -maxrate 75.0M -bufsize 20M o_water_bubble_h264_g_30_br75.mp4

# crf h264
ffmpeg -i lucia.mp4 -c:v libx264 -slice-max-size 1460  -no-scenecut  true -g 60  -crf 18  -c:a copy lucia_crf18.mp4      
#crf h264 smaller g
ffmpeg -i lucia.mp4 -c:v libx264 -slice-max-size 1460  -no-scenecut  true -g 15  -crf 18  -c:a copy lucia_crf18_g_15.mp4      

# vbr h264
ffmpeg -y -r 60000/1001  -threads 0 -i lucia_vbr.mp4 -c:v libx264 -g 15 -slice-max-size 1460  -no-scenecut  true -b:v 110M -maxrate 50.0M -bufsize 100M lucia_vbr_g_15.mp4


# h265 to h254 variable bit rate 10 bit twoo pass
ffmpeg -y -r 60000/1001  -threads 0 -i water_bubble.mp4 -c:v libx264 -b:v 110M -maxrate 130.0M -bufsize 100M -pass 1 -f mp4 /dev/null && \
ffmpeg -y -r 60000/1001  -threads 0 -i water_bubble.mp4 -c:v libx264 -b:v 110M -maxrate 130.0M -bufsize 100M -pass 2 water_bubble_h264_vbr.mp4

# print info about particular encoder
ffmpeg h264_nvenc -h

# get length of video in seconds
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 input.mp4

ffmpeg -i transmitted.mp4 -ss DELTA_TIME -i original.mp4 -lavfi psnr=stats_file=psnr_logfile.txt -f null - 

# convert flv to flac
ffmpeg -i AmmoniaLeak.flv AmmoniaLeak.flac

# gst rtp stream
gst-launch-1.0 -v filesrc location=water_bubble_h264_vbr.mp4 ! qtdemux ! h264parse ! rtph264pay config-interval=4 ! udpsink host=127.0.0.1 port=5000

# gst save to file
 gst-launch-1.0 -v -e udpsrc port=5000 ! "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
! rtph264depay ! h264parse !  mp4mux ! filesink location=xyz.mp4

# gst play file 
gst-launch-1.0 filesrc location=xyz.mp4 ! decodebin name=dec ! videoconvert ! autovideosink dec. ! audioconvert ! audioresample ! alsasink


# gst to gst 
# gst-launch-1.0 filesrc location=$file ! qtdemux ! h264parse ! rtph264pay config-interval=4 ! rtph264depay ! h264parse !  mp4mux !  filesink location=$filename.mp4  -e  # ! udpsink sync=true host=127.0.0.1 port=6000

# gst rtp loopback
gst-launch-1.0 filesrc location=$file ! qtdemux ! h264parse !\
rtph264pay config-interval=4   !\
rtpstreampay ! rtpstreamdepay !\
rtph264depay  ! h264parse ! appsink !  mp4mux !  filesink location=$filename.mp4  -e 

# gst tcp to bp send
# gst-launch-1.0 filesrc location=$file ! qtdemux ! h264parse ! rtph264pay config-interval=4 !  rtpstreampay !  tcpserversink sync=true port=$incoming_rtp_port  &

# vbr encoding max bit rate
https://trac.ffmpeg.org/wiki/Limiting%20the%20output%20bitrate
"Based on the -bufsize option, ffmpeg will calculate and correct
the average bit rate produced. If we didn't specify -bufsize, 
these intervals could be significantly longer than we would want. 
This would cause the current bit rate to frequently jump a lot over and below
the specified average bit rate and would cause an unsteady output bit rate.
If we specify a smaller -bufsize, ffmpeg will more frequently check for the
output bit rate and constrain it to the specified average bit rate from the 
command line. Hence, lowering -bufsize lowers the bitrate variation that the 
encoder can produce."


# bitrate over time where c is the time interval
ffmpeg-bitrate-stats -v -a time -of csv -c 0.5 -s video water_bubble.mp4 > water_bubble_bitrate.csv



# constant quality (variable bit rate)
ffmpeg -i lucia_original.mp4 -c:v libx264 -slice-max-size 1460  -no-scenecut  true -g 15 -crf 18 lucia_crf18.mp4

# constant bitrate (constrained encoding)
ffmpeg -i lucia_original.mp4 -c:v libx264 -slice-max-size 1460  -no-scenecut  true -g 15 -crf 21 -maxrate 20M -bufsize 2M lucia_cbr21.mp4


# constant quality (variable bit rate)
ffmpeg -i water_bubble.mp4 -c:v libx264 -slice-max-size 1460  -no-scenecut  true -g 15 -crf 18 water_bubble_crf18.mp4

# constant bitrate (constrained encoding)
ffmpeg -i lucia_original.mp4 -c:v libx264 -slice-max-size 1460  -no-scenecut  true -g 15 -crf 21 -maxrate 30M -bufsize 2M water_bubble_cbr21.mp4