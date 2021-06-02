@Echo off
START "Receive File" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\common\ltp\ltp-file-transfer.exe" "--receive-file=received.bin" "--dest-udp-port=12345" "--this-ltp-engine-id=100" "--one-way-light-time-ms=1000" "--one-way-margin-time-ms=200" "--estimated-rx-filesize=1000000000"
timeout /t 3
START "Send File" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\common\ltp\ltp-file-transfer.exe" "--send-file=a.EXE" "--dest-udp-hostname=localhost" "--dest-udp-port=12345" "--this-ltp-engine-id=200" "--remote-ltp-engine-id=100" "--ltp-data-segment-mtu=60000" "--one-way-light-time-ms=1000" "--one-way-margin-time-ms=200" "--client-service-id=400"
