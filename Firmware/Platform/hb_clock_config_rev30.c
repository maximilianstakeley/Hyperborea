/*
 * Hyperborea Rev30 STM32H743 clock tree reference implementation.
 * HSE source: 25 MHz active oscillator X50. Because this is an active clock
 * source, HSE_BYPASS is expected. Confirm X50's exact output type/voltage and
 * that its output reaches PH0 before enabling this setting.
 *
 * PLL1: 25 MHz / M5 = 5 MHz input; xN192 = 960 MHz VCO; /P2 = 480 MHz SYSCLK.
 * PLL1Q /20 = 48 MHz auxiliary clock. HCLK is SYSCLK /2 = 240 MHz.
 * APB clocks are divided by 2 = 120 MHz. Validate all peripheral kernel clocks
 * independently in CubeMX; this function does not configure every peripheral.
 *
 * 480 MHz requires VOS0 and eligible silicon revision. ST documents a silicon
 * revision-Y limitation to 400 MHz. Read DBGMCU_IDC.REV_ID / errata and select
 * the safe fallback before production. Do not assume every STM32H743ZI sample
 * can operate at 480 MHz.
 */
#include "main.h"

#ifndef HB_STM32H743_REVISION_V_CONFIRMED
#error "480 MHz requires confirmation that the fitted STM32H743 silicon revision supports VOS0; revision Y is limited to 400 MHz"
#endif
#ifndef HB_MCU_SUPPLY_CONFIG
#error "Define HB_MCU_SUPPLY_CONFIG to match the actual board power topology (PWR_LDO_SUPPLY or PWR_DIRECT_SMPS_SUPPLY)"
#endif

void HB_ClockConfig_Rev30(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Power supply mode must match the actual board regulator topology. */
    if (HAL_PWREx_ConfigSupply(HB_MCU_SUPPLY_CONFIG) != HAL_OK) Error_Handler();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY) == 0U) { }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_BYPASS; /* X50 is an active 25 MHz oscillator */
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 5;
    osc.PLL.PLLN = 192;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 20;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_2; /* 4–8 MHz PLL input range */
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_APB3_DIV2;
    clk.APB1CLKDivider = RCC_APB1_DIV2;
    clk.APB2CLKDivider = RCC_APB2_DIV2;
    clk.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}
