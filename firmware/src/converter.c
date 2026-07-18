#include "converter.h"

#include <math.h>
#include <stddef.h>

static float clampf(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static bool finite_measurement(const converter_measurement_t *measurement) {
    return measurement != NULL && measurement->sample_valid &&
           isfinite(measurement->input_current_a) &&
           isfinite(measurement->bus_voltage_v) &&
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
    converter_config_t config = {
        .kp = 0.08f,
        .ki = 8.0f,
        .current_limit_a = 2.0f,
        .current_trip_a = 2.5f,
        .bus_voltage_min_v = 8.0f,
        .bus_voltage_max_v = 30.0f,
        .temperature_trip_c = 70.0f,
        .duty_limit = 0.85f,
        .current_slew_a_per_s = 5.0f,
        .integrator_limit = 0.75f,
    };
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
           isfinite(config->current_slew_a_per_s) && config->current_slew_a_per_s > 0.0f &&
           isfinite(config->integrator_limit) && config->integrator_limit >= 0.0f;
}

void converter_init(converter_t *converter) {
    if (converter == NULL) return;
    converter->state = CONVERTER_STATE_IDLE;
    converter->faults = CONVERTER_FAULT_NONE;
    converter->armed = false;
    converter->requested_current_a = 0.0f;
    converter->ramped_current_a = 0.0f;
    converter->integrator = 0.0f;
    converter->duty_command = 0.0f;
}

void converter_arm(converter_t *converter) {
    if (converter != NULL && converter->faults == CONVERTER_FAULT_NONE) converter->armed = true;
}

void converter_disarm(converter_t *converter) {
    if (converter == NULL) return;
    converter->armed = false;
    converter->state = converter->faults == CONVERTER_FAULT_NONE ? CONVERTER_STATE_IDLE : CONVERTER_STATE_FAULT;
    converter->ramped_current_a = 0.0f;
    converter->integrator = 0.0f;
    converter->duty_command = 0.0f;
}

void converter_set_current_reference(converter_t *converter, float current_a) {
    if (converter != NULL && isfinite(current_a)) converter->requested_current_a = current_a;
}

void converter_step(converter_t *converter,
                    const converter_config_t *config,
                    const converter_measurement_t *measurement,
                    float dt_s) {
    if (converter == NULL || !converter_config_is_valid(config)) return;

    const uint32_t new_faults = evaluate_faults(config, measurement, dt_s);
    if (new_faults != CONVERTER_FAULT_NONE) {
        converter->faults |= new_faults;
        converter->state = CONVERTER_STATE_FAULT;
        converter->armed = false;
        converter->duty_command = 0.0f;
        converter->integrator = 0.0f;
        return;
    }

    if (!converter->armed || converter->faults != CONVERTER_FAULT_NONE) {
        converter->state = converter->faults == CONVERTER_FAULT_NONE ? CONVERTER_STATE_IDLE : CONVERTER_STATE_FAULT;
        converter->duty_command = 0.0f;
        converter->integrator = 0.0f;
        converter->ramped_current_a = 0.0f;
        return;
    }

    converter->state = CONVERTER_STATE_RUN;
    const float target = clampf(converter->requested_current_a, -config->current_limit_a, config->current_limit_a);
    const float max_delta = config->current_slew_a_per_s * dt_s;
    const float delta = clampf(target - converter->ramped_current_a, -max_delta, max_delta);
    converter->ramped_current_a += delta;

    const float error = converter->ramped_current_a - measurement->input_current_a;
    const float candidate_integrator = clampf(converter->integrator + config->ki * error * dt_s,
                                              -config->integrator_limit,
                                              config->integrator_limit);
    const float unsaturated = config->kp * error + candidate_integrator;
    const float output = clampf(unsaturated, -config->duty_limit, config->duty_limit);
    if (output == unsaturated ||
        (output >= config->duty_limit && error < 0.0f) ||
        (output <= -config->duty_limit && error > 0.0f)) {
        converter->integrator = candidate_integrator;
    }
    converter->duty_command = output;
}

bool converter_clear_faults(converter_t *converter,
                            const converter_config_t *config,
                            const converter_measurement_t *measurement) {
    if (converter == NULL || !converter_config_is_valid(config) || converter->armed) return false;
    if (evaluate_faults(config, measurement, 0.001f) != CONVERTER_FAULT_NONE) return false;
    converter->faults = CONVERTER_FAULT_NONE;
    converter->state = CONVERTER_STATE_IDLE;
    converter->integrator = 0.0f;
    converter->ramped_current_a = 0.0f;
    converter->duty_command = 0.0f;
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
