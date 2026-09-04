# QSPI Flash 驱动使用说明

本目录提供 STM32H750 访问单颗 Winbond W25Q256（32 MiB）的完整实现，包括轮询读写、MDMA、扇区/整片擦除、Memory-Mapped、动态 MPU、XIP、自测试和 External Loader。

当前平台适配使用 STM32H7 HAL 的全局 `QSPI_HandleTypeDef hqspi`，并通过 `#include "quadspi.h"` 获取其声明。复制本目录后，目标工程必须自行提供 `quadspi.h`、`hqspi`、GPIO/QSPI/MDMA 初始化代码和 STM32H7 HAL；应用层 API 可以保持不变。

## 文件组成

| 文件 | 作用 |
| --- | --- |
| `external_flash.h/.c` | 应用应使用的公共 API |
| `flash_device.h/.c` | W25Q256 指令、状态寄存器和器件参数 |
| `flash_port_stm32h7.h/.c` | STM32H7 HAL、QSPI、MDMA 和 MPU 平台适配 |
| `external_flash_test.h/.c` | 数据模式、长度、跨页和跨扇区自动测试 |
| `external_flash_xip.h/.c` | `.qspi_text` 外置执行测试 |
| `ExternalLoader/` | 可独立加载到 SRAM 运行的外部烧录算法 |

普通应用通常只需要包含：

```c
#include "external_flash.h"
```

如果要调用自动数据测试或 XIP 测试，再分别包含：

```c
#include "external_flash_test.h"
#include "external_flash_xip.h"
```

应用代码不应直接依赖 `flash_device.h` 或 `flash_port_stm32h7.h`，除非正在移植或调试驱动内部实现。

## 编译接入

需要把以下源文件加入应用目标：

```text
QSPI_Flash/flash_port_stm32h7.c
QSPI_Flash/flash_device.c
QSPI_Flash/external_flash.c
```

使用自动测试或 XIP 时再加入：

```text
QSPI_Flash/external_flash_test.c
QSPI_Flash/external_flash_xip.c
```

同时将 `QSPI_Flash` 加入头文件搜索路径。例如目标工程使用 CMake 时，可以直接加入：

```cmake
target_sources(your_firmware_target PRIVATE
    QSPI_Flash/flash_port_stm32h7.c
    QSPI_Flash/flash_device.c
    QSPI_Flash/external_flash.c
)

# 需要破坏性自动数据测试时加入。
target_sources(your_firmware_target PRIVATE
    QSPI_Flash/external_flash_test.c
)

# 需要 XIP 测试且已经配置 .qspi_text 链接段时加入。
target_sources(your_firmware_target PRIVATE
    QSPI_Flash/external_flash_xip.c
)

target_include_directories(your_firmware_target PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/QSPI_Flash
)
```

将 `your_firmware_target` 替换为目标工程实际的固件 target 名称。目标工程自身的 HAL/CMSIS 和生成代码头文件目录也必须处于 include path 中，以便找到 `stm32h7xx_hal.h` 和 `quadspi.h`。

平台必须提供 CubeMX/HAL 生成的：

- 全局 `QSPI_HandleTypeDef hqspi`；
- `MX_GPIO_Init()`；
- `MX_MDMA_Init()`；
- `MX_QUADSPI_Init()`；
- HAL 毫秒节拍；
- W25Q256 对应的 QSPI GPIO 和时钟配置。

## 地址和容量约定

公共读写、擦除 API 使用的是 **Flash 内部偏移**，不是 Cortex-M7 Memory-Mapped 绝对地址。

| 项目 | 数值 |
| --- | --- |
| Flash 内部偏移范围 | `0x00000000~0x01FFFFFF` |
| Memory-Mapped 地址范围 | `0x90000000~0x91FFFFFF` |
| 总容量 | 32 MiB |
| 编程页大小 | 256 字节 |
| 擦除扇区大小 | 4 KiB |
| 擦除值 | `0xFF` |

例如，下面两者指向同一个 Flash 字节：

```text
公共 API 偏移：       0x00100000
Memory-Mapped 地址：  0x90100000
```

## 启动阶段 MPU 配置

在 QUADSPI 尚未进入 Memory-Mapped 模式时，应禁止 CPU 访问整个 QSPI 窗口，避免 Cortex-M7 推测取指或预取导致总线错误。下面是可以直接放入目标应用的最小配置：

