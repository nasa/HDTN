@Echo off
SET HDTN_CONFIG_FILE_PARAM="--hdtn-config-file=%HDTN_SOURCE_ROOT%\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json"
SET CONTACT_PLAN_FILE_PARAM="--contact-plan-file=%HDTN_SOURCE_ROOT%\module\scheduler\src\contactPlanCutThroughMode.json"
SET WEB_SSL_CERT="--gui-certificate-pem-file=C:\hdtn_ssl_certificates\cert_ipn10.0.pem"
SET WEB_SSL_KEY="--gui-private-key-pem-file=C:\hdtn_ssl_certificates\privatekey_ipn10.0.pem"
SET WEB_SSL_DH="--gui-dh-pem-file=C:\hdtn_ssl_certificates\dh4096_ipn10.0.pem"
REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
REM CALL activate.bat
START "BpSink" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--my-uri-eid=ipn:2.1" "--inducts-config-file=%HDTN_SOURCE_ROOT%\config_files\inducts\bpsink_one_stcp_port4558.json"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "%HDTN_CONFIG_FILE_PARAM%" "%CONTACT_PLAN_FILE_PARAM%" "%WEB_SSL_CERT%" "%WEB_SSL_KEY%" "%WEB_SSL_DH%"
timeout /t 6
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=0" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--duration=10" "--bundle-size=50" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_stcp_port4556.json"
