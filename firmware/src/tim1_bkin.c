#include "tim1_bkin.h"

#include <stddef.h>

/* STM32F1 TIM1 BDTR bit positions (RM0008) */
#define TIM_BDTR_AOE_Pos 14u
#define TIM_BDTR_BKP_Pos 13u
#define TIM_BDTR_BKE_Pos 12u
#define TIM_BDTR_MOE_Pos 15u

void tim1_bkin_init(tim1_bkin_t *b) {
    if (b == NULL) return;
    b->bkin_active_low = true;  /* BKP=0 */
    b->aoe_enable = false;      /* AOE=0: do not auto-reenable after break */
    b->break_enable = true;
    b->latched_trip = false;
    b->moe = false;             /* start with outputs off */
}

void tim1_bkin_on_hardware_break(tim1_bkin_t *b) {
    if (b == NULL) return;
    /* Hardware already cleared MOE; mirror that before any ISR bookkeeping. */
    b->moe = false;
    b->latched_trip = true;
}

bool tim1_bkin_try_clear(tim1_bkin_t *b, bool pll_locked, bool hardware_inputs_safe) {
    if (b == NULL) return false;
    if (!b->latched_trip) return false;
    /* Software clear is only allowed after safe inputs + PLL re-lock.
       It does not replace the external comparator / nFAULT → PA6 path. */
    if (!hardware_inputs_safe || !pll_locked) return false;
    b->latched_trip = false;
    /* MOE must be re-set explicitly by the platform after clear; AOE stays 0. */
    return true;
}

bool tim1_bkin_outputs_allowed(const tim1_bkin_t *b) {
    return b != NULL && b->moe && !b->latched_trip;
}

uint32_t tim1_bkin_bdtr_bits(void) {
    /* BKE=1, BKP=0, AOE=0; MOE left to runtime */
    return (1u << TIM_BDTR_BKE_Pos);
}
