@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json --hdtn-distributed-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_distributed_defaults.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\scheduler\src\contactPlanCutThroughModeStartAfter15Sec.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "BpSink1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:1.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_tcpcl_port4557.json"
timeout /t 3
START "BpSink2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Storage" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\storage\hdtn-storage.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 3
START "Scheduler" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\scheduler\hdtn-scheduler.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 2
START "Router" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\router\hdtn-router.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 4
START "Telem" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\telem_cmd_interface\telem_cmd_interface.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 4
START "BpGen2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=5" "--my-uri-eid=ipn:102.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556.json"
timeout /t 1
START "BpGen1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--duration=3" "--my-uri-eid=ipn:101.1" "--dest-uri-eid=ipn:1.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556.json"
