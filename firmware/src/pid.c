#include "pid.h"

#include "converter_math.h"

#include <math.h>
#include <stddef.h>

static const float PID_TWO_PI = 6.28318530717958647692f;

bool pid_gains_valid(const pid_gains_t *gains) {
    if (gains == NULL) {
        return false;
    }
    return isfinite(gains->kp) && gains->kp >= 0.0f &&
           isfinite(gains->ki) && gains->ki >= 0.0f &&
           isfinite(gains->kd) && gains->kd >= 0.0f &&
           isfinite(gains->output_limit) && gains->output_limit > 0.0f &&
           isfinite(gains->integrator_limit) && gains->integrator_limit >= 0.0f &&
           isfinite(gains->derivative_filter_hz) && gains->derivative_filter_hz >= 0.0f;
}

void pid_reset(pid_state_t *state) {
    if (state == NULL) {
        return;
    }
    state->integrator = 0.0f;
    state->derivative = 0.0f;
    state->previous_measurement = 0.0f;
    state->initialized = false;
}

float pid_step(pid_state_t *state,
               const pid_gains_t *gains,
               float reference,
               float measurement,
               float dt_s) {
    float error;
    float candidate;
    float derivative;
    float unsaturated;
    float output;
    bool saturated_high;
    bool saturated_low;
    bool winding_up;

    if (state == NULL || !pid_gains_valid(gains) || !isfinite(reference) ||
        !isfinite(measurement) || !isfinite(dt_s) || dt_s <= 0.0f) {
        return 0.0f;
    }

    error = reference - measurement;
    candidate = converter_clampf(state->integrator + gains->ki * error * dt_s,
                                 -gains->integrator_limit,
                                 gains->integrator_limit);

    derivative = 0.0f;
    if (state->initialized) {
        const float raw_derivative = (measurement - state->previous_measurement) / dt_s;
        if (gains->derivative_filter_hz > 0.0f) {
            const float tau = 1.0f / (PID_TWO_PI * gains->derivative_filter_hz);
            const float alpha = dt_s / (tau + dt_s);
            state->derivative += alpha * (raw_derivative - state->derivative);
            derivative = state->derivative;
        } else {
            state->derivative = raw_derivative;
            derivative = raw_derivative;
        }
    } else {
        state->derivative = 0.0f;
    }
    state->previous_measurement = measurement;
    state->initialized = true;

    unsaturated = gains->kp * error + candidate - gains->kd * derivative;
    output = converter_clampf(unsaturated, -gains->output_limit, gains->output_limit);
    saturated_high = unsaturated > gains->output_limit;
    saturated_low = unsaturated < -gains->output_limit;
    winding_up = (saturated_high && error > 0.0f) || (saturated_low && error < 0.0f);
    if (!winding_up) {
        state->integrator = candidate;
    }
    return output;
}
