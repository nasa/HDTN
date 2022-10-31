@Echo off
START "BpGen" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=0" "--my-uri-eid=ipn:1.1" "--dest-uri-eid=ipn:2.1" "--duration=10" "--bundle-size=100000" "--outducts-config-file=%HDTN_SOURCE_ROOT%\config_files\outducts\bpgen_one_stcp_port4556.json"
