#include "bringup.h"
#include "converter_runtime.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void test_mode_sequence(void) {
    bringup_t b;
    bringup_config_t cfg = bringup_default_config();
    cfg.open_loop_dwell_ms = 50u;
    cfg.current_dc_dwell_ms = 50u;
    cfg.bus_ramp_v_per_s = 100.0f;
    bringup_init(&b, &cfg);
    bringup_request_start(&b);

    /* IDLE → OPEN_LOOP */
    bringup_step_1khz(&b, 12.0f, 0.0f, false, true, 1u);
    assert(b.mode == BRINGUP_OPEN_LOOP);
    assert(b.force_open_loop);
    assert(!b.current_enable);

    /* stay until lock + dwell */
    for (uint32_t i = 0; i < 60u; ++i) {
        bringup_step_1khz(&b, 12.0f, 1.0f, true, true, 1u);
    }
    assert(b.mode == BRINGUP_CURRENT_DC);
    assert(b.current_enable);
    assert(!b.outer_enable);
    assert(fabsf(b.id_ref_a - cfg.id_dc_a) < 1e-6f);

    for (uint32_t i = 0; i < 60u; ++i) {
        bringup_step_1khz(&b, 20.0f, 4.0f, true, true, 1u);
    }
    assert(b.mode == BRINGUP_OUTER_V);
    assert(b.outer_enable);
    assert(b.current_enable);
}

static void test_bus_slow_ramp(void) {
    bringup_t b;
    bringup_config_t cfg = bringup_default_config();
    cfg.bus_start_v = 12.0f;
    cfg.bus_target_v = 24.0f;
    cfg.bus_ramp_v_per_s = 4.0f;
    cfg.open_loop_dwell_ms = 10u;
    bringup_init(&b, &cfg);
    bringup_request_start(&b);
    bringup_step_1khz(&b, 12.0f, 0.0f, true, true, 1u);
    assert(fabsf(b.bus_cmd_v - 12.0f) < 0.05f);
    for (int i = 0; i < 1000; ++i) { /* 1 s → +4 V */
        bringup_step_1khz(&b, 12.0f, 8.0f, true, true, 1u);
    }
    assert(b.bus_cmd_v > 15.5f && b.bus_cmd_v < 16.5f);
    for (int i = 0; i < 3000; ++i) {
        bringup_step_1khz(&b, 20.0f, 8.0f, true, true, 1u);
    }
    assert(fabsf(b.bus_cmd_v - 24.0f) < 0.05f);
}

static void test_runtime_bringup_advances(void) {
    converter_runtime_t rt;
    converter_runtime_config_t rc = converter_runtime_default_config();
    converter_raw_sample_t raw;
    const float dt = 1.0f / 20000.0f;
    bringup_mode_t seen_ol = BRINGUP_IDLE, seen_dc = BRINGUP_IDLE, seen_ov = BRINGUP_IDLE;

    rc.bringup_enable = true;
    converter_runtime_init(&rt, NULL, &rc);
    rt.bringup.config.open_loop_dwell_ms = 20u;
    rt.bringup.config.current_dc_dwell_ms = 20u;
    converter_runtime_arm(&rt, 0u);

    raw.current_adc = 2048u;
    raw.bus_adc = 1200u;
    raw.temperature_adc = 750u;
    raw.sample_valid = true;
    raw.hardware_fault = false;

    for (int i = 0; i < 8000; ++i) {
        float t = (float)i * dt;
        float u1 = 8.0f * sinf(2.0f * (float)M_PI * 100.0f * t);
        float counts = (u1 + 20.48f) / 0.01f;
        if (counts < 0.0f) counts = 0.0f;
        if (counts > 4095.0f) counts = 4095.0f;
        raw.plant_adc = (uint16_t)counts;
        converter_runtime_step(&rt, &raw, (uint32_t)(i / 20), dt);
        if (i % 20 == 0) {
            converter_runtime_bringup_1khz(&rt, 1u);
        }
        seen_ol = (rt.bringup.mode == BRINGUP_OPEN_LOOP) ? BRINGUP_OPEN_LOOP : seen_ol;
        if (rt.bringup.mode == BRINGUP_CURRENT_DC) seen_dc = BRINGUP_CURRENT_DC;
        if (rt.bringup.mode == BRINGUP_OUTER_V) seen_ov = BRINGUP_OUTER_V;
    }
    assert(seen_ol == BRINGUP_OPEN_LOOP);
    assert(seen_dc == BRINGUP_CURRENT_DC);
    assert(seen_ov == BRINGUP_OUTER_V);
}

int main(void) {
    test_mode_sequence();
    test_bus_slow_ramp();
    test_runtime_bringup_advances();
    puts("all bringup tests passed");
    return 0;
}
