@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1udp_port4556_egress1udp_port4558flowid2_0.8Mbps.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\scheduler\src\contactPlanCutThroughMode.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_udp_port4558.json"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "Scheduler" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\scheduler\hdtn-scheduler.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 4
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=0" "--bundle-size=1000" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--duration=10" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_udp_port4556_0.5Mbps.json"
