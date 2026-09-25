#include "tim1_bkin.h"

#include <assert.h>
#include <stdio.h>

static void test_bdtr_contract(void) {
    uint32_t bits = tim1_bkin_bdtr_bits();
    /* BKE set, BKP clear, AOE clear */
    assert((bits & (1u << 12)) != 0u); /* BKE */
    assert((bits & (1u << 13)) == 0u); /* BKP */
    assert((bits & (1u << 14)) == 0u); /* AOE */
}

static void test_break_clears_moe_before_clear(void) {
    tim1_bkin_t b;
    tim1_bkin_init(&b);
    b.moe = true;
    tim1_bkin_on_hardware_break(&b);
    assert(!b.moe);
    assert(b.latched_trip);
    assert(!tim1_bkin_outputs_allowed(&b));
    /* Cannot clear without PLL + safe */
    assert(!tim1_bkin_try_clear(&b, false, true));
    assert(b.latched_trip);
    assert(tim1_bkin_try_clear(&b, true, true));
    assert(!b.latched_trip);
    /* MOE still false until platform explicitly re-arms (AOE=0) */
    assert(!b.moe);
    assert(!tim1_bkin_outputs_allowed(&b));
    b.moe = true;
    assert(tim1_bkin_outputs_allowed(&b));
}

static void test_defaults(void) {
    tim1_bkin_t b;
    tim1_bkin_init(&b);
    assert(b.bkin_active_low);
    assert(!b.aoe_enable);
    assert(b.break_enable);
}

int main(void) {
    test_bdtr_contract();
    test_break_clears_moe_before_clear();
    test_defaults();
    puts("all tim1_bkin tests passed");
    return 0;
}
