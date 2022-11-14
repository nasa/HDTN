@echo off
setlocal
REM ARG1 is the cmake options
REM %~1   Expand %1 removing any surrounding quotes (")
SET CMAKE_ARGS=%~1
REM https://stackoverflow.com/questions/562038/escaping-double-quotes-in-batch-script
SET "CMAKE_ARGS=%CMAKE_ARGS:""="%"
echo %CMAKE_ARGS%
REM ARG2 is the vcvars64.bat full file path surrounded with quotes
SET VCVARS_BAT_FILE_FULL_PATH=%2
echo %VCVARS_BAT_FILE_FULL_PATH%
IF EXIST %VCVARS_BAT_FILE_FULL_PATH% (
    CALL %VCVARS_BAT_FILE_FULL_PATH%
 ) ELSE (
    Echo The vcvars64.bat file was not found.
    GOTO fail
 )
perlll
IF %ERRORLEVEL% NEQ 0 GOTO fail
GOTO end
:fail
echo - Script failed
exit /b 1

:end
endlocal
