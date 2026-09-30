#include "app_tim1_bkin.h"

#include "main.h"

#include <stddef.h>

extern TIM_HandleTypeDef htim1;

static tim1_bkin_t g_bkin;

void AppTim1Bkin_Init(void) {
    TIM_BreakDeadTimeConfigTypeDef brk = {0};

    tim1_bkin_init(&g_bkin);

    /*
     * CubeMX contract (also written in README):
     *   TIM1 Break: Enable
     *   Break polarity: Low (active-low on PA6) => BKP=0
     *   Automatic output: Disable => AOE=0
     *   Pin: PA6 TIM1_BKIN
     * PB12 remains GPIO fault only — never wire PB12 as BKIN on F103.
     */
    brk.OffStateRunMode = TIM_OSSR_DISABLE;
    brk.OffStateIDLEMode = TIM_OSSI_DISABLE;
    brk.LockLevel = TIM_LOCKLEVEL_OFF;
    brk.DeadTime = 0u;
    brk.BreakState = TIM_BREAK_ENABLE;
    brk.BreakPolarity = TIM_BREAKPOLARITY_LOW;
    brk.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    (void)HAL_TIMEx_ConfigBreakDeadTime(&htim1, &brk);

    /* Ensure MOE starts clear; PWM channels may be started later. */
    __HAL_TIM_MOE_DISABLE(&htim1);
    g_bkin.moe = false;

    (void)HAL_TIM_Base_Start_IT(&htim1); /* break IRQ uses TIM1 update/break vector path */
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);
}

void AppTim1Bkin_OnBreakIrq(void) {
    /* Hardware cleared MOE already; record trip before any control ISR work. */
    tim1_bkin_on_hardware_break(&g_bkin);
    __HAL_TIM_MOE_DISABLE(&htim1);
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
}

bool AppTim1Bkin_TryClearAndArmMoe(bool pll_locked) {
    bool safe = true;
#if defined(HW_FAULT_GPIO_Port) && defined(HW_FAULT_Pin)
    if (HAL_GPIO_ReadPin(HW_FAULT_GPIO_Port, HW_FAULT_Pin) == GPIO_PIN_RESET) safe = false;
#endif
#if defined(ESTOP_GPIO_Port) && defined(ESTOP_Pin)
    if (HAL_GPIO_ReadPin(ESTOP_GPIO_Port, ESTOP_Pin) == GPIO_PIN_RESET) safe = false;
#endif
    if (!tim1_bkin_try_clear(&g_bkin, pll_locked, safe)) return false;
    __HAL_TIM_MOE_ENABLE(&htim1);
    g_bkin.moe = true;
    return true;
}

bool AppTim1Bkin_OutputsAllowed(void) {
    return tim1_bkin_outputs_allowed(&g_bkin);
}

const tim1_bkin_t *AppTim1Bkin_GetState(void) {
    return &g_bkin;
}

/*
 * Hook for stm32f1xx_it.c:
 *   void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim) {
 *     if (htim->Instance == TIM1) AppTim1Bkin_OnBreakIrq();
 *   }
 */
