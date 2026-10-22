#include "pll.h"

#include "converter_math.h"

#include <math.h>
#include <stddef.h>

static const float PLL_TWO_PI = 6.28318530717958647692f;

pll_config_t pll_default_config(void) {
    pll_config_t config;
    config.nominal_hz = 50.0f;
    config.kp = 40.0f;
    config.ki = 800.0f;
    config.sogi_k = 1.41421356f;
    config.frequency_limit_hz = 12.0f;
    config.lock_vq_ratio = 0.2f;
    config.lock_samples = 200u;
    return config;
}

bool pll_config_valid(const pll_config_t *config) {
    if (config == NULL) {
        return false;
    }
    return isfinite(config->nominal_hz) && config->nominal_hz > 0.0f &&
           config->nominal_hz < 10000.0f &&
           isfinite(config->kp) && config->kp >= 0.0f &&
           isfinite(config->ki) && config->ki >= 0.0f &&
           isfinite(config->sogi_k) && config->sogi_k > 0.0f &&
           isfinite(config->frequency_limit_hz) && config->frequency_limit_hz > 0.0f &&
           isfinite(config->lock_vq_ratio) && config->lock_vq_ratio > 0.0f &&
           config->lock_vq_ratio <= 1.0f &&
           config->lock_samples > 0u;
}

void pll_reset(pll_state_t *state, const pll_config_t *config) {
    if (state == NULL) {
        return;
    }
    state->alpha = 0.0f;
    state->beta = 0.0f;
    state->theta_rad = 0.0f;
    state->frequency_hz = (config != NULL && isfinite(config->nominal_hz)) ? config->nominal_hz : 0.0f;
    state->integrator = 0.0f;
    state->vd = 0.0f;
    state->vq = 0.0f;
    state->aligned_samples = 0u;
    state->locked = false;
}

bool pll_step(pll_state_t *state, const pll_config_t *config, float voltage, float dt_s) {
    float omega;
    float error;
    float alpha_next;
    float beta_next;
    float cosine;
    float sine;
    float amplitude;
    float correction;
    float frequency_hz;
    bool aligned;

    if (state == NULL || !pll_config_valid(config) || !isfinite(voltage) ||
        !isfinite(dt_s) || dt_s <= 0.0f) {
        return false;
    }

    omega = PLL_TWO_PI * state->frequency_hz;
    error = voltage - state->alpha;
    alpha_next = state->alpha + dt_s * omega * (config->sogi_k * error - state->beta);
    beta_next = state->beta + dt_s * omega * alpha_next;
    if (!isfinite(alpha_next) || !isfinite(beta_next)) {
        pll_reset(state, config);
        return false;
    }
    state->alpha = alpha_next;
    state->beta = beta_next;

    cosine = cosf(state->theta_rad);
    sine = sinf(state->theta_rad);
    state->vd = state->alpha * cosine + state->beta * sine;
    state->vq = -state->alpha * sine + state->beta * cosine;

    state->integrator = converter_clampf(
        state->integrator + config->ki * state->vq * dt_s,
        -PLL_TWO_PI * config->frequency_limit_hz,
        PLL_TWO_PI * config->frequency_limit_hz);
    correction = config->kp * state->vq + state->integrator;
    frequency_hz = converter_clampf(config->nominal_hz + correction / PLL_TWO_PI,
                                    config->nominal_hz - config->frequency_limit_hz,
                                    config->nominal_hz + config->frequency_limit_hz);
    state->frequency_hz = frequency_hz;
    state->theta_rad = fmodf(state->theta_rad + PLL_TWO_PI * frequency_hz * dt_s, PLL_TWO_PI);
    if (state->theta_rad < 0.0f) {
        state->theta_rad += PLL_TWO_PI;
    }

    amplitude = sqrtf(state->vd * state->vd + state->vq * state->vq);
    aligned = amplitude > 0.5f && fabsf(state->vq) <= config->lock_vq_ratio * amplitude;
    if (aligned && state->aligned_samples < UINT32_MAX) {
        state->aligned_samples += 1u;
    } else if (!aligned) {
        state->aligned_samples = 0u;
    }
    state->locked = state->aligned_samples >= config->lock_samples;
    return true;
}
