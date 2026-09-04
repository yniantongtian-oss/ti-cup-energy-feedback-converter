#include "converter.h"

#include <math.h>
#include <stddef.h>

static float clampf(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float pi_step(float *integ, float kp, float ki, float err, float dt,
                     float lim_out, float lim_i) {
    float candidate = clampf(*integ + ki * err * dt, -lim_i, lim_i);
    float unsaturated = kp * err + candidate;
    float out = clampf(unsaturated, -lim_out, lim_out);
    if (out == unsaturated ||
        (out >= lim_out && err < 0.0f) ||
        (out <= -lim_out && err > 0.0f)) {
        *integ = candidate;
    }
    return out;
}

static bool finite_measurement(const converter_measurement_t *measurement) {
    return measurement != NULL && measurement->sample_valid &&
           isfinite(measurement->input_current_a) &&
           isfinite(measurement->current_beta_a) &&
           isfinite(measurement->bus_voltage_v) &&
           isfinite(measurement->plant_voltage_v) &&
           isfinite(measurement->temperature_c);
}

static uint32_t evaluate_faults(const converter_config_t *config,
                                const converter_measurement_t *measurement,
                                float dt_s) {
    uint32_t faults = CONVERTER_FAULT_NONE;
    if (!isfinite(dt_s) || dt_s <= 0.0f || dt_s > 1.0f) faults |= CONVERTER_FAULT_BAD_TIMESTEP;
    if (!finite_measurement(measurement)) {
        faults |= CONVERTER_FAULT_INVALID_SAMPLE;
        return faults;
    }
    if (fabsf(measurement->input_current_a) > config->current_trip_a) faults |= CONVERTER_FAULT_OVERCURRENT;
    if (measurement->bus_voltage_v > config->bus_voltage_max_v) faults |= CONVERTER_FAULT_OVERVOLTAGE;
    if (measurement->bus_voltage_v < config->bus_voltage_min_v) faults |= CONVERTER_FAULT_UNDERVOLTAGE;
    if (measurement->temperature_c > config->temperature_trip_c) faults |= CONVERTER_FAULT_OVERTEMPERATURE;
    return faults;
}

converter_config_t converter_default_config(void) {
    converter_config_t config;
    config.kp = 2.0f;
    config.ki = 200.0f;
    config.current_limit_a = 2.0f;
    config.current_trip_a = 2.5f;
    config.bus_voltage_min_v = 8.0f;
    config.bus_voltage_max_v = 30.0f;
    config.temperature_trip_c = 70.0f;
    config.duty_limit = 0.85f;
    config.voltage_command_limit = 20.0f;
    config.current_slew_a_per_s = 5.0f;
    config.integrator_limit = 15.0f;
    config.mod_bus_min_v = 1.0f;
    config.feedforward_enable = true;
    config.park_enable = true;
    return config;
}

bool converter_config_is_valid(const converter_config_t *config) {
    if (config == NULL) return false;
    return isfinite(config->kp) && config->kp >= 0.0f &&
           isfinite(config->ki) && config->ki >= 0.0f &&
           isfinite(config->current_limit_a) && config->current_limit_a > 0.0f &&
           isfinite(config->current_trip_a) && config->current_trip_a > config->current_limit_a &&
           isfinite(config->bus_voltage_min_v) && config->bus_voltage_min_v >= 0.0f &&
           isfinite(config->bus_voltage_max_v) && config->bus_voltage_max_v > config->bus_voltage_min_v &&
           isfinite(config->temperature_trip_c) && config->temperature_trip_c > 0.0f &&
           isfinite(config->duty_limit) && config->duty_limit > 0.0f && config->duty_limit <= 1.0f &&
           isfinite(config->voltage_command_limit) && config->voltage_command_limit > 0.0f &&
           isfinite(config->current_slew_a_per_s) && config->current_slew_a_per_s > 0.0f &&
           isfinite(config->integrator_limit) && config->integrator_limit >= 0.0f &&
           isfinite(config->mod_bus_min_v) && config->mod_bus_min_v > 0.0f;
}

void converter_init(converter_t *converter) {
    if (converter == NULL) return;
    converter->state = CONVERTER_STATE_IDLE;
    converter->faults = CONVERTER_FAULT_NONE;
    converter->armed = false;
    converter->requested_current_a = 0.0f;
    converter->ramped_current_a = 0.0f;
    converter->integrator_d = 0.0f;
    converter->integrator_q = 0.0f;
    converter->id_meas = 0.0f;
    converter->iq_meas = 0.0f;
    converter->ud = 0.0f;
    converter->uq = 0.0f;
    converter->voltage_command = 0.0f;
    converter->duty_command = 0.0f;
    converter->feedforward_active = false;
    converter->park_active = false;
}

void converter_arm(converter_t *converter) {
    if (converter != NULL && converter->faults == CONVERTER_FAULT_NONE) converter->armed = true;
}

void converter_disarm(converter_t *converter) {
    if (converter == NULL) return;
    converter->armed = false;
    converter->state = converter->faults == CONVERTER_FAULT_NONE ? CONVERTER_STATE_IDLE : CONVERTER_STATE_FAULT;
    converter->ramped_current_a = 0.0f;
    converter->integrator_d = 0.0f;
    converter->integrator_q = 0.0f;
    converter->ud = 0.0f;
    converter->uq = 0.0f;
    converter->voltage_command = 0.0f;
    converter->duty_command = 0.0f;
    converter->feedforward_active = false;
    converter->park_active = false;
}

void converter_set_current_reference(converter_t *converter, float current_a) {
    if (converter != NULL && isfinite(current_a)) converter->requested_current_a = current_a;
}

void converter_step(converter_t *converter,
                    const converter_config_t *config,
                    const converter_measurement_t *measurement,
                    float dt_s) {
    float u_alpha;

    if (converter == NULL || !converter_config_is_valid(config)) return;

    {
        const uint32_t new_faults = evaluate_faults(config, measurement, dt_s);
        if (new_faults != CONVERTER_FAULT_NONE) {
            converter->faults |= new_faults;
            converter->state = CONVERTER_STATE_FAULT;
            converter->armed = false;
            converter->voltage_command = 0.0f;
            converter->duty_command = 0.0f;
            converter->integrator_d = 0.0f;
            converter->integrator_q = 0.0f;
            converter->feedforward_active = false;
            converter->park_active = false;
            return;
        }
    }

    if (!converter->armed || converter->faults != CONVERTER_FAULT_NONE) {
        converter->state = converter->faults == CONVERTER_FAULT_NONE ? CONVERTER_STATE_IDLE : CONVERTER_STATE_FAULT;
        converter->voltage_command = 0.0f;
        converter->duty_command = 0.0f;
        converter->integrator_d = 0.0f;
        converter->integrator_q = 0.0f;
        converter->ramped_current_a = 0.0f;
        converter->feedforward_active = false;
        converter->park_active = false;
        return;
    }

    converter->state = CONVERTER_STATE_RUN;
    {
        const float target = clampf(converter->requested_current_a, -config->current_limit_a, config->current_limit_a);
        const float max_delta = config->current_slew_a_per_s * dt_s;
        const float delta = clampf(target - converter->ramped_current_a, -max_delta, max_delta);
        converter->ramped_current_a += delta;
    }

    if (config->park_enable && measurement->theta_valid && isfinite(measurement->theta_rad)) {
        float c = cosf(measurement->theta_rad);
        float s = sinf(measurement->theta_rad);
        float id_ref = converter->ramped_current_a;
        float iq_ref = 0.0f; /* Iq = 0, fundamental sync frame; no harmonic PR */
        float id = c * measurement->input_current_a + s * measurement->current_beta_a;
        float iq = -s * measurement->input_current_a + c * measurement->current_beta_a;
        float ud = pi_step(&converter->integrator_d, config->kp, config->ki,
                           id_ref - id, dt_s, config->voltage_command_limit, config->integrator_limit);
        float uq = pi_step(&converter->integrator_q, config->kp, config->ki,
                           iq_ref - iq, dt_s, config->voltage_command_limit, config->integrator_limit);
        u_alpha = c * ud - s * uq;
        converter->id_meas = id;
        converter->iq_meas = iq;
        converter->ud = ud;
        converter->uq = uq;
        converter->park_active = true;
    } else {
        /* Fallback stationary PI (tests / no theta). Still no PR. */
        float err = converter->ramped_current_a - measurement->input_current_a;
        u_alpha = pi_step(&converter->integrator_d, config->kp, config->ki, err, dt_s,
                          config->voltage_command_limit, config->integrator_limit);
        converter->integrator_q = 0.0f;
        converter->id_meas = measurement->input_current_a;
        converter->iq_meas = 0.0f;
        converter->ud = u_alpha;
        converter->uq = 0.0f;
        converter->park_active = false;
    }

    converter->voltage_command = clampf(u_alpha, -config->voltage_command_limit, config->voltage_command_limit);
    converter->feedforward_active = config->feedforward_enable;

    {
        const float vbus = measurement->bus_voltage_v;
        if (!(vbus > config->mod_bus_min_v)) {
            converter->duty_command = 0.0f;
            return;
        }
        {
            const float u1 = config->feedforward_enable ? measurement->plant_voltage_v : 0.0f;
            const float duty_raw = (converter->voltage_command + u1) / vbus;
            converter->duty_command = clampf(duty_raw, -config->duty_limit, config->duty_limit);
        }
    }
}

bool converter_clear_faults(converter_t *converter,
                            const converter_config_t *config,
                            const converter_measurement_t *measurement) {
    if (converter == NULL || !converter_config_is_valid(config) || converter->armed) return false;
    if (evaluate_faults(config, measurement, 0.001f) != CONVERTER_FAULT_NONE) return false;
    converter->faults = CONVERTER_FAULT_NONE;
    converter->state = CONVERTER_STATE_IDLE;
    converter->integrator_d = 0.0f;
    converter->integrator_q = 0.0f;
    converter->ramped_current_a = 0.0f;
    converter->voltage_command = 0.0f;
    converter->duty_command = 0.0f;
    converter->feedforward_active = false;
    converter->park_active = false;
    return true;
}

const char *converter_state_name(converter_state_t state) {
    switch (state) {
        case CONVERTER_STATE_IDLE: return "IDLE";
        case CONVERTER_STATE_RUN: return "RUN";
        case CONVERTER_STATE_FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}
