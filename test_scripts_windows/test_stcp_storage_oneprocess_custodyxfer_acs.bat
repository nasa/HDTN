@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2_bpgencustodyxfer.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
CALL activate.bat
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--acs-aware-bundle-agent" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_stcp_port4558.json" "--custody-transfer-outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpsink_one_stcp_port4556.json"
timeout /t 3
REM START "Send Release" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--flow-id=1" "--delay-before-send=20" "%HDTN_CONFIG_FILE_PARAM%"
REM START "Send Release Multi" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-multiple-release-message-sender.exe" "--events-file=releaseMessagesCustodyXfer.json" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 1
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "Send Release" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--dest-uri-eid-to-release-or-stop=ipn:1.0" "--delay-before-send=3" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--custody-transfer-use-acs" "--bundle-rate=100" "--duration=5" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_stcp_port4556.json" "--custody-transfer-inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpgen_one_stcp_port4560.json"
START "Send Release" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--dest-uri-eid-to-release-or-stop=ipn:2.1" "--delay-before-send=6" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 8