```c
#include "stm32h7xx_hal.h"

static void App_QSPI_MPU_Config(void)
{
    MPU_Region_InitTypeDef region = {0};

    HAL_MPU_Disable();

    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER1;
    region.BaseAddress = 0x90000000UL;
    region.Size = MPU_REGION_SIZE_256MB;
    region.SubRegionDisable = 0U;
    region.TypeExtField = MPU_TEX_LEVEL0;
    region.AccessPermission = MPU_REGION_NO_ACCESS;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    region.IsShareable = MPU_ACCESS_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&region);

    HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);
    __DSB();
    __ISB();
}
```

此驱动固定使用 MPU Region 2 动态开放实际的 32 MiB 地址范围，因此目标应用必须为驱动保留 Region 2。上例使用 Region 1 作为 256 MiB No-Access 背景保护；如果 Region 1 或 Region 2 已被其他模块使用，必须同步修改应用配置和 `flash_port_stm32h7.c` 中的 Region 编号，保证动态区域优先级高于背景保护区域。

## 初始化

在初始化 Flash 之前，必须先完成启动阶段 MPU 保护、Cache、HAL、系统时钟、GPIO、MDMA 和 QUADSPI 初始化。推荐顺序如下：

```c
App_QSPI_MPU_Config();
SCB_EnableICache();
SCB_EnableDCache();

HAL_Init();
SystemClock_Config();

MX_GPIO_Init();
MX_MDMA_Init();
MX_QUADSPI_Init();

ext_flash_info_t flash_info;
ext_flash_result_t result = ext_flash_init(&flash_info);
if (result != EXT_FLASH_OK) {
    /* 不应继续擦除或写入，可在此记录错误或进入安全状态。 */
    Error_Handler();
}
```

驱动只会在成功进入 Memory-Mapped 模式后动态开放优先级更高的 Region 2；退出映射时会重新禁用 Region 2，使上面的 No-Access 保护恢复生效。

`ext_flash_init()` 会完成：

1. 开启 MDMA 所需的 QUADSPI 中断；
2. 向 W25Q256 发送 Reset Enable 和 Reset；
3. 读取 JEDEC ID 和三个状态寄存器；
4. 检查 JEDEC ID 是否为 `EF 40 19`；
5. 检查并按需设置 Quad Enable 位。

初始化成功后，`flash_info` 中可以读取：

```c
flash_info.manufacturer_id;
flash_info.memory_type;
flash_info.capacity_id;
flash_info.status_register_1;
flash_info.status_register_2;
flash_info.status_register_3;
```

需要刷新这些信息时可调用：

```c
result = ext_flash_read_info(&flash_info);
```

## 擦除

NOR Flash 写入前必须先擦除目标区域。扇区擦除地址必须按 4 KiB 对齐：

```c
#define USER_FLASH_OFFSET 0x00020000UL

ext_flash_result_t result = ext_flash_erase_sector(USER_FLASH_OFFSET);
if (result != EXT_FLASH_OK) {
    /* 擦除失败。 */
}
```

如果给 `ext_flash_erase_sector()` 传入未对齐地址或超出 32 MiB 的偏移，函数会返回 `EXT_FLASH_ERROR_IO`。

整片擦除接口为：

```c
result = ext_flash_erase_chip();
```

> `ext_flash_erase_chip()` 会清除整颗 W25Q256 的全部内容，最长可能等待 300 秒。除非明确确认所有数据都可以丢失，否则不要调用。

## 轮询方式写入

调用者负责先擦除目标扇区，`ext_flash_write()` 会自动把任意长度写入拆分为不跨越 256 字节页边界的 Page Program：

```c
static const uint8_t write_data[] = {
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE
};

ext_flash_result_t result = ext_flash_erase_sector(USER_FLASH_OFFSET);
if (result == EXT_FLASH_OK) {
    result = ext_flash_write(USER_FLASH_OFFSET,
                             write_data,
                             sizeof(write_data));
}
```

参数含义：

- `address`：Flash 内部字节偏移；
- `data`：待写入数据的地址，不可为 `NULL`；
- `length`：写入字节数，必须大于 0 且不能越过 `0x01FFFFFF`。

写入接口不会自动保存或恢复原扇区中的其他数据。如果只修改扇区的一部分，并且需要保留其余内容，应先读出整个扇区，在 RAM 中修改，再执行“擦除整个扇区并写回”。

