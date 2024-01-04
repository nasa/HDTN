@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1bpencap_namedpipe_flowid2.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\router\contact_plans\contactPlanCutThroughMode_unlimitedRate.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "Encap Repeater" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\encap_repeater\encap-repeater.exe" "--stream-name-0=\\.\pipe\bp_local_pipe" "--stream-init-0=create" "--stream-name-1=\\.\pipe\bp_local_pipe2" "--stream-init-1=open" "--queue-size=5" "--encap-packet-type=bp"
timeout /t 3
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_bpencap_namedpipe2.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 6
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=0" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--duration=20" "--bundle-size=100000" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_stcp_port4556.json"
