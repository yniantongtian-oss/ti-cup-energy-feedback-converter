#include "sogi_pll.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float wrap_0_2pi(float th) {
    const float twopi = (float)(2.0 * M_PI);
    while (th >= twopi) th -= twopi;
    while (th < 0.0f) th += twopi;
    return th;
}

sogi_pll_config_t sogi_pll_default_config(void) {
    sogi_pll_config_t c;
    c.k_sogi = 1.41421356f;
    c.kp = 40.0f;
    c.ki = 800.0f;
    c.f0_hz = 100.0f; /* energy-feedback plant often 100 Hz; demo default */
    c.fmin_hz = 80.0f;
    c.fmax_hz = 120.0f;
    c.lock_err_max = 0.15f;
    c.unlock_err_max = 0.35f;
    c.lock_hold_samples = 200u;   /* 10 ms @ 20 kHz */
    c.unlock_hold_samples = 100u;
    c.umin_v = 1.0f;
    return c;
}

bool sogi_pll_config_is_valid(const sogi_pll_config_t *config) {
    if (config == NULL) return false;
    return isfinite(config->k_sogi) && config->k_sogi > 0.0f &&
           isfinite(config->kp) && config->kp >= 0.0f &&
           isfinite(config->ki) && config->ki >= 0.0f &&
           isfinite(config->f0_hz) && config->f0_hz > 0.0f &&
           isfinite(config->fmin_hz) && config->fmin_hz > 0.0f &&
           isfinite(config->fmax_hz) && config->fmax_hz > config->fmin_hz &&
           config->f0_hz >= config->fmin_hz && config->f0_hz <= config->fmax_hz &&
           isfinite(config->lock_err_max) && config->lock_err_max > 0.0f &&
           isfinite(config->unlock_err_max) &&
           config->unlock_err_max >= config->lock_err_max &&
           config->lock_hold_samples > 0u && config->unlock_hold_samples > 0u &&
           isfinite(config->umin_v) && config->umin_v >= 0.0f;
}

void sogi_pll_init(sogi_pll_t *pll, const sogi_pll_config_t *config) {
    if (pll == NULL) return;
    sogi_pll_reset(pll, config);
}

void sogi_pll_reset(sogi_pll_t *pll, const sogi_pll_config_t *config) {
    if (pll == NULL) return;
    pll->v_alpha = 0.0f;
    pll->v_beta = 0.0f;
    pll->integrator = 0.0f;
    pll->omega_rad_s = (config != NULL && sogi_pll_config_is_valid(config))
                           ? (float)(2.0 * M_PI) * config->f0_hz
                           : (float)(2.0 * M_PI) * 100.0f;
    pll->theta_rad = 0.0f;
    pll->amplitude_v = 0.0f;
    pll->phase_error = 0.0f;
    pll->locked = false;
    pll->lock_count = 0u;
    pll->unlock_count = 0u;
}

void sogi_pll_step(sogi_pll_t *pll,
                   const sogi_pll_config_t *config,
                   float u1_v,
                   float dt_s) {
    float omega, k, d_alpha, d_beta, v_d, v_q, wmin, wmax, amp;

    if (pll == NULL || !sogi_pll_config_is_valid(config)) return;
    if (!isfinite(u1_v) || !isfinite(dt_s) || dt_s <= 0.0f || dt_s > 0.01f) {
        pll->locked = false;
        pll->lock_count = 0u;
        return;
    }

    omega = pll->omega_rad_s;
    k = config->k_sogi;

    /* SOGI (continuous → forward Euler) */
    d_alpha = k * (u1_v - pll->v_alpha) * omega - pll->v_beta * omega;
    d_beta = pll->v_alpha * omega;
    pll->v_alpha += d_alpha * dt_s;
    pll->v_beta += d_beta * dt_s;

    /* Park: rotate alpha/beta by estimated theta → d/q */
    {
        float c = cosf(pll->theta_rad);
        float s = sinf(pll->theta_rad);
        v_d = c * pll->v_alpha + s * pll->v_beta;
        v_q = -s * pll->v_alpha + c * pll->v_beta;
    }
    pll->phase_error = v_q;
    amp = sqrtf(pll->v_alpha * pll->v_alpha + pll->v_beta * pll->v_beta);
    pll->amplitude_v = amp;

    /* PI on q → omega correction; center at nominal */
    pll->integrator = clampf(pll->integrator + config->ki * v_q * dt_s, -5000.0f, 5000.0f);
    omega = (float)(2.0 * M_PI) * config->f0_hz + config->kp * v_q + pll->integrator;
    wmin = (float)(2.0 * M_PI) * config->fmin_hz;
    wmax = (float)(2.0 * M_PI) * config->fmax_hz;
    pll->omega_rad_s = clampf(omega, wmin, wmax);
    pll->theta_rad = wrap_0_2pi(pll->theta_rad + pll->omega_rad_s * dt_s);

    /* Lock detect: small |q|, adequate amplitude, freq in band */
    {
        float abs_err = fabsf(v_q);
        float abs_err_n = (amp > 1e-3f) ? (abs_err / amp) : 1.0f;
        bool candidate = (amp >= config->umin_v) && (abs_err_n <= config->lock_err_max);
        bool bad = (amp < config->umin_v) || (abs_err_n >= config->unlock_err_max);

        if (pll->locked) {
            if (bad) {
                pll->unlock_count++;
                if (pll->unlock_count >= config->unlock_hold_samples) {
                    pll->locked = false;
                    pll->lock_count = 0u;
                    pll->unlock_count = 0u;
                }
            } else {
                pll->unlock_count = 0u;
            }
        } else {
            if (candidate) {
                pll->lock_count++;
                if (pll->lock_count >= config->lock_hold_samples) {
                    pll->locked = true;
                    pll->unlock_count = 0u;
                }
            } else {
                pll->lock_count = 0u;
            }
        }
    }

    (void)v_d;
}
