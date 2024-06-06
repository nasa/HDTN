@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\router\contact_plans\contactPlanCutThroughMode_unlimitedRate.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "BpFileTransfer Rx" /D "C:/hdtn_tmp_received" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpfiletransfer.exe" "--port-number=8088" "--save-directory=.\received" "--my-uri-eid=ipn:2.1" "--dest-uri-eid=ipn:1.1" "--force-disable-custody" "--custody-transfer-inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_tcpcl_port4558.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%"
timeout /t 6
REM ONE FILE TEST NEXT LINE
REM START "BpSendFile" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsendfile.exe" "--max-bundle-size-bytes=4000000" "--file-or-folder-path=.\a.EXE" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556.json"
REM DIRECTORY TEST NEXT LINE (directories not recursive)
START "BpFileTransfer Tx" /D "C:/hdtn_tmp" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpfiletransfer.exe" "--port-number=8089" "--recurse-directories-depth=3" "--upload-new-files" "--max-tx-bundle-size-bytes=4000000" "--file-or-folder-path-tx=." "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--force-disable-custody" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556_allowrxcustodysignals.json"
