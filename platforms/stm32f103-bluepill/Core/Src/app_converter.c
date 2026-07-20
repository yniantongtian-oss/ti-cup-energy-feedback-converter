#include "app_converter.h"

#include "main.h"

#include <math.h>

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

static converter_runtime_t g_runtime;
static volatile uint16_t g_adc_dma[3] = {0u, 0u, 0u};
static bool g_emergency_latched = false;

/* Incremented by AppConverter_AdcDmaUpdate() in the DMA completion callback.
 * Read and compared in AppConverter_1msTask() to detect stale ADC samples.
 * Must be the same width as the compare variable to handle wrap-around. */
static volatile uint32_t g_adc_dma_count = 0u;
static uint32_t g_adc_dma_count_last = 0u;

/** Maximum number of consecutive 1 ms ticks without a new ADC conversion
 *  before a sample is considered stale.  Reserved for future use. */
#define ADC_FRESHNESS_TICKS_MAX (5u)

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

    /* Never command both power-flow directions at the same time. */
    if (duty > 0.0f) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0u);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);
    } else {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, compare);
    }
}

static converter_raw_sample_t current_raw_sample(void) {
    converter_raw_sample_t raw;
    /* Snapshot the DMA counter; if it has not advanced since the last call
     * the ADC conversion has stalled and the sample must be treated as invalid. */
    uint32_t current_count = g_adc_dma_count;
    bool fresh = (current_count != g_adc_dma_count_last);
    g_adc_dma_count_last = current_count;

    raw.current_adc    = g_adc_dma[0];
    raw.bus_adc        = g_adc_dma[1];
    raw.temperature_adc = g_adc_dma[2];
    raw.sample_valid   = fresh;
    raw.hardware_fault = !hardware_inputs_safe();
    return raw;
}

void AppConverter_Init(void) {
    converter_config_t control = converter_default_config();
    converter_runtime_config_t runtime = converter_runtime_default_config();

    /* Replace these calibration values with measurements from your own board. */
    runtime.current_gain_a_per_count = 0.001f;
    runtime.current_offset_a = -2.048f;
    runtime.bus_gain_v_per_count = 0.01f;
    runtime.bus_offset_v = 0.0f;
    runtime.temperature_gain_c_per_count = 0.1f;
    runtime.temperature_offset_c = -50.0f;
    runtime.command_timeout_ms = 500u;

    converter_runtime_init(&g_runtime, &control, &runtime);
    g_emergency_latched = false;
    force_pwm_off();

    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    force_pwm_off();
    (void)HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_dma, 3u);
}

void AppConverter_1msTask(uint32_t now_ms) {
    converter_raw_sample_t raw = current_raw_sample();
    converter_runtime_step(&g_runtime, &raw, now_ms, 0.001f);
    apply_signed_duty(g_runtime.controller.duty_command);
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
    raw = current_raw_sample();
    return converter_runtime_clear_faults(&g_runtime, &raw);
}

void AppConverter_EmergencyStop(void) {
    g_emergency_latched = true;
    converter_runtime_disarm(&g_runtime);
    force_pwm_off();
}

void AppConverter_AdcDmaUpdate(void) {
    g_adc_dma_count++;
}

const converter_runtime_t *AppConverter_GetRuntime(void) {
    return &g_runtime;
}

const volatile uint16_t *AppConverter_GetAdcDmaBuffer(void) {
    return g_adc_dma;
}
