#include "converter_runtime.h"

#include <math.h>
#include <stddef.h>

static float calibrated(uint16_t raw, float gain, float offset) {
    return (float)raw * gain + offset;
}

static bool raw_sample_is_valid(const converter_runtime_t *runtime,
                                const converter_raw_sample_t *raw) {
    if (runtime == NULL || raw == NULL || !raw->sample_valid || raw->hardware_fault) return false;
    return raw->current_adc <= runtime->runtime_config.adc_full_scale &&
           raw->bus_adc <= runtime->runtime_config.adc_full_scale &&
           raw->plant_adc <= runtime->runtime_config.adc_full_scale &&
           raw->temperature_adc <= runtime->runtime_config.adc_full_scale;
}

static converter_measurement_t convert_sample(converter_runtime_t *runtime,
                                              const converter_raw_sample_t *raw) {
    converter_measurement_t converted;
    const converter_runtime_config_t *config = &runtime->runtime_config;

    converted.input_current_a = calibrated(raw->current_adc,
                                           config->current_gain_a_per_count,
                                           config->current_offset_a);
    converted.current_beta_a = 0.0f;
    converted.theta_rad = 0.0f;
    converted.theta_valid = false;
    converted.bus_voltage_v = calibrated(raw->bus_adc,
                                         config->bus_gain_v_per_count,
                                         config->bus_offset_v);
    converted.plant_voltage_v = calibrated(raw->plant_adc,
                                           config->plant_gain_v_per_count,
                                           config->plant_offset_v);
    converted.temperature_c = calibrated(raw->temperature_adc,
                                         config->temperature_gain_c_per_count,
                                         config->temperature_offset_c);
    converted.sample_valid = true;

    if (!runtime->filter_initialized) {
        runtime->measurement = converted;
        runtime->filter_initialized = true;
    } else {
        const float alpha = config->filter_alpha;
        runtime->measurement.input_current_a +=
            alpha * (converted.input_current_a - runtime->measurement.input_current_a);
        runtime->measurement.bus_voltage_v +=
            alpha * (converted.bus_voltage_v - runtime->measurement.bus_voltage_v);
        runtime->measurement.plant_voltage_v +=
            alpha * (converted.plant_voltage_v - runtime->measurement.plant_voltage_v);
        runtime->measurement.temperature_c +=
            alpha * (converted.temperature_c - runtime->measurement.temperature_c);
        runtime->measurement.sample_valid = true;
    }
    return runtime->measurement;
}

converter_runtime_config_t converter_runtime_default_config(void) {
    converter_runtime_config_t config;
    config.current_gain_a_per_count = 0.001f;
    config.current_offset_a = -2.048f;
    config.bus_gain_v_per_count = 0.01f;
    config.bus_offset_v = 0.0f;
    config.plant_gain_v_per_count = 0.01f;
    config.plant_offset_v = -20.48f;
    config.temperature_gain_c_per_count = 0.1f;
    config.temperature_offset_c = -50.0f;
    config.filter_alpha = 0.2f;
    config.command_timeout_ms = 500u;
    config.adc_full_scale = 4095u;
    config.pll_gate_pwm = true;
    config.bringup_enable = true;
    return config;
}

bool converter_runtime_config_is_valid(const converter_runtime_config_t *config) {
    if (config == NULL) return false;
    return isfinite(config->current_gain_a_per_count) &&
           isfinite(config->current_offset_a) &&
           isfinite(config->bus_gain_v_per_count) && config->bus_gain_v_per_count > 0.0f &&
           isfinite(config->bus_offset_v) &&
           isfinite(config->plant_gain_v_per_count) &&
           isfinite(config->plant_offset_v) &&
           isfinite(config->temperature_gain_c_per_count) &&
           isfinite(config->temperature_offset_c) &&
           isfinite(config->filter_alpha) && config->filter_alpha > 0.0f && config->filter_alpha <= 1.0f &&
           config->command_timeout_ms > 0u && config->adc_full_scale > 0u;
}

void converter_runtime_init(converter_runtime_t *runtime,
                            const converter_config_t *control_config,
                            const converter_runtime_config_t *runtime_config) {
    if (runtime == NULL) return;
    runtime->control_config = control_config != NULL ? *control_config : converter_default_config();
    runtime->runtime_config = runtime_config != NULL ? *runtime_config : converter_runtime_default_config();
    converter_init(&runtime->controller);
    runtime->pll_config = sogi_pll_default_config();
    sogi_pll_init(&runtime->pll, &runtime->pll_config);
    bringup_init(&runtime->bringup, NULL);
    runtime->measurement.input_current_a = 0.0f;
    runtime->measurement.current_beta_a = 0.0f;
    runtime->measurement.bus_voltage_v = 0.0f;
    runtime->measurement.plant_voltage_v = 0.0f;
    runtime->measurement.temperature_c = 0.0f;
    runtime->measurement.theta_rad = 0.0f;
    runtime->measurement.theta_valid = false;
    runtime->measurement.sample_valid = false;
    runtime->i_sogi_alpha = 0.0f;
    runtime->i_sogi_beta = 0.0f;
    runtime->last_command_ms = 0u;
    runtime->command_received = false;
    runtime->filter_initialized = false;
    runtime->pwm_released = false;
}

void converter_runtime_set_reference(converter_runtime_t *runtime,
                                     float current_a,
                                     uint32_t now_ms) {
    if (runtime == NULL || !isfinite(current_a)) return;
    converter_set_current_reference(&runtime->controller, current_a);
    runtime->last_command_ms = now_ms;
    runtime->command_received = true;
}

