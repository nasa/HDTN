@echo off
setlocal
REM ARG1 is the number of build cpu cores
SET NUM_CPU_CORES=%1
CALL "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
cmake -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED=ON -DBUILD_STATIC=ON -DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX:PATH="%cd%\myinstall" ..
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


