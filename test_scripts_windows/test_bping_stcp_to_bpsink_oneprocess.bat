@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2_bpgencustodyxfer.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
CALL activate.bat
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
REM START BPSINK SO ALL EGRESS OUTDUCTS CAN CONNECT EVEN THOUGH JUST PINGING HDTN
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--acs-aware-bundle-agent" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_stcp_port4558.json" "--custody-transfer-outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpsink_one_stcp_port4556.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "Send Release" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--dest-uri-eid-to-release-or-stop=ipn:1.1" "--delay-before-send=3" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "Send Release" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--dest-uri-eid-to-release-or-stop=ipn:2.2047" "--delay-before-send=3" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "BPing" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bping.exe" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.2047" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_stcp_port4556.json" "--custody-transfer-inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpgen_one_stcp_port4560.json"


