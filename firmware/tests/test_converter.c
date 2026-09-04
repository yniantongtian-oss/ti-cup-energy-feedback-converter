#include "converter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static converter_measurement_t safe_measurement(void) {
    converter_measurement_t measurement = {
        .input_current_a = 0.0f,
        .bus_voltage_v = 12.0f,
        .plant_voltage_v = 0.0f,
        .current_beta_a = 0.0f,
        .theta_rad = 0.0f,
        .theta_valid = false,
        .temperature_c = 25.0f,
        .sample_valid = true,
    };
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
    assert(config.feedforward_enable);
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

static void test_feedforward_duty_formula(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    config.kp = 1.0f;
    config.ki = 0.0f;
    config.current_slew_a_per_s = 1000.0f;
    config.voltage_command_limit = 50.0f;
    config.duty_limit = 1.0f;
    config.feedforward_enable = true;
    measurement.bus_voltage_v = 10.0f;
    measurement.plant_voltage_v = 5.0f;

    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);

    assert(converter.feedforward_active);
    assert(fabsf(converter.voltage_command - 1.0f) < 1e-5f);
    assert(fabsf(converter.duty_command - 0.6f) < 1e-5f);

    config.feedforward_enable = false;
    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    assert(!converter.feedforward_active);
    assert(fabsf(converter.voltage_command - 1.0f) < 1e-5f);
    assert(fabsf(converter.duty_command - 0.1f) < 1e-5f);
}

static void test_low_bus_forces_zero_duty(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    config.bus_voltage_min_v = 0.0f;
    config.mod_bus_min_v = 1.0f;
    config.kp = 1.0f;
    config.ki = 0.0f;
    config.current_slew_a_per_s = 1000.0f;
    measurement.bus_voltage_v = 0.5f;
    measurement.plant_voltage_v = 5.0f;

    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    assert(converter.state == CONVERTER_STATE_RUN);
    assert(converter.duty_command == 0.0f);
}

int main(void) {
    test_safe_startup();
    test_arm_and_slew_limit();
    test_fault_latches_and_forces_zero();
    test_fault_clear_requires_safe_disarmed_state();
    test_invalid_sample_fault();
    test_feedforward_duty_formula();
    test_low_bus_forces_zero_duty();
    puts("all converter tests passed");
    return 0;
}
