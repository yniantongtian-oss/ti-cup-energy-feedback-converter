#ifndef TIM1_BKIN_H
#define TIM1_BKIN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Step F — F103 TIM1 break contract (no C2000 CMPSS):
 *   PA6 = TIM1_BKIN, active-low (BKP=0), AOE=0 (no auto re-arm).
 *   Hardware break clears MOE before any software ISR.
 *   PB12 GPIO fault is NOT a substitute for BKIN.
 *   Software clear of a latched trip flag does not replace the comparator/nFAULT path.
 */

typedef struct {
    bool bkin_active_low;       /* true => BKP=0 */
    bool aoe_enable;            /* must be false for energy-feedback */
    bool break_enable;          /* BKE */
    bool latched_trip;          /* software mirror after hardware trip */
    bool moe;                   /* main output enable mirror */
} tim1_bkin_t;

void tim1_bkin_init(tim1_bkin_t *b);
void tim1_bkin_on_hardware_break(tim1_bkin_t *b);   /* MOE cleared first */
bool tim1_bkin_try_clear(tim1_bkin_t *b, bool pll_locked, bool hardware_inputs_safe);
bool tim1_bkin_outputs_allowed(const tim1_bkin_t *b);

/* Register programming helpers for docs/tests (bitfield values, not ST headers). */
uint32_t tim1_bkin_bdtr_bits(void); /* BKE=1, BKP=0, AOE=0 */

#ifdef __cplusplus
}
#endif

#endif
