#include "converter_runtime.h"

#include "protection.h"

#include <math.h>
#include <stddef.h>

static float calibrated_count(uint16_t raw, float gain, float offset) {
    /* Two statements so Clang cannot contract this into an FMA. The fused
     * result differs in the last bits (750 * 0.1 - 50 is not exactly 25). */
    const float scaled = (float)raw * gain;
    return scaled + offset;
}

static bool adc_counts_valid(const converter_runtime_t *runtime, const converter_raw_sample_t *raw) {
    if (runtime == NULL || raw == NULL || !raw->sample_valid) {
        return false;
    }
    return raw->current_adc <= runtime->runtime_config.adc_full_scale &&
           raw->bus_adc <= runtime->runtime_config.adc_full_scale &&
           raw->temperature_adc <= runtime->runtime_config.adc_full_scale;
}

static converter_measurement_t calibrate(const converter_runtime_t *runtime,
                                         const converter_raw_sample_t *raw) {
    converter_measurement_t converted;
    const converter_runtime_config_t *config = &runtime->runtime_config;

    converted.input_current_a = calibrated_count(raw->current_adc,
                                                 config->current_gain_a_per_count,
                                                 config->current_offset_a);
    converted.bus_voltage_v = calibrated_count(raw->bus_adc,
                                               config->bus_gain_v_per_count,
                                               config->bus_offset_v);
    converted.temperature_c = calibrated_count(raw->temperature_adc,
                                               config->temperature_gain_c_per_count,
                                               config->temperature_offset_c);
    converted.sample_valid = true;
    return converted;
}

static void filter_measurement(converter_runtime_t *runtime, const converter_measurement_t *instant) {
    const float alpha = runtime->runtime_config.filter_alpha;

    if (!runtime->filter_initialized) {
        runtime->measurement = *instant;
        runtime->filter_initialized = true;
        return;
    }
    runtime->measurement.input_current_a +=
        alpha * (instant->input_current_a - runtime->measurement.input_current_a);
    runtime->measurement.bus_voltage_v +=
        alpha * (instant->bus_voltage_v - runtime->measurement.bus_voltage_v);
    runtime->measurement.temperature_c +=
        alpha * (instant->temperature_c - runtime->measurement.temperature_c);
    runtime->measurement.sample_valid = true;
}

converter_runtime_config_t converter_runtime_default_config(void) {
    converter_runtime_config_t config;
    config.current_gain_a_per_count = 0.001f;
    config.current_offset_a = -2.048f;
    config.bus_gain_v_per_count = 0.01f;
    config.bus_offset_v = 0.0f;
    config.temperature_gain_c_per_count = 0.1f;
    config.temperature_offset_c = -50.0f;
    config.filter_alpha = 0.2f;
    config.command_timeout_ms = 500u;
    config.adc_full_scale = 4095u;
    return config;
}

bool converter_runtime_config_is_valid(const converter_runtime_config_t *config) {
    if (config == NULL) {
        return false;
    }
    return isfinite(config->current_gain_a_per_count) &&
           isfinite(config->current_offset_a) &&
           isfinite(config->bus_gain_v_per_count) && config->bus_gain_v_per_count > 0.0f &&
           isfinite(config->bus_offset_v) &&
           isfinite(config->temperature_gain_c_per_count) &&
           isfinite(config->temperature_offset_c) &&
           isfinite(config->filter_alpha) && config->filter_alpha > 0.0f && config->filter_alpha <= 1.0f &&
           config->command_timeout_ms > 0u && config->adc_full_scale > 0u;
}

void converter_runtime_init(converter_runtime_t *runtime,
                            const converter_config_t *control_config,
                            const converter_runtime_config_t *runtime_config) {
    if (runtime == NULL) {
        return;
    }
    runtime->control_config = control_config != NULL ? *control_config : converter_default_config();
    runtime->runtime_config = runtime_config != NULL ? *runtime_config : converter_runtime_default_config();
    converter_init(&runtime->controller);
    runtime->measurement.input_current_a = 0.0f;
    runtime->measurement.bus_voltage_v = 0.0f;
    runtime->measurement.temperature_c = 0.0f;
    runtime->measurement.sample_valid = false;
    runtime->last_command_ms = 0u;
    runtime->command_received = false;
    runtime->filter_initialized = false;
}

