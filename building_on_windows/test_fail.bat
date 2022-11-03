@echo off
setlocal
IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    CALL "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
 ) ELSE IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    CALL "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
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
