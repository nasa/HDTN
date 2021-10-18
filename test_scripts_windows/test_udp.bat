@Echo off

REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
CALL activate.bat
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\inducts\bpsink_one_udp_port4558.json"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1udp_port4556_egress1udp_port4558flowid2_0.8Mbps.json"
timeout /t 3
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe" "--cut-through-only-test" "--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1udp_port4556_egress1udp_port4558flowid2_0.8Mbps.json"
timeout /t 3
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\outducts\bpgen_one_udp_port4556_0.5Mbps.json"