## 轮询方式读取

```c
uint8_t read_data[sizeof(write_data)];

ext_flash_result_t result = ext_flash_read(USER_FLASH_OFFSET,
                                            read_data,
                                            sizeof(read_data));
if (result == EXT_FLASH_OK) {
    /* read_data 中包含读回内容。 */
}
```

参数含义：

- `address`：Flash 内部字节偏移；
- `data`：接收缓冲区，不可为 `NULL`；
- `length`：读取字节数，必须大于 0 且不能越界。

完整的擦除、写入和读回校验示例：

```c
uint8_t readback[sizeof(write_data)];

if (ext_flash_erase_sector(USER_FLASH_OFFSET) == EXT_FLASH_OK &&
    ext_flash_write(USER_FLASH_OFFSET, write_data,
                    sizeof(write_data)) == EXT_FLASH_OK &&
    ext_flash_read(USER_FLASH_OFFSET, readback,
                   sizeof(readback)) == EXT_FLASH_OK) {
    for (size_t i = 0; i < sizeof(write_data); ++i) {
        if (readback[i] != write_data[i]) {
            /* i 是首个数据错误偏移。 */
            break;
        }
    }
}
```

## Write Enable 和忙等待

页编程和擦除接口内部会自动执行 Write Enable 并等待器件就绪。一般应用不需要单独调用以下接口：

```c
ext_flash_write_enable();
ext_flash_wait_ready(100U);
```

它们主要用于器件诊断或测试：

- `ext_flash_write_enable()` 发送 WREN 并确认 `SR1.WEL=1`；
- `ext_flash_wait_ready(timeout_ms)` 等待 `SR1.BUSY=0`；
- 等待超时返回 `EXT_FLASH_ERROR_TIMEOUT`。

## MDMA 读写

MDMA 接口的地址、擦除和页边界规则与轮询接口一致：

```c
__attribute__((section(".ram_axi"), aligned(32)))
static uint8_t mdma_write_buffer[512];

__attribute__((section(".ram_axi"), aligned(32)))
static uint8_t mdma_read_buffer[512];

/* CPU 写入源缓冲区后，把最新数据清理到 SRAM。 */
SCB_CleanDCache_by_Addr((uint32_t *)mdma_write_buffer,
                        sizeof(mdma_write_buffer));

ext_flash_erase_sector(USER_FLASH_OFFSET);
ext_flash_write_mdma(USER_FLASH_OFFSET,
                     mdma_write_buffer,
                     sizeof(mdma_write_buffer));

/* 启动接收前丢弃目标区可能存在的脏 Cache line。 */
SCB_CleanInvalidateDCache_by_Addr((uint32_t *)mdma_read_buffer,
                                  sizeof(mdma_read_buffer));
ext_flash_read_mdma(USER_FLASH_OFFSET,
                    mdma_read_buffer,
                    sizeof(mdma_read_buffer));

/* MDMA 完成后让 CPU 重新从 SRAM 获取数据。 */
SCB_InvalidateDCache_by_Addr((uint32_t *)mdma_read_buffer,
                             sizeof(mdma_read_buffer));
```

MDMA 使用限制：

- 缓冲区不能放在 MDMA 无法访问的 DTCM；
- 当前驱动已在 `.ram_axi`（AXI SRAM，`0x24000000`）缓冲区上完成验证；
- 缓冲区地址和执行 Cache 维护的长度应按 32 字节 Cache line 对齐；
- 当前生成的 MDMA 配置在 D2 SRAM 上实测数据错误，迁移前必须重新验证总线和 MDMA 配置；
- `MX_MDMA_Init()` 必须在 `ext_flash_init()` 前完成。

复制本目录时不会同时复制应用链接脚本，因此必须在目标工程的链接脚本中加入 `.ram_axi`。假设目标脚本中的 `RAM` 是从 `0x24000000` 开始的 AXI SRAM，可加入：

```ld
.ram_axi (NOLOAD) :
{
    . = ALIGN(32);
    *(.ram_axi)
    *(.ram_axi*)
    . = ALIGN(32);
} >RAM
```

如果目标链接脚本中的 `RAM` 不是 AXI SRAM，应先在 `MEMORY` 中定义正确的 AXI SRAM 区域，再把 `.ram_axi` 放入该区域。

