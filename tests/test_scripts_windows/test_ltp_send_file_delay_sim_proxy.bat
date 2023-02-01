@Echo off
START "UDP delay sim For Sender" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\module\udp_delay_sim\udp-delay-sim.exe" "--remote-udp-hostname=localhost" "--remote-udp-port=1113" "--my-bound-udp-port=1115" "--num-rx-udp-packets-buffer-size=10000" "--max-rx-udp-packet-size-bytes=65000" "--send-delay-ms=0"
timeout /t 3
START "UDP delay sim For Receiver" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\module\udp_delay_sim\udp-delay-sim.exe" "--remote-udp-hostname=localhost" "--remote-udp-port=1114" "--my-bound-udp-port=1116" "--num-rx-udp-packets-buffer-size=100" "--max-rx-udp-packet-size-bytes=65000" "--send-delay-ms=0"
timeout /t 3
START "Receive File" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\common\ltp\ltp-file-transfer.exe" "--receive-file=received.bin" "--remote-udp-port=1116" "--my-bound-udp-port=1113" "--this-ltp-engine-id=100" "--remote-ltp-engine-id=200" "--one-way-light-time-ms=1000" "--one-way-margin-time-ms=200" "--estimated-rx-filesize=1000000000" "--dont-save-file" "--random-number-size-bits=64"
timeout /t 3
START "Send File" /D "C:/hdtn_tmp_snd" "cmd /k" "%HDTN_BUILD_ROOT%\common\ltp\ltp-file-transfer.exe" "--send-file=a.EXE" "--remote-udp-hostname=localhost" "--remote-udp-port=1115" "--my-bound-udp-port=1114" "--this-ltp-engine-id=200" "--remote-ltp-engine-id=100" "--ltp-data-segment-mtu=60000" "--one-way-light-time-ms=1000" "--one-way-margin-time-ms=200" "--client-service-id=1" "--checkpoint-every-nth-tx-packet=100" "--random-number-size-bits=64"
