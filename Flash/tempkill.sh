#!/bin/bash

# 删除各种临时文件和编译产物
find . -name "*.bak" -type f -delete
find . -name "*.ddk" -type f -delete
find . -name "*.edk" -type f -delete
find . -name "*.lst" -type f -delete
find . -name "*.lnp" -type f -delete
find . -name "*.mpf" -type f -delete
find . -name "*.mpj" -type f -delete
find . -name "*.obj" -type f -delete
find . -name "*.omf" -type f -delete

find . -name "*.plg" -type f -delete
find . -name "*.rpt" -type f -delete
find . -name "*.tmp" -type f -delete
find . -name "*.__i" -type f -delete
find . -name "*.crf" -type f -delete
find . -name "*.o" -type f -delete
find . -name "*.d" -type f -delete
find . -name "*.axf" -type f -delete
find . -name "*.tra" -type f -delete
find . -name "*.dep" -type f -delete

find . -name "*.iex" -type f -delete
find . -name "*.htm" -type f -delete
find . -name "*.map" -type f -delete

exit 0