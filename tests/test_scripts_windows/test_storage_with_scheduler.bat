@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json --hdtn-distributed-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_distributed_defaults.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
REM on 10 off 20 on 30 off 40
START "Scheduler" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\scheduler\hdtn-scheduler.exe" "--contact-plan-file=contactPlanIpn2.1.json" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 1
START "Router" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\router\hdtn-router.exe" "%HDTN_CONFIG_FILE_PARAM%" "--contact-plan-file=contactPlanIpn2.1.json"
timeout /t 2
START "Telem" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\telem_cmd_interface\telem_cmd_interface.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 4
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Storage" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-storage.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=15" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556.json"
timeout /t 8





