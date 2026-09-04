#ifndef APP_CONVERTER_H
#define APP_CONVERTER_H

#include "converter_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Step B: current loop at TIM1 PWM rate (~20 kHz). 1 ms is slow path only. */
#ifndef APP_CURRENT_LOOP_HZ
#define APP_CURRENT_LOOP_HZ 20000u
#endif
#define APP_CURRENT_LOOP_DT_S (1.0f / (float)APP_CURRENT_LOOP_HZ)

void AppConverter_Init(void);

/* Injected ISR: SOGI-PLL on U1, then PI; PWM only if PLL locked (no RAMPGEN). */
void AppConverter_CurrentLoopFromInjected(uint16_t current_adc,
                                          uint16_t plant_adc_u1,
                                          uint32_t now_ms);

/* 1 kHz: refresh bus/temp from regular DMA, command timeout / Modbus. No current PI. */
void AppConverter_1msTask(uint32_t now_ms);

void AppConverter_SetCurrentMilliamp(int16_t current_ma, uint32_t now_ms);
bool AppConverter_Arm(uint32_t now_ms);
void AppConverter_Disarm(void);
bool AppConverter_ClearFaults(void);
void AppConverter_EmergencyStop(void);

const converter_runtime_t *AppConverter_GetRuntime(void);
const volatile uint16_t *AppConverter_GetAdcDmaBuffer(void);
uint32_t AppConverter_GetCurrentLoopCount(void);

#ifdef __cplusplus
}
#endif

#endif
