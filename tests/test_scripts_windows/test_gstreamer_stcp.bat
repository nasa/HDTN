@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\router\contact_plans\contactPlanCutThroughMode_unlimitedRate.json"
SET outgoing_rtp_port_to_bpsend_stream=6565
SET VIDEO_FILE="C:/Users/btomko/Downloads/trailer_480p.mov"
SET PATH=%PATH%;C:\gstreamer\1.0\msvc_x86_64\runtime\bin


START "GStreamer Save" "cmd /k" "gst-launch-1.0" "-e" "udpsrc" "address=127.0.0.1" "port=8554" "!" "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" "!" "rtph264depay" "!" "queue" "!" "h264parse" "!" "qtmux" "!" "filesink" "location=received.mp4"
timeout /t 1
START "BpReceiveStream" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bprecv_stream.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_stcp_port4558.json" "--max-rx-bundle-size-bytes=70000" "--num-circular-buffer-vectors=10000" "--max-outgoing-rtp-packet-size-bytes=1460" "--outduct-type=udp" "--outgoing-rtp-port=8554" "--outgoing-rtp-hostname=127.0.0.1"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 3
START "BpSendStream" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsend_stream.exe" "--use-bp-version-7" "--bundle-rate=0" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--bundle-size=200000" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_stcp_port4556.json" "--induct-type=udp" "--max-incoming-udp-packet-size-bytes=1800" "--num-circular-buffer-vectors=3000" "--rtp-packets-per-bundle=20" "--incoming-rtp-stream-port=%outgoing_rtp_port_to_bpsend_stream%"
timeout /t 3
START "GStreamer" "cmd /k" "gst-launch-1.0" "-e" "filesrc" "location=%VIDEO_FILE%" "!" "qtdemux" "!" "h264parse" "!" "rtph264pay" "config-interval=1" "!" "udpsink" "host=127.0.0.1" "port=%outgoing_rtp_port_to_bpsend_stream%"
