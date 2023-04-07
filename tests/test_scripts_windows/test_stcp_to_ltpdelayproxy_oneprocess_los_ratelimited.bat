@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1ltpdelayproxy_flowid2_50Mbps.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\scheduler\src\contactPlanCutThroughMode_50Mbps.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
REM start los 17 seconds in because 1 second ltp ping will start los counter
START "UDP delay sim For Sender" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\module\udp_delay_sim\udp-delay-sim.exe" "--remote-udp-hostname=localhost" "--remote-udp-port=4558" "--my-bound-udp-port=1114" "--num-rx-udp-packets-buffer-size=10000" "--max-rx-udp-packet-size-bytes=65000" "--send-delay-ms=0" "--los-start-ms=17000" "--los-duration-ms=10000"
timeout /t 3
START "UDP delay sim For Receiver" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\module\udp_delay_sim\udp-delay-sim.exe" "--remote-udp-hostname=localhost" "--remote-udp-port=1113" "--my-bound-udp-port=4556" "--num-rx-udp-packets-buffer-size=100" "--max-rx-udp-packet-size-bytes=65000" "--send-delay-ms=0" "--los-start-ms=17000" "--los-duration-ms=10000"
timeout /t 3
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_ltp_delaysim.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 6
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--duration=40" "--bundle-size=100000" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_stcp_port4556.json"