## Memory-Mapped 数据读取

Memory-Mapped 模式把 Flash 映射到 `0x90000000~0x91FFFFFF`。正确调用顺序如下：

```c
const uint32_t flash_offset = USER_FLASH_OFFSET;

if (ext_flash_memory_mapped_enable() == EXT_FLASH_OK) {
    if (ext_flash_memory_mapped_access_enable(0) == EXT_FLASH_OK) {
        const uint8_t *mapped =
            (const uint8_t *)(0x90000000UL + flash_offset);

        /* 现在可以只读访问 mapped[0]、mapped[1]…… */

        ext_flash_memory_mapped_access_disable();
    }
    ext_flash_memory_mapped_disable();
}
```

`ext_flash_memory_mapped_access_enable()` 的参数含义：

- `0`：只用于数据读取，MPU 禁止从该区域取指；
- 非 `0`：允许执行外置代码，仅用于已经正确链接和编程的 XIP 区域。

必须遵守以下规则：

1. 先调用 `ext_flash_memory_mapped_enable()` 配置 QUADSPI；
2. 再调用 `ext_flash_memory_mapped_access_enable()` 开放 MPU Region 2；
3. CPU 读取结束后先关闭 MPU 访问；
4. 最后调用 `ext_flash_memory_mapped_disable()` 退出 QUADSPI 映射；
5. 执行擦除或写入前必须退出 Memory-Mapped 模式；
6. 启用 D-Cache 时，读取刚写入的数据前应处理对应地址的 Cache 一致性。

## 自动数据测试

初始化成功后可以执行：

```c
ext_flash_test_report_t report;
ext_flash_result_t result = ext_flash_run_data_tests(&report);
```

测试包括固定值、递增值、全 `0x00`、全 `0xFF`、伪随机、不同长度、跨页和跨扇区写入。失败时可检查：

```c
report.result;
report.stage;
report.case_index;
report.passed_cases;
report.address;
report.first_mismatch_offset;
report.expected;
report.actual;
```

> 此测试会反复擦写最后两个扇区，即内部偏移 `0x01FFE000~0x01FFFFFF`。不要在这些地址保存有效数据。

## XIP 测试

`external_flash_xip.c` 要求目标应用的链接脚本将 `.qspi_text` 运行地址固定在 `0x90100000`，同时在内部 Flash 保留加载镜像。复制本目录后，需要先在链接脚本的 `MEMORY` 中加入 32 MiB QSPI 区域：

```ld
MEMORY
{
    /* 保留目标工程原有的内部 FLASH 和 RAM 定义。 */
    QSPI (rx) : ORIGIN = 0x90000000, LENGTH = 32M
}
```

再在 `SECTIONS` 中加入：

```ld
.qspi_text 0x90100000 :
{
    . = ALIGN(4);
    __qspi_text_start__ = .;
    KEEP(*(.qspi_text))
    KEEP(*(.qspi_text*))
    . = ALIGN(4);
    __qspi_text_end__ = .;
} >QSPI AT>FLASH

__qspi_text_load_start__ = LOADADDR(.qspi_text);
__qspi_text_load_end__ = LOADADDR(.qspi_text) + SIZEOF(.qspi_text);
```

其中 `FLASH` 必须是目标工程已有的内部 Flash 区域名称。如果名称不同，应把 `AT>FLASH` 改成实际名称。配置完成后调用：

```c
ext_flash_xip_report_t report;
ext_flash_result_t result = ext_flash_run_xip_test(&report);
```

函数会自动擦除 XIP 镜像区、写入代码、进入 Memory-Mapped 模式、开放可执行 MPU 区域、清除 I-Cache 并从外置 Flash 调用测试函数。

> XIP 测试会改写外置 Flash 偏移 `0x00100000` 附近的扇区。实际产品中，从外置 Flash 执行代码时不能同时擦除或编程同一颗 Flash；Flash 操作代码和关键中断应保留在内部 Flash 或 RAM。

## 返回值

| 返回值 | 含义 |
| --- | --- |
| `EXT_FLASH_OK` | 操作成功 |
| `EXT_FLASH_ERROR_IO` | 参数非法、越界或 QSPI/HAL 操作失败 |
| `EXT_FLASH_ERROR_DEVICE_ID` | JEDEC ID 不是预期的 W25Q256 |
| `EXT_FLASH_ERROR_TIMEOUT` | 显式等待 Flash 就绪超时 |

