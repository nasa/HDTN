@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\scheduler\src\contactPlanCutThroughMode.json"
REM must run from HDTN_SOURCE_ROOT for bpsec key file relative paths
START "BpSink" /D "%HDTN_SOURCE_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_stcp_port4558.json" "--bpsec-config-file=%HDTN_SOURCE_ROOT%\config_files\bpsec\ipn2.1_con_plus_int.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_SOURCE_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%" "--bpsec-config-file=%HDTN_SOURCE_ROOT%\config_files\bpsec\ipn10.1_con_plus_int.json"
timeout /t 3
START "BpGen" /D "%HDTN_SOURCE_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--use-bp-version-7" "--bundle-rate=0" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--duration=10" "--bundle-size=100000" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_stcp_port4556.json" "--bpsec-config-file=%HDTN_SOURCE_ROOT%\config_files\bpsec\ipn1.1_con_plus_int.json"
