#include "app_converter.h"

#include "main.h"

#include <math.h>

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

static converter_runtime_t g_runtime;
/*
 * Regular group DMA (slow, ~1 kHz refresh path): IN1 bus, IN2 temp.
 * Injected group (TIM1 OCxREF, ~20 kHz): rank1=IN0 current, rank2=IN3 U1.
 * Do NOT put current/U1 only on regular continuous — that is not PWM-phase locked.
 */
static volatile uint16_t g_adc_dma[2] = {0u, 0u};
static volatile uint16_t g_slow_bus_adc = 0u;
static volatile uint16_t g_slow_temp_adc = 0u;
static volatile uint32_t g_current_loop_count = 0u;
static bool g_emergency_latched = false;

static bool hardware_inputs_safe(void) {
#if defined(HW_FAULT_GPIO_Port) && defined(HW_FAULT_Pin)
    if (HAL_GPIO_ReadPin(HW_FAULT_GPIO_Port, HW_FAULT_Pin) == GPIO_PIN_RESET) return false;
#endif
#if defined(ESTOP_GPIO_Port) && defined(ESTOP_Pin)
    if (HAL_GPIO_ReadPin(ESTOP_GPIO_Port, ESTOP_Pin) == GPIO_PIN_RESET) return false;
#endif
    return !g_emergency_latched;
}

static void force_pwm_off(void) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0u);
}

static void apply_signed_duty(float duty) {
    uint32_t period;
    uint32_t compare;

    if (!isfinite(duty) || duty == 0.0f || !hardware_inputs_safe()) {
        force_pwm_off();
        return;
    }

    period = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1u;
    compare = (uint32_t)(fabsf(duty) * (float)period);
    if (compare >= period) compare = period - 1u;

    if (duty > 0.0f) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0u);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);
    } else {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, compare);
    }
}

static converter_raw_sample_t make_raw(uint16_t current_adc, uint16_t plant_adc) {
    converter_raw_sample_t raw;
    raw.current_adc = current_adc;
    raw.plant_adc = plant_adc;
    raw.bus_adc = g_slow_bus_adc;
    raw.temperature_adc = g_slow_temp_adc;
    raw.sample_valid = true;
    raw.hardware_fault = !hardware_inputs_safe();
    return raw;
}

void AppConverter_Init(void) {
    converter_config_t control = converter_default_config();
    converter_runtime_config_t runtime = converter_runtime_default_config();

    runtime.current_gain_a_per_count = 0.001f;
    runtime.current_offset_a = -2.048f;
    runtime.bus_gain_v_per_count = 0.01f;
    runtime.bus_offset_v = 0.0f;
    runtime.plant_gain_v_per_count = 0.01f;
    runtime.plant_offset_v = -20.48f;
    runtime.temperature_gain_c_per_count = 0.1f;
    runtime.temperature_offset_c = -50.0f;
    runtime.command_timeout_ms = 500u;

    converter_runtime_init(&g_runtime, &control, &runtime);
    g_emergency_latched = false;
    g_current_loop_count = 0u;
    force_pwm_off();

    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    force_pwm_off();

    /* Regular: bus + temp only. Injected start is in CubeMX / MX_ADC1_Init side. */
    (void)HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_dma, 2u);
    (void)HAL_ADCEx_InjectedStart_IT(&hadc1);
}

void AppConverter_CurrentLoopFromInjected(uint16_t current_adc,
                                          uint16_t plant_adc_u1,
                                          uint32_t now_ms) {
    converter_raw_sample_t raw = make_raw(current_adc, plant_adc_u1);
    converter_runtime_step(&g_runtime, &raw, now_ms, APP_CURRENT_LOOP_DT_S);
    /* Step C: SOGI-PLL must lock U1 before PWM; runtime zeros duty if unlocked. */
    if (!g_runtime.pwm_released) {
        force_pwm_off();
    } else {
        apply_signed_duty(g_runtime.controller.duty_command);
    }
    g_current_loop_count++;
}

void AppConverter_1msTask(uint32_t now_ms) {
    /* Slow path only: cache bus/temp for the next injected ISR. No PI, no PWM write. */
    g_slow_bus_adc = g_adc_dma[0];
    g_slow_temp_adc = g_adc_dma[1];
    (void)now_ms;
    /* Command timeout still evaluated inside the 20 kHz step using now_ms. */
}

void AppConverter_SetCurrentMilliamp(int16_t current_ma, uint32_t now_ms) {
    converter_runtime_set_reference(&g_runtime, (float)current_ma / 1000.0f, now_ms);
}

bool AppConverter_Arm(uint32_t now_ms) {
    if (!hardware_inputs_safe() || g_runtime.controller.faults != CONVERTER_FAULT_NONE) return false;
    converter_runtime_arm(&g_runtime, now_ms);
    return g_runtime.controller.armed;
}

void AppConverter_Disarm(void) {
    converter_runtime_disarm(&g_runtime);
    force_pwm_off();
}

bool AppConverter_ClearFaults(void) {
    converter_raw_sample_t raw;
    if (!hardware_inputs_safe()) return false;
    raw = make_raw(0u, 2048u);
    raw.current_adc = 2048u;
    return converter_runtime_clear_faults(&g_runtime, &raw);
}

void AppConverter_EmergencyStop(void) {
    g_emergency_latched = true;
    converter_runtime_disarm(&g_runtime);
    force_pwm_off();
}

const converter_runtime_t *AppConverter_GetRuntime(void) {
    return &g_runtime;
}

const volatile uint16_t *AppConverter_GetAdcDmaBuffer(void) {
    return g_adc_dma;
}

uint32_t AppConverter_GetCurrentLoopCount(void) {
    return g_current_loop_count;
}

/*
 * Hook for Cube-generated stm32f1xx_it.c / HAL:
 *   void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc) {
 *     if (hadc->Instance != ADC1) return;
 *     AppConverter_CurrentLoopFromInjected(
 *         HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1),
 *         HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2),
 *         HAL_GetTick());
 *   }
 * Trigger must be TIM1 OCxREF (quiet point), NOT TIM1 Update (2×PWM on center-aligned).
 */
