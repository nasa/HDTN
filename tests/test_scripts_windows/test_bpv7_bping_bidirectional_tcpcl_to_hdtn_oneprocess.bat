@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%"
timeout /t 6
START "BPing" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bping.exe" "--use-bp-version-7" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:10.2047" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_tcpcl_port4556_allowrxcustodysignals.json"


