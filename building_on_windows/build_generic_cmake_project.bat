@echo off
setlocal
REM ARG1 is the number of build cpu cores
SET NUM_CPU_CORES=%1
echo %NUM_CPU_CORES%
REM ARG2 is the cmake options
REM %~2   Expand %2 removing any surrounding quotes (")
SET CMAKE_ARGS=%~2
echo %CMAKE_ARGS%
IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    CALL "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
 ) ELSE IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    CALL "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
 ) ELSE (
    Echo The vcvars64.bat file was not found.
    GOTO fail
 )
cmake -G "Visual Studio 17 2022" -A x64 %CMAKE_ARGS%
IF %ERRORLEVEL% NEQ 0 GOTO fail
cmake --build . -j%NUM_CPU_CORES% --config Release
IF %ERRORLEVEL% NEQ 0 GOTO fail
cmake --build . --target install --config Release
IF %ERRORLEVEL% NEQ 0 GOTO fail

GOTO end
:fail
echo - Script failed
exit /b 1

:end
endlocal


