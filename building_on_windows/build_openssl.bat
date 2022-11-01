@echo off
setlocal
REM ARG1 is the install folder name
SET OPENSSL_INSTALL_FOLDER_NAME=%1
CALL "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
perl Configure VC-WIN64A --prefix="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%" --openssldir="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%\SSL"
IF %ERRORLEVEL% NEQ 0 GOTO fail
nmake
IF %ERRORLEVEL% NEQ 0 GOTO fail
REM nmake test
REM admin prompt not needed below due to --openssldir set above (not to program files)
nmake install_sw
IF %ERRORLEVEL% NEQ 0 GOTO fail

REM the following do not copy with install
copy libssl_static.lib "%cd%\%OPENSSL_INSTALL_FOLDER_NAME%\lib"
IF %ERRORLEVEL% NEQ 0 GOTO fail
copy libcrypto_static.lib "%cd%\%OPENSSL_INSTALL_FOLDER_NAME%\lib"
IF %ERRORLEVEL% NEQ 0 GOTO fail

GOTO end
:fail
echo - Script failed
exit /b 1

:end
endlocal
