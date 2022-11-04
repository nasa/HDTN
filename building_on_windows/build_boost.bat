@echo off
setlocal
REM ARG1 is the number of cpu cores
SET NUM_CPU_CORES=%1
REM ARG2 is the vcvars64.bat full file path surrounded with quotes
SET VCVARS_BAT_FILE_FULL_PATH=%2
echo %VCVARS_BAT_FILE_FULL_PATH%
IF EXIST %VCVARS_BAT_FILE_FULL_PATH% (
    CALL %VCVARS_BAT_FILE_FULL_PATH%
 ) ELSE (
    Echo The vcvars64.bat file was not found.
    GOTO fail
 )

CALL bootstrap.bat
IF %ERRORLEVEL% NEQ 0 GOTO fail
REM COMMENTED OUT IMMEDIATELY BELOW THE FULL DEBUG,RELEASE BUILDS
REM .\b2 -j8 toolset=msvc address-model=64 --build-type=complete define=BOOST_USE_WINAPI_VERSION=0x0A00 stage
REM BELOW ONLY BUILD RELEASE AND SHARED RUNTIME TO SPEED UP BUILD TIME
.\b2 -j%NUM_CPU_CORES% toolset=msvc address-model=64 variant=release runtime-link=shared --build-type=complete define=BOOST_USE_WINAPI_VERSION=0x0A00 stage
IF %ERRORLEVEL% EQU 0 GOTO end
REM sometimes the .b2 fails on one of the targets.. try to rerun one more time
echo ".b2 failed on at least one of the targets.. try to rerun b2 one more time"
.\b2 -j%NUM_CPU_CORES% toolset=msvc address-model=64 variant=release runtime-link=shared --build-type=complete define=BOOST_USE_WINAPI_VERSION=0x0A00 stage
IF %ERRORLEVEL% NEQ 0 GOTO fail

GOTO end
:fail
echo - Script failed
exit /b 1

:end
endlocal


