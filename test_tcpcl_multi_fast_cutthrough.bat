@Echo off

CALL C:\Users\btomko\Anaconda3\Scripts\activate.bat C:\Users\btomko\Anaconda3
START "RegServer" /D "%~dp0\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--use-tcpcl" "--port=4557"
timeout /t 3
START "BpSink2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpsink-async.exe" "--use-tcpcl" "--port=4558"
timeout /t 3
START "Egress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\egress\hdtn-egress-async.exe" "--use-tcpcl" "--port1=4557" "--port2=4558"
timeout /t 3
START "Ingress" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\ingress\hdtn-ingress.exe"
timeout /t 3
START "BpGen2" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=0" "--use-tcpcl" "--duration=100" "--flow-id=2" "--bundle-size=100000"
timeout /t 1
START "BpGen1" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=0" "--use-tcpcl" "--duration=100" "--flow-id=1" "--bundle-size=100000"
timeout /t 8