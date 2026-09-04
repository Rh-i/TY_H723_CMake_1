#include "Dev_Inf.h"

/**
 * @brief W25Q256 外部存储布局描述符。
 *
 * 烧录工具从 `.Dev_Info` 段读取该全局常量：映射基址为 0x90000000，
 * 总容量 32 MiB，页大小 256 字节，共 8192 个 4 KiB 擦除扇区。
 */
__attribute__((section(".Dev_Info"), used))
const StorageInfoType StorageInfo = {
    "W25Q256_STM32H750_CMSIS_DAP",
    NOR_FLASH,
    0x90000000UL,
    0x02000000UL,
    0x00000100UL,
    0xFFU,
    {
        {8192U, 0x00001000UL},
        {0U, 0U}
    }
};
