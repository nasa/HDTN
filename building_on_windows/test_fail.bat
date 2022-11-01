@echo off
setlocal
CALL "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
perlll
IF %ERRORLEVEL% NEQ 0 GOTO fail
GOTO end
:fail
echo - Script failed
exit /b 1

:end
endlocal
