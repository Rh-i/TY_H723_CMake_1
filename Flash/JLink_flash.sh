#!/bin/bash

# 获取当前脚本所在的目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 获取父目录名称作为项目名称
PROJECT_NAME=$(basename "$(dirname "$SCRIPT_DIR")")

# 构建 ELF 文件路径 (假设构建目录结构与 Windows 一致)
ELF_FILE="build/Debug/${PROJECT_NAME}.elf"

# 检查文件是否存在
if [ ! -f "$ELF_FILE" ]; then
    echo "Error: ELF file not found at $ELF_FILE"
    
    # 尝试查找 build/Debug 目录下的 .elf 文件
    # 使用 shopt 允许空结果不报错，或者使用 ls 配合 grep
    FOUND_ELF=$(find build/Debug -name "*.elf" -type f 2>/dev/null | head -n 1)
    
    if [ -n "$FOUND_ELF" ]; then
        echo "Looking for available ELF files in build directory..."
        echo "Found: $FOUND_ELF"
        ELF_FILE="$FOUND_ELF"
        echo "Using: $ELF_FILE"
    else
        echo "Error: No ELF files found in build directory!"
        exit 1
    fi
fi

echo "Project Name: $PROJECT_NAME"
echo "ELF File: $ELF_FILE"

# 创建临时 J-Link 脚本
TEMP_SCRIPT="temp_jlink_script.jlink"

# 写入 J-Link 命令
# 注意：Linux 下路径分隔符是 /
cat > "$TEMP_SCRIPT" <<EOF
device STM32H723VG
if SWD
speed 4000
r
loadfile $ELF_FILE
r
go
exit
EOF

# 执行 J-Link
# Linux 下使用 JLinkExe，Windows 下使用 JLink
if command -v JLinkExe &> /dev/null; then
    JLinkExe -CommanderScript "$TEMP_SCRIPT"
elif command -v JLink &> /dev/null; then
    JLink -CommanderScript "$TEMP_SCRIPT"
else
    echo "Error: JLinkExe/JLink command not found. Please ensure SEGGER J-Link software is installed and in PATH."
    # 清理临时文件
    rm -f "$TEMP_SCRIPT"
    exit 1
fi

# 清理临时文件
rm -f "$TEMP_SCRIPT"