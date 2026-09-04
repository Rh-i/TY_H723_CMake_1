@echo off
setlocal

for %%i in ("%~dp0\..") do (
    set "PROJECT_DIR=%%~fi"
    set "PROJECT_NAME=%%~nxi"
)

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Debug"

cd /d "%PROJECT_DIR%" || exit /b 1

set "BUILD_DIR=build/%BUILD_TYPE%"
set "INTERNAL_IMAGE=%BUILD_DIR%/%PROJECT_NAME%_internal.hex"
set "EXTERNAL_IMAGE=%BUILD_DIR%/%PROJECT_NAME%_usb_xip.bin"

if not exist "%INTERNAL_IMAGE%" (
    echo Error: internal image not found at %INTERNAL_IMAGE%
    exit /b 1
)
if not exist "%EXTERNAL_IMAGE%" (
    echo Error: external USB XIP image not found at %EXTERNAL_IMAGE%
    exit /b 1
)
for %%i in ("%EXTERNAL_IMAGE%") do if %%~zi EQU 0 (
    echo Error: external USB XIP image is empty at %EXTERNAL_IMAGE%
    exit /b 1
)

where openocd >nul 2>nul
if errorlevel 1 (
    echo Error: openocd not found; the J-Link script uses OpenOCD's jlink adapter.
    exit /b 1
)

echo Programming internal Flash through J-Link: %INTERNAL_IMAGE%
openocd -f Flash/jlink.cfg -c "program \"%INTERNAL_IMAGE%\" verify reset exit"
if errorlevel 1 exit /b %errorlevel%

echo Programming W25Q64JV USB XIP payload through J-Link: %EXTERNAL_IMAGE%
openocd -f Flash/jlink.cfg ^
    -c "init" ^
    -c "reset halt" ^
    -c "mww 0x38003ffc 0x55535031" ^
    -c "resume" ^
    -c "sleep 1500" ^
    -c "halt" ^
    -c "flash probe stm32h7x.octospi2" ^
    -c "flash write_image erase \"%EXTERNAL_IMAGE%\" 0x70110000 bin" ^
    -c "verify_image \"%EXTERNAL_IMAGE%\" 0x70110000 bin" ^
    -c "reset run" ^
    -c "shutdown"

exit /b %errorlevel%
