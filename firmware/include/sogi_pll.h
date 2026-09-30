#ifndef SOGI_PLL_H
#define SOGI_PLL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Single-phase SOGI-PLL on plant-side U1.
 * Step C: sync source is SOGI-PLL only — no RAMPGEN / open-loop angle ramp.
 */

typedef struct {
    float k_sogi;           /* SOGI gain, typically ~1.414 */
    float kp;               /* PI on q-axis (or phase error) */
    float ki;
    float f0_hz;            /* nominal AC frequency */
    float fmin_hz;
    float fmax_hz;
    float lock_err_max;     /* |q| or phase-error threshold for lock */
    float unlock_err_max;   /* hysteresis unlock */
    uint32_t lock_hold_samples;
    uint32_t unlock_hold_samples;
    float umin_v;           /* reject near-zero U1 amplitude */
} sogi_pll_config_t;

typedef struct {
    float v_alpha;          /* SOGI direct */
    float v_beta;           /* SOGI quadrature */
    float integrator;       /* PI integrator (rad/s) */
    float omega_rad_s;      /* estimated angular freq */
    float theta_rad;        /* 0..2pi */
    float amplitude_v;
    float phase_error;      /* last q (Park) used for PI */
    bool locked;
    uint32_t lock_count;
    uint32_t unlock_count;
} sogi_pll_t;

sogi_pll_config_t sogi_pll_default_config(void);
bool sogi_pll_config_is_valid(const sogi_pll_config_t *config);
void sogi_pll_init(sogi_pll_t *pll, const sogi_pll_config_t *config);
void sogi_pll_reset(sogi_pll_t *pll, const sogi_pll_config_t *config);

/* Feed one U1 sample. dt_s ~ 1/20000. Updates theta/omega/locked. */
void sogi_pll_step(sogi_pll_t *pll,
                   const sogi_pll_config_t *config,
                   float u1_v,
                   float dt_s);

#ifdef __cplusplus
}
#endif

#endif
