@echo off
setlocal
REM ARG1 is the install folder name
REM %~1   Expand %1 removing any surrounding quotes (")
SET OPENSSL_INSTALL_FOLDER_NAME=%~1
REM ARG2 is the vcvars64.bat full file path surrounded with quotes
SET VCVARS_BAT_FILE_FULL_PATH=%2
echo %VCVARS_BAT_FILE_FULL_PATH%
IF EXIST %VCVARS_BAT_FILE_FULL_PATH% (
    CALL %VCVARS_BAT_FILE_FULL_PATH%
 ) ELSE (
    Echo The vcvars64.bat file was not found.
    GOTO fail
 )
REM ARG3 is the make command without quotes (either nmake or jom -j#)
SET MAKE_COMMAND=%~3
echo %MAKE_COMMAND%
REM /FS (not a linker flag) needed to prevent this error: if multiple CL.EXE write to the same .PDB file, please use /FS
if /I "%MAKE_COMMAND%" == "nmake" (
    echo "Using NMAKE"
    perl Configure VC-WIN64A no-unit-test no-tests threads shared --prefix="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%" --openssldir="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%\SSL"
 ) ELSE (
    echo "Using JOM"
    REM perl Configure VC-WIN64A CFLAGS=/FS CXXFLAGS=/FS --prefix="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%" --openssldir="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%\SSL" (don't use.. removes /O2)
    perl Configure VC-WIN64A no-unit-test no-tests threads shared /FS --prefix="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%" --openssldir="%cd%\%OPENSSL_INSTALL_FOLDER_NAME%\SSL"
 )

IF %ERRORLEVEL% NEQ 0 GOTO fail
%MAKE_COMMAND%
IF %ERRORLEVEL% NEQ 0 GOTO fail
REM nmake test
REM admin prompt not needed below due to --openssldir set above (not to program files)
REM don't use JOM for the install portion
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
