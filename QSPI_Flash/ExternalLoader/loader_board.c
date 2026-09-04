#include "loader_board.h"

#include <stdint.h>

#include "main.h"
#include "quadspi.h"

/** @brief Loader 私有 QSPI HAL 句柄，名称与平台适配层约定一致。 */
QSPI_HandleTypeDef hqspi;
/** @brief 板级初始化进度/失败码，定义见 loader_board.h。 */
volatile int loader_board_stage;
/** @brief 发生 Fault 时保存的 Configurable Fault Status Register。 */
volatile uint32_t loader_fault_cfsr;
/** @brief 发生 Fault 时保存的 HardFault Status Register。 */
volatile uint32_t loader_fault_hfsr;
/** @brief 发生 BusFault 时保存的错误地址。 */
volatile uint32_t loader_fault_bfar;
/** @brief 发生 MemManage Fault 时保存的错误地址。 */
volatile uint32_t loader_fault_mmfar;

/** @brief 链接脚本定义的 Loader 栈顶地址。 */
extern uint32_t __loader_stack_top__;
void SysTick_Handler(void);
void QUADSPI_IRQHandler(void);

/** @brief 未使用中断的安全默认入口；停在循环中以便 GDB 定位。 */
static void Default_Handler(void)
{
    while (1) {
    }
}

/** @brief Fault 统一入口，先保存诊断寄存器再停止执行。 */
static void Fault_Handler(void)
{
    loader_fault_cfsr = SCB->CFSR;
    loader_fault_hfsr = SCB->HFSR;
    loader_fault_bfar = SCB->BFAR;
    loader_fault_mmfar = SCB->MMFAR;
    while (1) {
    }
}

/**
 * @brief Loader 自带 RAM 向量表。
 *
 * 烧录工具不会运行应用启动文件，因此这里显式提供初始栈指针、Fault、
 * SysTick 和 QUADSPI IRQ，其余中断指向 Default_Handler。
 */
__attribute__((section(".isr_vector"), used))
const uintptr_t loader_vectors[16U + QUADSPI_IRQn + 1U] = {
    [0] = (uintptr_t)&__loader_stack_top__,
    [1 ... 2] = (uintptr_t)Default_Handler,
    [3 ... 6] = (uintptr_t)Fault_Handler,
    [7 ... 14] = (uintptr_t)Default_Handler,
    [15] = (uintptr_t)SysTick_Handler,
    [16 ... (15U + QUADSPI_IRQn)] = (uintptr_t)Default_Handler,
    [16U + QUADSPI_IRQn] = (uintptr_t)QUADSPI_IRQHandler
};

/** @brief HAL 毫秒节拍中断，使超时轮询在 Loader 中正常工作。 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/** @brief HAL 兼容的不可恢复错误入口，停住等待调试器检查。 */
void Error_Handler(void)
{
    while (1) {
    }
}

/**
 * @brief 用板载 25 MHz HSE 配置 480 MHz SYSCLK 和 240 MHz HCLK。
 * @return 0 成功，-1 表示振荡器或总线时钟配置失败。
 */
static int loader_clock_init(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clock = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscillator.PLL.PLLM = 5U;
    oscillator.PLL.PLLN = 192U;
    oscillator.PLL.PLLP = 2U;
    oscillator.PLL.PLLQ = 2U;
    oscillator.PLL.PLLR = 2U;
    oscillator.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    oscillator.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        return -1;
    }

    clock.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                      RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clock.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clock.AHBCLKDivider = RCC_HCLK_DIV2;
    clock.APB3CLKDivider = RCC_APB3_DIV2;
    clock.APB1CLKDivider = RCC_APB1_DIV2;
    clock.APB2CLKDivider = RCC_APB2_DIV2;
    clock.APB4CLKDivider = RCC_APB4_DIV2;
    return HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_4) == HAL_OK ? 0 : -1;
}

/**
 * @brief 初始化 NAND 禁用脚、U7 QSPI 引脚、时钟和 QUADSPI 外设。
 * @return 0 成功，-1 表示外设时钟或 QSPI 初始化失败。
 * @note 引脚定义对应当前 STM32H750XBHx 开发板，不可直接用于 VBT6 新板。
 */
static int loader_qspi_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    RCC_PeriphCLKInitTypeDef peripheral_clock = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_MDMA_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_9, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &gpio);

    peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
    peripheral_clock.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock) != HAL_OK) {
        return -1;
    }
    __HAL_RCC_QSPI_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOG, &gpio);

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOF, &gpio);
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Alternate = GPIO_AF10_QUADSPI;
    HAL_GPIO_Init(GPIOF, &gpio);
    gpio.Pin = GPIO_PIN_2;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOB, &gpio);

    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = 3U;
    hqspi.Init.FifoThreshold = 4U;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize = 24U;
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_4_CYCLE;
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    return HAL_QSPI_Init(&hqspi) == HAL_OK ? 0 : -1;
}

/**
 * @brief 覆盖 HAL 的 QSPI MSP 弱函数。
 *
 * Loader 已在 loader_qspi_init() 中显式完成 GPIO/时钟配置，因此这里不
 * 再重复操作。
 *
 * @param[in] handle HAL 传入的 QSPI 句柄，当前无需使用。
 */
void HAL_QSPI_MspInit(QSPI_HandleTypeDef *handle)
{
    (void)handle;
}

int loader_board_init(void)
{
    loader_board_stage = 1;
    SCB->CPACR |= (3UL << (10U * 2U)) | (3UL << (11U * 2U));
    __DSB();
    __ISB();
    SCB->VTOR = (uint32_t)(uintptr_t)loader_vectors;
    __DSB();
    __ISB();
    HAL_Init();
    loader_board_stage = 2;
    if (loader_clock_init() != 0) {
        loader_board_stage = -2;
        return -1;
    }
    loader_board_stage = 3;
    if (loader_qspi_init() != 0) {
        loader_board_stage = -3;
        return -1;
    }
    loader_board_stage = 4;
    return 0;
}
