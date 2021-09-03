@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2_bpgencustodyxfer.json"
CALL C:\Users\btomko\Anaconda3\Scripts\activate.bat C:\Users\btomko\Anaconda3
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "Send Release" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--flow-id=1" "--delay-before-send=20" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 1
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "--always-send-to-storage" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--custody-transfer-use-acs" "--bundle-rate=100" "--duration=5" "--flow-id=2" "--my-node-id=1" "--my-service-id=1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_tcpcl_port4556.json" "--custody-transfer-inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpgen_one_tcpcl_port4560.json"
timeout /t 8



