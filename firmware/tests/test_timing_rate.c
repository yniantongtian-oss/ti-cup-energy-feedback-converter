#include "converter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

/* Step B: converter_step must accept ~20 kHz dt and keep duty formula. */
static void test_20khz_step_runs(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t m = {
        .input_current_a = 0.0f,
        .bus_voltage_v = 24.0f,
        .plant_voltage_v = 0.0f,
        .temperature_c = 25.0f,
        .sample_valid = true,
    };
    const float dt = 1.0f / 20000.0f;
    config.current_slew_a_per_s = 50.0f; /* demo: reach 1 A in 20 ms */
    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_arm(&converter);
    for (int i = 0; i < 8000; ++i) { /* 0.4 s @ 20 kHz */
        m.plant_voltage_v = 5.0f * sinf(2.0f * 3.1415926f * 100.0f * (float)i * dt);
        converter_step(&converter, &config, &m, dt);
        assert(converter.state == CONVERTER_STATE_RUN);
        assert(fabsf(converter.duty_command) <= config.duty_limit + 1e-6f);
        m.input_current_a +=
            (converter.duty_command * m.bus_voltage_v - m.plant_voltage_v -
             0.5f * m.input_current_a) *
            dt / 0.002f;
    }
    assert(fabsf(m.input_current_a - 1.0f) < 0.25f);
}

static void test_reject_bad_timestep_still(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t m = {
        .input_current_a = 0.0f,
        .bus_voltage_v = 12.0f,
        .plant_voltage_v = 0.0f,
        .temperature_c = 25.0f,
        .sample_valid = true,
    };
    converter_init(&converter);
    converter_arm(&converter);
    converter_step(&converter, &config, &m, 0.0f);
    assert((converter.faults & CONVERTER_FAULT_BAD_TIMESTEP) != 0u);
}

int main(void) {
    test_20khz_step_runs();
    test_reject_bad_timestep_still();
    puts("all timing rate tests passed");
    return 0;
}