void converter_runtime_set_reference(converter_runtime_t *runtime,
                                     float current_a,
                                     uint32_t now_ms) {
    if (runtime == NULL || !isfinite(current_a)) {
        return;
    }
    converter_set_current_reference(&runtime->controller, current_a);
    runtime->last_command_ms = now_ms;
    runtime->command_received = true;
}

void converter_runtime_arm(converter_runtime_t *runtime, uint32_t now_ms) {
    if (runtime == NULL) {
        return;
    }
    runtime->last_command_ms = now_ms;
    runtime->command_received = true;
    converter_arm(&runtime->controller);
}

void converter_runtime_disarm(converter_runtime_t *runtime) {
    if (runtime == NULL) {
        return;
    }
    converter_disarm(&runtime->controller);
    runtime->command_received = false;
}

void converter_runtime_emergency_stop(converter_runtime_t *runtime) {
    if (runtime == NULL) {
        return;
    }
    runtime->controller.faults |= CONVERTER_FAULT_ESTOP;
    runtime->command_received = false;
    converter_disarm(&runtime->controller);
}

bool converter_runtime_acknowledge_estop(converter_runtime_t *runtime) {
    if (runtime == NULL || (runtime->controller.faults & CONVERTER_FAULT_ESTOP) == 0u) {
        return false;
    }
    runtime->controller.faults &= ~CONVERTER_FAULT_ESTOP;
    if (runtime->controller.faults == CONVERTER_FAULT_NONE) {
        runtime->controller.state = CONVERTER_STATE_IDLE;
    }
    return true;
}

void converter_runtime_step(converter_runtime_t *runtime,
                            const converter_raw_sample_t *raw,
                            uint32_t now_ms,
                            float dt_s) {
    converter_measurement_t invalid;
    converter_measurement_t instant;
    uint32_t instant_faults;

    if (runtime == NULL) {
        return;
    }
    if (!converter_config_is_valid(&runtime->control_config) ||
        !converter_runtime_config_is_valid(&runtime->runtime_config)) {
        converter_disarm(&runtime->controller);
        return;
    }

    if (runtime->command_received &&
        (uint32_t)(now_ms - runtime->last_command_ms) >= runtime->runtime_config.command_timeout_ms) {
        const bool was_armed = runtime->controller.armed;
        converter_set_current_reference(&runtime->controller, 0.0f);
        runtime->command_received = false;
        if (was_armed) {
            runtime->controller.faults |= CONVERTER_FAULT_COMM_TIMEOUT;
            runtime->controller.armed = false;
        }
    }

    if (raw != NULL && raw->hardware_fault) {
        runtime->controller.faults |= CONVERTER_FAULT_HARDWARE;
        runtime->controller.armed = false;
    }

    if (!adc_counts_valid(runtime, raw)) {
        invalid.input_current_a = 0.0f;
        invalid.bus_voltage_v = 0.0f;
        invalid.temperature_c = 0.0f;
        invalid.sample_valid = false;
        runtime->measurement.sample_valid = false;
        converter_step(&runtime->controller, &runtime->control_config, &invalid, dt_s);
        return;
    }

    /* Trip on the calibrated sample. The first-order filter is only the
     * regulator feedback, so a one-sample spike cannot hide under the trip.
     */
    instant = calibrate(runtime, raw);
    instant_faults = protection_evaluate(&runtime->control_config, &instant, dt_s);
    if (runtime->controller.faults != CONVERTER_FAULT_NONE || instant_faults != CONVERTER_FAULT_NONE) {
        runtime->measurement = instant;
        runtime->filter_initialized = true;
        converter_step(&runtime->controller, &runtime->control_config, &instant, dt_s);
        return;
    }

    filter_measurement(runtime, &instant);
    converter_step(&runtime->controller, &runtime->control_config, &runtime->measurement, dt_s);
}

bool converter_runtime_clear_faults(converter_runtime_t *runtime,
                                    const converter_raw_sample_t *raw) {
    converter_measurement_t instant;

    if (runtime == NULL || !adc_counts_valid(runtime, raw) || raw->hardware_fault) {
        return false;
    }
    if ((runtime->controller.faults & CONVERTER_FAULT_ESTOP) != 0u) {
        return false;
    }
    instant = calibrate(runtime, raw);
    if (!converter_clear_faults(&runtime->controller, &runtime->control_config, &instant)) {
        return false;
    }
    runtime->measurement = instant;
    runtime->filter_initialized = true;
    return true;
}
