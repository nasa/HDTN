@Echo off
SET BUILD_DIR=C:\Users\btomko\CMakeBuilds\89d6b833-3ef7-3331-9a0b-cec90ac58946\build\x64-Release

START "Unit tests" /D "%~dp0\module\storage\storage-brian\unit_tests" "cmd /k" "%BUILD_DIR%\tests\unit_tests\unit-tests.exe"
