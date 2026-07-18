#include "converter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static converter_measurement_t safe_measurement(void) {
    converter_measurement_t measurement = {0.0f, 12.0f, 25.0f, true};
    return measurement;
}

static void test_safe_startup(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    assert(converter.state == CONVERTER_STATE_IDLE);
    assert(converter.duty_command == 0.0f);
}

static void test_arm_and_slew_limit(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_set_current_reference(&converter, 100.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.01f);
    assert(converter.state == CONVERTER_STATE_RUN);
    assert(fabsf(converter.ramped_current_a - 0.05f) < 1e-6f);
    assert(fabsf(converter.duty_command) <= config.duty_limit);
}

static void test_fault_latches_and_forces_zero(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_step(&converter, &config, &measurement, 0.001f);
    measurement.input_current_a = config.current_trip_a + 0.1f;
    converter_step(&converter, &config, &measurement, 0.001f);
    assert(converter.state == CONVERTER_STATE_FAULT);
    assert((converter.faults & CONVERTER_FAULT_OVERCURRENT) != 0u);
    assert(converter.duty_command == 0.0f);
    assert(!converter.armed);
}

static void test_fault_clear_requires_safe_disarmed_state(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.temperature_c = config.temperature_trip_c + 1.0f;
    converter_step(&converter, &config, &measurement, 0.001f);
    assert(!converter_clear_faults(&converter, &config, &measurement));
    measurement = safe_measurement();
    assert(converter_clear_faults(&converter, &config, &measurement));
    assert(converter.state == CONVERTER_STATE_IDLE);
    assert(converter.faults == CONVERTER_FAULT_NONE);
}

static void test_invalid_sample_fault(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.sample_valid = false;
    converter_step(&converter, &config, &measurement, 0.001f);
    assert((converter.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    assert(converter.duty_command == 0.0f);
}

int main(void) {
    test_safe_startup();
    test_arm_and_slew_limit();
    test_fault_latches_and_forces_zero();
    test_fault_clear_requires_safe_disarmed_state();
    test_invalid_sample_fault();
    puts("all converter tests passed");
    return 0;
}
