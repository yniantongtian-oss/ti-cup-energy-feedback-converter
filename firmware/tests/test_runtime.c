#include "converter_runtime.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static converter_raw_sample_t safe_raw(void) {
    converter_raw_sample_t raw;
    raw.current_adc = 2048u;
    raw.bus_adc = 1200u;
    raw.temperature_adc = 750u;
    raw.sample_valid = true;
    raw.hardware_fault = false;
    return raw;
}

static void test_default_calibration(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    assert(runtime.measurement.sample_valid);
    assert(fabsf(runtime.measurement.input_current_a) < 1e-6f);
    assert(fabsf(runtime.measurement.bus_voltage_v - 12.0f) < 1e-6f);
    assert(fabsf(runtime.measurement.temperature_c - 25.0f) < 1e-6f);
}

static void test_command_timeout_disarms(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_config_t config = converter_runtime_default_config();
    config.command_timeout_ms = 10u;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_set_reference(&runtime, 1.0f, 0u);
    converter_runtime_arm(&runtime, 0u);
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    assert(runtime.controller.state == CONVERTER_STATE_RUN);
    converter_runtime_step(&runtime, &raw, 11u, 0.001f);
    assert(runtime.controller.state == CONVERTER_STATE_IDLE);
    assert(!runtime.controller.armed);
    assert(runtime.controller.duty_command == 0.0f);
}

static void test_invalid_adc_latches_fault(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    raw.bus_adc = 5000u;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    assert(runtime.controller.state == CONVERTER_STATE_FAULT);
    assert((runtime.controller.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    assert(runtime.controller.duty_command == 0.0f);
}

static void test_hardware_fault_latches_fault(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    raw.hardware_fault = true;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    assert((runtime.controller.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    assert(!runtime.controller.armed);
}

static void test_clear_fault_after_safe_sample(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    raw.sample_valid = false;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    raw = safe_raw();
    assert(converter_runtime_clear_faults(&runtime, &raw));
    assert(runtime.controller.faults == CONVERTER_FAULT_NONE);
    assert(runtime.controller.state == CONVERTER_STATE_IDLE);
}

int main(void) {
    test_default_calibration();
    test_command_timeout_disarms();
    test_invalid_adc_latches_fault();
    test_hardware_fault_latches_fault();
    test_clear_fault_after_safe_sample();
    puts("all runtime tests passed");
    return 0;
}