void converter_runtime_arm(converter_runtime_t *runtime, uint32_t now_ms) {
    if (runtime == NULL) return;
    runtime->last_command_ms = now_ms;
    runtime->command_received = true;
    converter_arm(&runtime->controller);
    if (runtime->runtime_config.bringup_enable) {
        bringup_request_start(&runtime->bringup);
    }
}

void converter_runtime_disarm(converter_runtime_t *runtime) {
    if (runtime == NULL) return;
    converter_disarm(&runtime->controller);
    runtime->command_received = false;
    runtime->pwm_released = false;
    bringup_request_stop(&runtime->bringup);
}

void converter_runtime_step(converter_runtime_t *runtime,
                            const converter_raw_sample_t *raw,
                            uint32_t now_ms,
                            float dt_s) {
    converter_measurement_t invalid;
    if (runtime == NULL) return;

    if (!converter_config_is_valid(&runtime->control_config) ||
        !converter_runtime_config_is_valid(&runtime->runtime_config) ||
        !sogi_pll_config_is_valid(&runtime->pll_config)) {
        converter_disarm(&runtime->controller);
        runtime->pwm_released = false;
        return;
    }

    if (runtime->command_received &&
        (uint32_t)(now_ms - runtime->last_command_ms) > runtime->runtime_config.command_timeout_ms) {
        converter_set_current_reference(&runtime->controller, 0.0f);
        converter_disarm(&runtime->controller);
        runtime->command_received = false;
        runtime->pwm_released = false;
    }

    if (!raw_sample_is_valid(runtime, raw)) {
        invalid.input_current_a = 0.0f;
        invalid.current_beta_a = 0.0f;
        invalid.bus_voltage_v = 0.0f;
        invalid.plant_voltage_v = 0.0f;
        invalid.temperature_c = 0.0f;
        invalid.theta_rad = 0.0f;
        invalid.theta_valid = false;
        invalid.sample_valid = false;
        converter_step(&runtime->controller, &runtime->control_config, &invalid, dt_s);
        runtime->pwm_released = false;
        runtime->controller.duty_command = 0.0f;
        return;
    }

    runtime->measurement = convert_sample(runtime, raw);

    /* Sync on U1 only — never a software ramp angle. */
    sogi_pll_step(&runtime->pll, &runtime->pll_config,
                  runtime->measurement.plant_voltage_v, dt_s);

    /* Current SOGI → i_beta for Park (same omega as PLL). No harmonic PR. */
    {
        const float w = runtime->pll.omega_rad_s;
        const float k = runtime->pll_config.k_sogi;
        const float i_a = runtime->measurement.input_current_a;
        const float d_a = k * (i_a - runtime->i_sogi_alpha) * w - runtime->i_sogi_beta * w;
        const float d_b = runtime->i_sogi_alpha * w;
        runtime->i_sogi_alpha += d_a * dt_s;
        runtime->i_sogi_beta += d_b * dt_s;
        runtime->measurement.current_beta_a = runtime->i_sogi_beta;
        runtime->measurement.theta_rad = runtime->pll.theta_rad;
        runtime->measurement.theta_valid = runtime->pll.locked;
    }

    /* Bring-up may own Id_ref when enabled. */
    if (runtime->runtime_config.bringup_enable &&
        runtime->bringup.current_enable &&
        !runtime->bringup.force_open_loop) {
        converter_set_current_reference(&runtime->controller, runtime->bringup.id_ref_a);
    }

    converter_step(&runtime->controller,
                   &runtime->control_config,
                   &runtime->measurement,
                   dt_s);

    if (runtime->runtime_config.bringup_enable) {
        if (runtime->bringup.force_open_loop) {
            runtime->controller.duty_command = runtime->bringup.open_loop_duty_cmd;
        } else if (!runtime->bringup.current_enable) {
            runtime->controller.duty_command = 0.0f;
            runtime->controller.voltage_command = 0.0f;
        }
        if (runtime->bringup.mode == BRINGUP_FAULT) {
            runtime->controller.duty_command = 0.0f;
        }
    }

    if (runtime->runtime_config.pll_gate_pwm) {
        runtime->pwm_released = runtime->pll.locked;
        if (!runtime->pll.locked) {
            runtime->controller.duty_command = 0.0f;
            runtime->controller.voltage_command = 0.0f;
        }
    } else {
        runtime->pwm_released = true;
    }
}

void converter_runtime_bringup_1khz(converter_runtime_t *runtime, uint32_t dt_ms) {
    bool hw_ok;
    if (runtime == NULL) return;
    if (!runtime->runtime_config.bringup_enable) return;
    hw_ok = runtime->controller.faults == CONVERTER_FAULT_NONE;
    bringup_step_1khz(&runtime->bringup,
                      runtime->measurement.bus_voltage_v,
                      runtime->pll.amplitude_v,
                      runtime->pll.locked,
                      hw_ok,
                      dt_ms);
    if (runtime->bringup.mode == BRINGUP_FAULT) {
        converter_disarm(&runtime->controller);
        runtime->pwm_released = false;
    }
}

bringup_mode_t converter_runtime_bringup_mode(const converter_runtime_t *runtime) {
    return runtime != NULL ? runtime->bringup.mode : BRINGUP_IDLE;
}

bool converter_runtime_clear_faults(converter_runtime_t *runtime,
                                    const converter_raw_sample_t *raw) {
    if (runtime == NULL || !raw_sample_is_valid(runtime, raw)) return false;
    runtime->measurement = convert_sample(runtime, raw);
    return converter_clear_faults(&runtime->controller,
                                  &runtime->control_config,
                                  &runtime->measurement);
}

bool converter_runtime_pll_locked(const converter_runtime_t *runtime) {
    return runtime != NULL && runtime->pll.locked;
}

float converter_runtime_pll_theta_rad(const converter_runtime_t *runtime) {
    return runtime != NULL ? runtime->pll.theta_rad : 0.0f;
}
