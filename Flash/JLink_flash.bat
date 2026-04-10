@echo off
setlocal enabledelayedexpansion

REM 获取当前脚本所在目录的父目录名称（项目名称）
for %%i in ("%~dp0\..") do set "PROJECT_NAME=%%~nxi"

REM 构建 ELF 文件路径
set "ELF_FILE=build\Debug\!PROJECT_NAME!.elf"

REM 检查文件是否存在
if not exist "!ELF_FILE!" (
    echo Error: ELF file not found at !ELF_FILE!
    
    REM 尝试查找 build 目录下的 .elf 文件
    if exist "build\Debug\*.elf" (
        echo Looking for available ELF files in build directory...
        for %%f in (build\*.elf) do (
            set "FOUND_ELF=%%f"
            echo Found: !FOUND_ELF!
        )
        set "ELF_FILE=!FOUND_ELF!"
        echo Using: !ELF_FILE!
    ) else (
        echo Error: No ELF files found in build directory!
        pause
        exit /b 1
    )
)

echo Project Name: !PROJECT_NAME!
echo ELF File: !ELF_FILE!

REM 创建临时 J-Link 脚本
set "TEMP_SCRIPT=temp_jlink_script.jlink"
(
echo device STM32H723VG
echo if SWD
echo speed 4000
echo r
echo loadfile !ELF_FILE!
echo r
echo go
echo exit
) > "!TEMP_SCRIPT!"

REM 执行 J-Link
JLink -CommanderScript "!TEMP_SCRIPT!"

REM 清理临时文件
del "!TEMP_SCRIPT!"
