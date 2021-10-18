@Echo off

REM INITIALIZE ANACONDA PYTHON NEXT LINE (requires activate.bat in PATH)
CALL activate.bat
START "RegServer" /D "%HDTN_SOURCE_ROOT%\common\regsvr" "cmd /k" "python" "main.py"
timeout /t 3
START "HDTN One Process" /D "%HDTN_BUILD_ROOT%" "cmd /k" "%HDTN_BUILD_ROOT%\module\hdtn_one_process\hdtn-one-process.exe" "--cut-through-only-test" "--hdtn-config-file=%HDTN_SOURCE_ROOT%\tests\config_files\hdtn\hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json"
