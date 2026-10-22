#ifndef PLL_H
#define PLL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Single-phase SOGI-PLL for a future grid-voltage sample.
 * The Blue Pill reference port has no grid-sense channel, so the 1 ms
 * current loop does not call this. Host tests and the Python twin do.
 */
typedef struct {
    float nominal_hz;
    float kp;
    float ki;
    float sogi_k;
    float frequency_limit_hz;
    float lock_vq_ratio;
    uint32_t lock_samples;
} pll_config_t;

typedef struct {
    float alpha;
    float beta;
    float theta_rad;
    float frequency_hz;
    float integrator;
    float vd;
    float vq;
    uint32_t aligned_samples;
    bool locked;
} pll_state_t;

pll_config_t pll_default_config(void);
bool pll_config_valid(const pll_config_t *config);
/* pll_reset sets the oscillator to nominal_hz. Call it before pll_step. */
void pll_reset(pll_state_t *state, const pll_config_t *config);

/* Returns false when the step is rejected. A rejected finite-input failure
 * leaves the state unchanged. A non-finite internal update resets the state.
 */
bool pll_step(pll_state_t *state, const pll_config_t *config, float voltage, float dt_s);

#ifdef __cplusplus
}
#endif

#endif
