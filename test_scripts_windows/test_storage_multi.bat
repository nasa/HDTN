@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
CALL activate.bat
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:1.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_tcpcl_port4557.json"
timeout /t 3
START "BpSink2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Send Release Multi" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-multiple-release-message-sender.exe" "--events-file=releaseMessages1.json" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 1
START "Storage" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-storage.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "BpGen2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=5" "--my-uri-eid=ipn:102.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_tcpcl_port4556.json"
timeout /t 1
START "BpGen1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=3" "--my-uri-eid=ipn:101.1" "--dest-uri-eid=ipn:1.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_tcpcl_port4556.json"
timeout /t 8





