@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\scheduler\src\contactPlanCutThroughMode.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "BpReceiveFile" /D "C:/hdtn_tmp_received" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpreceivefile.exe" "--save-directory=.\received" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "Scheduler" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\scheduler\hdtn-scheduler.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 4
REM ONE FILE TEST NEXT LINE
REM START "BpSendFile" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsendfile.exe" "--max-bundle-size-bytes=4000000" "--file-or-folder-path=.\a.EXE" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556.json"
REM DIRECTORY TEST NEXT LINE (directories not recursive)
START "BpSendFile" /D "C:/hdtn_tmp_snd" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsendfile.exe" "--max-bundle-size-bytes=4000000" "--file-or-folder-path=." "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556.json"
