#include "converter_runtime.h"
#include "sogi_pll.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void test_locks_on_sine_u1(void) {
    sogi_pll_t pll;
    sogi_pll_config_t cfg = sogi_pll_default_config();
    const float dt = 1.0f / 20000.0f;
    const float f = 100.0f;
    const float amp = 8.0f;
    bool saw_lock = false;

    cfg.lock_hold_samples = 100u;
    sogi_pll_init(&pll, &cfg);
    for (int i = 0; i < 8000; ++i) {
        float t = (float)i * dt;
        float u1 = amp * sinf(2.0f * (float)M_PI * f * t);
        sogi_pll_step(&pll, &cfg, u1, dt);
        if (pll.locked) {
            saw_lock = true;
            break;
        }
    }
    assert(saw_lock);
    assert(fabsf(pll.omega_rad_s / (2.0f * (float)M_PI) - f) < 5.0f);
}

static void test_no_lock_on_zero_u1(void) {
    sogi_pll_t pll;
    sogi_pll_config_t cfg = sogi_pll_default_config();
    const float dt = 1.0f / 20000.0f;
    sogi_pll_init(&pll, &cfg);
    for (int i = 0; i < 4000; ++i) {
        sogi_pll_step(&pll, &cfg, 0.0f, dt);
    }
    assert(!pll.locked);
}

static void test_runtime_gates_pwm_until_lock(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw;
    const float dt = 1.0f / 20000.0f;
    bool locked_seen = false;
    float duty_while_unlocked = 0.0f;
    float duty_while_locked = 0.0f;

    converter_runtime_init(&runtime, NULL, NULL);
    runtime.pll_config.lock_hold_samples = 80u;
    runtime.pll_config.f0_hz = 100.0f;
    converter_runtime_set_reference(&runtime, 1.0f, 0u);
    converter_runtime_arm(&runtime, 0u);

    raw.current_adc = 2048u;
    raw.bus_adc = 2400u; /* 24 V */
    raw.temperature_adc = 750u;
    raw.sample_valid = true;
    raw.hardware_fault = false;

    for (int i = 0; i < 6000; ++i) {
        float t = (float)i * dt;
        float u1 = 8.0f * sinf(2.0f * (float)M_PI * 100.0f * t);
        /* plant_adc: gain 0.01 offset -20.48 => raw = (u1 + 20.48)/0.01 */
        float plant_counts = (u1 + 20.48f) / 0.01f;
        if (plant_counts < 0.0f) plant_counts = 0.0f;
        if (plant_counts > 4095.0f) plant_counts = 4095.0f;
        raw.plant_adc = (uint16_t)plant_counts;

        converter_runtime_step(&runtime, &raw, (uint32_t)(i / 20), dt);
        if (!runtime.pll.locked) {
            assert(runtime.controller.duty_command == 0.0f);
            assert(!runtime.pwm_released);
            duty_while_unlocked = runtime.controller.duty_command;
        } else {
            locked_seen = true;
            assert(runtime.pwm_released);
            duty_while_locked = runtime.controller.duty_command;
            if (i > 3000) break;
        }
    }
    assert(locked_seen);
    assert(duty_while_unlocked == 0.0f);
    (void)duty_while_locked;
}

static void test_gate_disable_allows_duty_without_lock(void) {
    converter_runtime_t runtime;
    converter_runtime_config_t rcfg = converter_runtime_default_config();
    converter_raw_sample_t raw;
    rcfg.pll_gate_pwm = false;
    converter_runtime_init(&runtime, NULL, &rcfg);
    converter_runtime_set_reference(&runtime, 1.0f, 0u);
    converter_runtime_arm(&runtime, 0u);
    raw.current_adc = 2048u;
    raw.bus_adc = 1200u;
    raw.plant_adc = 2048u; /* 0 V U1 — will not lock */
    raw.temperature_adc = 750u;
    raw.sample_valid = true;
    raw.hardware_fault = false;
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    assert(!runtime.pll.locked);
    assert(runtime.pwm_released); /* gate off */
    assert(fabsf(runtime.controller.duty_command) > 0.0f ||
           runtime.controller.state == CONVERTER_STATE_RUN);
}

int main(void) {
    test_locks_on_sine_u1();
    test_no_lock_on_zero_u1();
    test_runtime_gates_pwm_until_lock();
    test_gate_disable_allows_duty_without_lock();
    puts("all sogi_pll tests passed");
    return 0;
}
