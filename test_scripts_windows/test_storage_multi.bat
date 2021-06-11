@Echo off

CALL C:\Users\btomko\Anaconda3\Scripts\activate.bat C:\Users\btomko\Anaconda3
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_tcpcl_port4557.json"
timeout /t 3
START "BpSink2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\egress_two_tcpcl_port4557flowid1_port4558flowid2.json"
timeout /t 3
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe" "--always-send-to-storage" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\ingress_one_tcpcl_port4556.json"
timeout /t 3
START "Send Release Multi" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-multiple-release-message-sender.exe" "--events-file=releaseMessages1.json"
timeout /t 1
START "Storage" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-storage.exe" "--storage-config-json-file=%HDTN_SOURCE_ROOT%\module\storage\unit_tests\storageConfig.json"
timeout /t 3
START "BpGen2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=5" "--flow-id=2" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_tcpcl_port4556.json"
timeout /t 1
START "BpGen1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=3" "--flow-id=1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_tcpcl_port4556.json"
timeout /t 8





