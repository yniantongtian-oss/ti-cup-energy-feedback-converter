#ifndef CONVERTER_RUNTIME_H
#define CONVERTER_RUNTIME_H

#include "converter.h"
#include "sogi_pll.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float current_gain_a_per_count;
    float current_offset_a;
    float bus_gain_v_per_count;
    float bus_offset_v;
    float plant_gain_v_per_count;
    float plant_offset_v;
    float temperature_gain_c_per_count;
    float temperature_offset_c;
    float filter_alpha;
    uint32_t command_timeout_ms;
    uint16_t adc_full_scale;
    bool pll_gate_pwm;              /* default true: unlock => duty 0 */
} converter_runtime_config_t;

typedef struct {
    uint16_t current_adc;
    uint16_t bus_adc;
    uint16_t plant_adc;             /* U1 */
    uint16_t temperature_adc;
    bool sample_valid;
    bool hardware_fault;
} converter_raw_sample_t;

typedef struct {
    converter_t controller;
    converter_config_t control_config;
    converter_runtime_config_t runtime_config;
    converter_measurement_t measurement;
    sogi_pll_config_t pll_config;
    sogi_pll_t pll;
    float i_sogi_alpha;             /* current SOGI for Park i_beta */
    float i_sogi_beta;
    uint32_t last_command_ms;
    bool command_received;
    bool filter_initialized;
    bool pwm_released;
} converter_runtime_t;

converter_runtime_config_t converter_runtime_default_config(void);
bool converter_runtime_config_is_valid(const converter_runtime_config_t *config);

void converter_runtime_init(converter_runtime_t *runtime,
                            const converter_config_t *control_config,
                            const converter_runtime_config_t *runtime_config);

void converter_runtime_set_reference(converter_runtime_t *runtime,
                                     float current_a,
                                     uint32_t now_ms);
void converter_runtime_arm(converter_runtime_t *runtime, uint32_t now_ms);
void converter_runtime_disarm(converter_runtime_t *runtime);

void converter_runtime_step(converter_runtime_t *runtime,
                            const converter_raw_sample_t *raw,
                            uint32_t now_ms,
                            float dt_s);

bool converter_runtime_clear_faults(converter_runtime_t *runtime,
                                    const converter_raw_sample_t *raw);

bool converter_runtime_pll_locked(const converter_runtime_t *runtime);
float converter_runtime_pll_theta_rad(const converter_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif
