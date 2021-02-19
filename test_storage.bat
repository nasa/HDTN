@Echo off
SET BUILD_DIR=C:\Users\btomko\CMakeBuilds\89d6b833-3ef7-3331-9a0b-cec90ac58946\build\x64-Release

CALL C:\Users\btomko\Anaconda3\Scripts\activate.bat C:\Users\btomko\Anaconda3
START "RegServer" /D "%~dp0\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "BpSink" /D "%BUILD_DIR%" "cmd /k" "%BUILD_DIR%\common\bpcodec\apps\bpsink-async.exe" "--use-tcpcl"
timeout /t 3
START "Egress" /D "%BUILD_DIR%" "cmd /k" "%BUILD_DIR%\module\egress\hdtn-egress-async.exe" "--use-tcpcl"
timeout /t 3
START "Ingress" /D "%BUILD_DIR%" "cmd /k" "%BUILD_DIR%\module\ingress\hdtn-ingress.exe"
timeout /t 3
START "Send Release" /D "%BUILD_DIR%" "cmd /k" "%BUILD_DIR%\module\storage\hdtn-release-message-sender.exe" "--release-message-type=start" "--flow-id=2" "--delay-before-send=14"
timeout /t 1
START "Storage" /D "%~dp0\module\storage\storage-brian\unit_tests" "cmd /k" "%BUILD_DIR%\module\storage\hdtn-storage.exe"
timeout /t 3
START "BpGen" /D "%BUILD_DIR%" "cmd /k" "%BUILD_DIR%\common\bpcodec\apps\bpgen-async.exe" "--bundle-rate=100" "--use-tcpcl" "--duration=5"
timeout /t 8





