#!/bin/bash

# 获取当前脚本所在目录的父目录名称（项目名称）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_NAME="$(basename "$PROJECT_DIR")"

# 构建 ELF 文件路径（Linux原生使用正斜杠，无需转换）
ELF_FILE="build/Debug/${PROJECT_NAME}.elf"

# 检查文件是否存在
if [ ! -f "$ELF_FILE" ]; then
    echo "Error: ELF file not found at $ELF_FILE"
    exit 1
fi

# 使用标准化路径烧录
# 注意：确保 openocd 在 PATH 中，或者使用绝对路径
openocd -f Flash/daplink.cfg -c "program \"${ELF_FILE}\" verify reset exit"