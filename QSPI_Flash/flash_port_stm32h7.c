#include "flash_port_stm32h7.h"

#include "quadspi.h"

#define FLASH_PORT_TIMEOUT_MS 100U

/** @brief 准备 MDMA 完成回调所需的 QUADSPI 全局中断。 */
void flash_port_prepare(void)
{
    HAL_NVIC_SetPriority(QUADSPI_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(QUADSPI_IRQn);
}

/** @brief QUADSPI 中断入口，将状态处理交给 STM32 HAL。 */
void QUADSPI_IRQHandler(void)
{
    HAL_QSPI_IRQHandler(&hqspi);
}

/**
 * @brief 填充一条默认的单线、无地址 QSPI 命令。
 * @param[out] command 待初始化的 HAL 命令结构。
 * @param[in] instruction 8 位 Flash 指令码。
 * @param[in] data_length 数据阶段字节数，0 表示没有数据阶段。
 */
static void flash_port_init_command(QSPI_CommandTypeDef *command,
                                    uint8_t instruction,
                                    uint32_t data_length)
{
    *command = (QSPI_CommandTypeDef){0};
    command->Instruction = instruction;
    command->InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command->AddressMode = QSPI_ADDRESS_NONE;
    command->AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command->DataMode = data_length == 0U ? QSPI_DATA_NONE : QSPI_DATA_1_LINE;
    command->DummyCycles = 0U;
    command->NbData = data_length;
    command->DdrMode = QSPI_DDR_MODE_DISABLE;
    command->DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command->SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
}

int flash_port_command(uint8_t instruction)
{
    QSPI_CommandTypeDef command;

    flash_port_init_command(&command, instruction, 0U);
    return HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

int flash_port_receive(uint8_t instruction, void *data, size_t length)
{
    QSPI_CommandTypeDef command;

    if ((data == NULL) || (length == 0U) || (length > UINT32_MAX)) {
        return -1;
    }

    flash_port_init_command(&command, instruction, (uint32_t)length);
    if (HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return HAL_QSPI_Receive(&hqspi, data, FLASH_PORT_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

int flash_port_transmit(uint8_t instruction, const void *data, size_t length)
{
    QSPI_CommandTypeDef command;

    if ((data == NULL) || (length == 0U)) {
        return -1;
    }
    flash_port_init_command(&command, instruction, (uint32_t)length);
    if (HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return HAL_QSPI_Transmit(&hqspi, (uint8_t *)data, FLASH_PORT_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

void flash_port_delay_ms(uint32_t milliseconds)
{
    HAL_Delay(milliseconds);
}

uint32_t flash_port_get_tick_ms(void)
{
    return HAL_GetTick();
}

/**
 * @brief 在默认命令基础上加入单线 32 位地址阶段。
 * @param[out] command 待初始化的 HAL 命令结构。
 * @param[in] instruction 8 位 Flash 指令码。
 * @param[in] address Flash 内部字节地址。
 * @param[in] data_length 数据阶段字节数。
 */
static void flash_port_init_address_command(QSPI_CommandTypeDef *command,
                                            uint8_t instruction,
                                            uint32_t address,
                                            uint32_t data_length)
{
    flash_port_init_command(command, instruction, data_length);
    command->Address = address;
    command->AddressMode = QSPI_ADDRESS_1_LINE;
    command->AddressSize = QSPI_ADDRESS_32_BITS;
}

int flash_port_address_command(uint8_t instruction, uint32_t address)
{
    QSPI_CommandTypeDef command;

    flash_port_init_address_command(&command, instruction, address, 0U);
    return HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

int flash_port_address_receive(uint8_t instruction, uint32_t address,
                               void *data, size_t length)
{
    QSPI_CommandTypeDef command;

    if ((data == NULL) || (length == 0U)) {
        return -1;
    }
    flash_port_init_address_command(&command, instruction, address, (uint32_t)length);
    if (HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return HAL_QSPI_Receive(&hqspi, data, FLASH_PORT_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

int flash_port_address_transmit(uint8_t instruction, uint32_t address,
                                const void *data, size_t length)
{
    QSPI_CommandTypeDef command;

    if ((data == NULL) || (length == 0U)) {
        return -1;
    }
    flash_port_init_address_command(&command, instruction, address, (uint32_t)length);
    if (HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return HAL_QSPI_Transmit(&hqspi, (uint8_t *)data, FLASH_PORT_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

/**
 * @brief 等待异步 QSPI/MDMA 传输完成，并在超时时中止外设。
 * @param[in] timeout_ms 最大等待时间，单位毫秒。
 * @return 0 表示 HAL 回到 READY 且无错误，否则返回 -1。
 */
static int flash_port_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (HAL_QSPI_GetState(&hqspi) != HAL_QSPI_STATE_READY) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            (void)HAL_QSPI_Abort(&hqspi);
            return -1;
        }
    }
    return HAL_QSPI_GetError(&hqspi) == HAL_QSPI_ERROR_NONE ? 0 : -1;
}

int flash_port_address_receive_mdma(uint8_t instruction, uint32_t address,
                                    void *data, size_t length, uint8_t dummy_cycles,
                                    flash_port_data_mode_t data_mode)
{
    QSPI_CommandTypeDef command;

    if ((data == NULL) || (length == 0U)) {
        return -1;
    }
    flash_port_init_address_command(&command, instruction, address, (uint32_t)length);
    command.DataMode = data_mode == FLASH_PORT_DATA_4_LINES ?
                       QSPI_DATA_4_LINES : QSPI_DATA_1_LINE;
    command.DummyCycles = dummy_cycles;
    if (HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) != HAL_OK ||
        HAL_QSPI_Receive_DMA(&hqspi, data) != HAL_OK) {
        return -1;
    }
    return flash_port_wait_ready(FLASH_PORT_TIMEOUT_MS);
}

int flash_port_address_transmit_mdma(uint8_t instruction, uint32_t address,
                                     const void *data, size_t length,
                                     flash_port_data_mode_t data_mode)
{
    QSPI_CommandTypeDef command;

    if ((data == NULL) || (length == 0U)) {
        return -1;
    }
    flash_port_init_address_command(&command, instruction, address, (uint32_t)length);
    command.DataMode = data_mode == FLASH_PORT_DATA_4_LINES ?
                       QSPI_DATA_4_LINES : QSPI_DATA_1_LINE;
    if (HAL_QSPI_Command(&hqspi, &command, FLASH_PORT_TIMEOUT_MS) != HAL_OK ||
        HAL_QSPI_Transmit_DMA(&hqspi, (uint8_t *)data) != HAL_OK) {
        return -1;
    }
    return flash_port_wait_ready(FLASH_PORT_TIMEOUT_MS);
}

int flash_port_memory_mapped_enable(uint8_t instruction, uint8_t dummy_cycles)
{
    QSPI_CommandTypeDef command;
    QSPI_MemoryMappedTypeDef config = {0};

    flash_port_init_address_command(&command, instruction, 0U, 1U);
    command.DataMode = QSPI_DATA_4_LINES;
    command.DummyCycles = dummy_cycles;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    config.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    return HAL_QSPI_MemoryMapped(&hqspi, &command, &config) == HAL_OK ? 0 : -1;
}

int flash_port_memory_mapped_disable(void)
{
    return HAL_QSPI_Abort(&hqspi) == HAL_OK ? 0 : -1;
}

int flash_port_memory_mapped_access_enable(int executable)
{
    MPU_Region_InitTypeDef region = {0};

    /* Region 2 优先级高于启动阶段配置的 Region 1 No-Access 区域。 */
    HAL_MPU_Disable();
    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER2;
    region.BaseAddress = 0x90000000UL;
    region.Size = MPU_REGION_SIZE_32MB;
    region.SubRegionDisable = 0U;
    region.TypeExtField = MPU_TEX_LEVEL0;
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec = executable != 0 ? MPU_INSTRUCTION_ACCESS_ENABLE :
                                          MPU_INSTRUCTION_ACCESS_DISABLE;
    region.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&region);
    HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);
    __DSB();
    __ISB();
    return 0;
}

void flash_port_memory_mapped_access_disable(void)
{
    MPU_Region_InitTypeDef region = {0};

    /* 禁用 Region 2 后，Region 1 自动重新阻止对 QSPI 窗口的推测访问。 */
    HAL_MPU_Disable();
    region.Enable = MPU_REGION_DISABLE;
    region.Number = MPU_REGION_NUMBER2;
    HAL_MPU_ConfigRegion(&region);
    HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);
    __DSB();
    __ISB();
}