除明确用于诊断的场景外，不要忽略返回值。初始化、擦除或写入失败后，不应继续使用目标数据。

## External Loader

`ExternalLoader/` 已包含 Loader 入口、板级初始化、设备描述和独立 RAM 链接脚本。它必须作为独立目标构建，不能与普通应用固件链接在一起。

创建 Loader target 时至少加入：

```text
QSPI_Flash/ExternalLoader/loader.c
QSPI_Flash/ExternalLoader/loader_board.c
QSPI_Flash/ExternalLoader/Dev_Inf.c
QSPI_Flash/flash_port_stm32h7.c
QSPI_Flash/flash_device.c
QSPI_Flash/external_flash.c
```

此外还必须链接目标 STM32H7 HAL/CMSIS 的启动依赖，包括 HAL Core、Cortex、GPIO、MDMA、PWR、QSPI、RCC、Flash 和 LL DelayBlock 模块。CMake target 的核心配置形式如下：

```cmake
set(QSPI_FLASH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/QSPI_Flash)

add_executable(H750_W25Q256_ExtLoader
    ${QSPI_FLASH_DIR}/ExternalLoader/loader.c
    ${QSPI_FLASH_DIR}/ExternalLoader/loader_board.c
    ${QSPI_FLASH_DIR}/ExternalLoader/Dev_Inf.c
    ${QSPI_FLASH_DIR}/flash_port_stm32h7.c
    ${QSPI_FLASH_DIR}/flash_device.c
    ${QSPI_FLASH_DIR}/external_flash.c
    # 在这里加入目标工程所需的 STM32H7 HAL/CMSIS 源文件或 HAL 库。
)

target_include_directories(H750_W25Q256_ExtLoader PRIVATE
    ${QSPI_FLASH_DIR}
    ${QSPI_FLASH_DIR}/ExternalLoader
    # 在这里加入目标工程的 HAL、CMSIS、main.h 和 quadspi.h 搜索路径。
)

target_compile_definitions(H750_W25Q256_ExtLoader PRIVATE
    USE_PWR_LDO_SUPPLY
    USE_HAL_DRIVER
    STM32H750xx
)

target_link_options(H750_W25Q256_ExtLoader PRIVATE
    -T${QSPI_FLASH_DIR}/ExternalLoader/external_loader.ld
    -nostartfiles
    --specs=nano.specs
    -Wl,--gc-sections
)

set_target_properties(H750_W25Q256_ExtLoader PROPERTIES
    SUFFIX ".stldr"
)
```

配置完成后按目标工程正常的 CMake 流程构建该 target，例如：

```bash
cmake --build <build-directory> --target H750_W25Q256_ExtLoader
```

输出文件名为：

```text
H750_W25Q256_ExtLoader.stldr
```

Loader 导出 `Init`、`Read`、`Write`、`SectorErase`、`MassErase` 和 `Verify`。它使用的是 `0x90000000` 起的绝对映射地址，与普通应用 API 使用的内部偏移不同。

`ExternalLoader/loader_board.c` 中的 25 MHz HSE、PLL、QSPI 引脚和 NAND 禁用脚配置属于板级代码。复制目录到另一块板后，必须先按目标原理图修改这些配置，再生成 `.stldr`。

## 移植检查清单

移植到新的 STM32H750 板卡时至少检查：

- QSPI CLK、NCS、IO0~IO3 引脚和 Alternate Function；
- 是否存在与其他器件共用的片选或 IO 引脚；
- QSPI 内核时钟、分频、采样移位和片选高电平时间；
- `hqspi.Init.FlashSize` 是否对应 32 MiB 单 Flash；
- Flash JEDEC ID、容量、页/扇区大小和 Quad Enable 位置；
- 4-byte 地址命令是否适用于目标 Flash；
- MPU Region 编号是否与应用已有配置冲突；
- AXI SRAM 的地址、大小和链接段；
- MDMA 请求、IRQ 和 D-Cache 一致性；
- XIP 的链接地址、内部加载镜像和启动流程。

随目录提供的 `flash_port_stm32h7.c` 和 `ExternalLoader/loader_board.c` 已针对 STM32H750XBHx 开发板验证。迁移到 STM32H750VBT6 或其他封装时，必须依据新板原理图重新生成或修改板级 GPIO/QSPI 配置。
