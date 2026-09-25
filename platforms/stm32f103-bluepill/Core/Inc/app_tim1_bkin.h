#ifndef APP_TIM1_BKIN_H
#define APP_TIM1_BKIN_H

#include "tim1_bkin.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PA6 = TIM1_BKIN (AF), active-low. Configure in CubeMX / MX_TIM1_Init. */
void AppTim1Bkin_Init(void);
void AppTim1Bkin_OnBreakIrq(void);          /* TIM1 break IRQ: latch, keep MOE clear */
bool AppTim1Bkin_TryClearAndArmMoe(bool pll_locked);
bool AppTim1Bkin_OutputsAllowed(void);
const tim1_bkin_t *AppTim1Bkin_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
