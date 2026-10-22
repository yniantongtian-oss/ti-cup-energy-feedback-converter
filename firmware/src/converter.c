#include "converter.h"

#include "converter_math.h"
#include "protection.h"

#include <math.h>
#include <stddef.h>

static void stop_actuators(converter_t *converter) {
    converter->armed = false;
    converter->ramped_current_a = 0.0f;
    converter->duty_command = 0.0f;
    pid_reset(&converter->current_pid);
    converter->integrator = 0.0f;
}

static void latch_fault(converter_t *converter, uint32_t faults) {
    converter->faults |= faults;
    converter->state = CONVERTER_STATE_FAULT;
    stop_actuators(converter);
}

static pid_gains_t gains_from_config(const converter_config_t *config) {
    pid_gains_t gains;
    gains.kp = config->kp;
    gains.ki = config->ki;
    gains.kd = config->kd;
    gains.output_limit = config->duty_limit;
    gains.integrator_limit = config->integrator_limit;
    gains.derivative_filter_hz = config->derivative_filter_hz;
    return gains;
}

converter_config_t converter_default_config(void) {
    converter_config_t config;
    config.kp = 0.08f;
    config.ki = 8.0f;
    config.kd = 0.0f;
    config.current_limit_a = 2.0f;
    config.current_trip_a = 2.5f;
    config.bus_voltage_min_v = 8.0f;
    config.bus_voltage_max_v = 30.0f;
    config.temperature_trip_c = 70.0f;
    config.duty_limit = 0.85f;
    config.current_slew_a_per_s = 5.0f;
    config.integrator_limit = 0.75f;
    config.derivative_filter_hz = 0.0f;
    return config;
}

bool converter_config_is_valid(const converter_config_t *config) {
    pid_gains_t gains;
    if (config == NULL) {
        return false;
    }
    gains = gains_from_config(config);
    return pid_gains_valid(&gains) &&
           isfinite(config->current_limit_a) && config->current_limit_a > 0.0f &&
           isfinite(config->current_trip_a) && config->current_trip_a > config->current_limit_a &&
           isfinite(config->bus_voltage_min_v) && config->bus_voltage_min_v >= 0.0f &&
           isfinite(config->bus_voltage_max_v) && config->bus_voltage_max_v > config->bus_voltage_min_v &&
           isfinite(config->temperature_trip_c) && config->temperature_trip_c > 0.0f &&
           isfinite(config->current_slew_a_per_s) && config->current_slew_a_per_s > 0.0f;
}

void converter_init(converter_t *converter) {
    if (converter == NULL) {
        return;
    }
    converter->state = CONVERTER_STATE_IDLE;
    converter->faults = CONVERTER_FAULT_NONE;
    converter->armed = false;
    converter->requested_current_a = 0.0f;
    converter->ramped_current_a = 0.0f;
    converter->integrator = 0.0f;
    converter->duty_command = 0.0f;
    pid_reset(&converter->current_pid);
}

void converter_arm(converter_t *converter) {
    if (converter != NULL && converter->faults == CONVERTER_FAULT_NONE) {
        converter->armed = true;
    }
}

void converter_disarm(converter_t *converter) {
    if (converter == NULL) {
        return;
    }
    stop_actuators(converter);
    converter->state = converter->faults == CONVERTER_FAULT_NONE ? CONVERTER_STATE_IDLE
                                                                 : CONVERTER_STATE_FAULT;
}

void converter_set_current_reference(converter_t *converter, float current_a) {
    if (converter != NULL && isfinite(current_a)) {
        converter->requested_current_a = current_a;
    }
}

void converter_step(converter_t *converter,
                    const converter_config_t *config,
                    const converter_measurement_t *measurement,
                    float dt_s) {
    uint32_t new_faults;
    pid_gains_t gains;
    float target;
    float max_delta;
    float delta;

    if (converter == NULL) {
        return;
    }
    if (!converter_config_is_valid(config)) {
        stop_actuators(converter);
        converter->state = converter->faults == CONVERTER_FAULT_NONE ? CONVERTER_STATE_IDLE
                                                                     : CONVERTER_STATE_FAULT;
        return;
    }

    new_faults = protection_evaluate(config, measurement, dt_s);
    if (new_faults != CONVERTER_FAULT_NONE) {
        latch_fault(converter, new_faults);
        return;
    }
    if (converter->faults != CONVERTER_FAULT_NONE) {
        converter->state = CONVERTER_STATE_FAULT;
        stop_actuators(converter);
        return;
    }
    if (!converter->armed) {
        converter->state = CONVERTER_STATE_IDLE;
        stop_actuators(converter);
        return;
    }

    converter->state = CONVERTER_STATE_RUN;
    target = converter_clampf(converter->requested_current_a,
                              -config->current_limit_a,
                              config->current_limit_a);
    max_delta = config->current_slew_a_per_s * dt_s;
    delta = converter_clampf(target - converter->ramped_current_a, -max_delta, max_delta);
    converter->ramped_current_a += delta;

    gains = gains_from_config(config);
    converter->duty_command = pid_step(&converter->current_pid,
                                       &gains,
                                       converter->ramped_current_a,
                                       measurement->input_current_a,
                                       dt_s);
    converter->integrator = converter->current_pid.integrator;
}

bool converter_clear_faults(converter_t *converter,
                            const converter_config_t *config,
                            const converter_measurement_t *measurement) {
    if (converter == NULL || !converter_config_is_valid(config) || converter->armed) {
        return false;
    }
    if ((converter->faults & CONVERTER_FAULT_ESTOP) != 0u) {
        return false;
    }
    if (protection_evaluate(config, measurement, 0.001f) != CONVERTER_FAULT_NONE) {
        return false;
    }
    converter->faults = CONVERTER_FAULT_NONE;
    converter->state = CONVERTER_STATE_IDLE;
    stop_actuators(converter);
    return true;
}

const char *converter_state_name(converter_state_t state) {
    switch (state) {
        case CONVERTER_STATE_IDLE:
            return "IDLE";
        case CONVERTER_STATE_RUN:
            return "RUN";
        case CONVERTER_STATE_FAULT:
            return "FAULT";
        default:
            return "UNKNOWN";
    }
}
