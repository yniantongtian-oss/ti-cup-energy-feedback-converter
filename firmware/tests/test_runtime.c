#include "converter_runtime.h"

#include "test_harness.h"

#include <math.h>

static converter_raw_sample_t safe_raw(void) {
    converter_raw_sample_t raw;
    raw.current_adc = 2048u;
    raw.bus_adc = 1200u;
    raw.temperature_adc = 750u;
    raw.sample_valid = true;
    raw.hardware_fault = false;
    return raw;
}

static converter_raw_sample_t zero_current_raw(void) {
    converter_raw_sample_t raw = safe_raw();
    raw.current_adc = 0u;
    return raw;
}

static void test_default_calibration(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    CHECK(runtime.measurement.sample_valid);
    CHECK_NEAR(runtime.measurement.input_current_a, 0.0f, 1e-6f);
    CHECK_NEAR(runtime.measurement.bus_voltage_v, 12.0f, 1e-6f);
    CHECK_NEAR(runtime.measurement.temperature_c, 25.0f, 1e-6f);
}

static void test_filter_smooths_a_sub_trip_step(void) {
    converter_runtime_t runtime;
    converter_runtime_config_t config = converter_runtime_default_config();
    converter_raw_sample_t raw;
    config.current_gain_a_per_count = 0.01f;
    config.current_offset_a = 0.0f;
    config.filter_alpha = 0.5f;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_arm(&runtime, 0u);
    raw = zero_current_raw();
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    raw.current_adc = 100u;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
    CHECK_NEAR(runtime.measurement.input_current_a, 0.5f, 1e-6f);
}

static void test_unfiltered_overcurrent_still_trips(void) {
    converter_runtime_t runtime;
    converter_runtime_config_t config = converter_runtime_default_config();
    converter_raw_sample_t raw;
    config.current_gain_a_per_count = 0.01f;
    config.current_offset_a = 0.0f;
    config.filter_alpha = 0.2f;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_arm(&runtime, 0u);
    raw = zero_current_raw();
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
    raw.current_adc = 1000u;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_FAULT);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_OVERCURRENT) != 0u);
    CHECK(runtime.controller.duty_command == 0.0f);
    CHECK_NEAR(runtime.measurement.input_current_a, 10.0f, 1e-5f);
}

static void test_unfiltered_overvoltage_still_trips(void) {
    converter_runtime_t runtime;
    converter_runtime_config_t config = converter_runtime_default_config();
    converter_raw_sample_t raw;
    config.filter_alpha = 0.2f;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_arm(&runtime, 0u);
    raw = safe_raw();
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    raw.bus_adc = 4000u;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_OVERVOLTAGE) != 0u);
    CHECK_NEAR(runtime.measurement.bus_voltage_v, 40.0f, 1e-4f);
}

static void test_command_timeout_latches_when_armed(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_config_t config = converter_runtime_default_config();
    config.command_timeout_ms = 10u;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_set_reference(&runtime, 1.0f, 0u);
    converter_runtime_arm(&runtime, 0u);
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
    converter_runtime_step(&runtime, &raw, 9u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
    converter_runtime_step(&runtime, &raw, 10u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_FAULT);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_COMM_TIMEOUT) != 0u);
    CHECK(!runtime.controller.armed);
    CHECK(runtime.controller.duty_command == 0.0f);
    CHECK(runtime.controller.requested_current_a == 0.0f);
}

static void test_idle_timeout_does_not_latch(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_config_t config = converter_runtime_default_config();
    config.command_timeout_ms = 10u;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_set_reference(&runtime, 1.0f, 0u);
    converter_runtime_step(&runtime, &raw, 10u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_IDLE);
    CHECK(runtime.controller.faults == CONVERTER_FAULT_NONE);
    CHECK(runtime.controller.requested_current_a == 0.0f);
}

static void test_new_command_refreshes_the_deadline(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_config_t config = converter_runtime_default_config();
    config.command_timeout_ms = 10u;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_arm(&runtime, 0u);
    converter_runtime_set_reference(&runtime, 0.5f, 0u);
    converter_runtime_step(&runtime, &raw, 9u, 0.001f);
    converter_runtime_set_reference(&runtime, 0.5f, 9u);
    converter_runtime_step(&runtime, &raw, 18u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
    converter_runtime_step(&runtime, &raw, 19u, 0.001f);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_COMM_TIMEOUT) != 0u);
}

static void test_timeout_survives_millisecond_wrap(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_config_t config = converter_runtime_default_config();
    config.command_timeout_ms = 20u;
    converter_runtime_init(&runtime, NULL, &config);
    converter_runtime_arm(&runtime, 0xfffffff0u);
    converter_runtime_step(&runtime, &raw, 0xfffffff8u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
    converter_runtime_step(&runtime, &raw, 0x10u, 0.001f);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_COMM_TIMEOUT) != 0u);
}

static void test_invalid_adc_latches_fault(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    raw.bus_adc = 5000u;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_FAULT);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    CHECK(!runtime.measurement.sample_valid);
    CHECK(runtime.controller.duty_command == 0.0f);
}

static void test_full_scale_adc_is_valid(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    raw.current_adc = 4095u;
    converter_runtime_step(&runtime, &raw, 0u, 0.001f);
    CHECK(runtime.measurement.sample_valid);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_INVALID_SAMPLE) == 0u);
}

static void test_hardware_fault_is_distinct(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    raw.hardware_fault = true;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_HARDWARE) != 0u);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_INVALID_SAMPLE) == 0u);
    CHECK(!runtime.controller.armed);
    CHECK(runtime.controller.duty_command == 0.0f);
    CHECK(!converter_runtime_clear_faults(&runtime, &raw));
    raw.hardware_fault = false;
    CHECK(converter_runtime_clear_faults(&runtime, &raw));
    CHECK(runtime.controller.faults == CONVERTER_FAULT_NONE);
}

static void test_emergency_stop_requires_local_acknowledgement(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    converter_runtime_emergency_stop(&runtime);
    CHECK(runtime.controller.state == CONVERTER_STATE_FAULT);
    CHECK((runtime.controller.faults & CONVERTER_FAULT_ESTOP) != 0u);
    CHECK(runtime.controller.duty_command == 0.0f);
    CHECK(!converter_runtime_clear_faults(&runtime, &raw));
    CHECK((runtime.controller.faults & CONVERTER_FAULT_ESTOP) != 0u);
    CHECK(converter_runtime_acknowledge_estop(&runtime));
    CHECK(runtime.controller.faults == CONVERTER_FAULT_NONE);
    CHECK(runtime.controller.state == CONVERTER_STATE_IDLE);
    CHECK(!converter_runtime_acknowledge_estop(&runtime));
    converter_runtime_arm(&runtime, 2u);
    converter_runtime_step(&runtime, &raw, 2u, 0.001f);
    CHECK(runtime.controller.state == CONVERTER_STATE_RUN);
}

static void test_clear_fault_after_safe_sample(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_arm(&runtime, 0u);
    raw.sample_valid = false;
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    raw = safe_raw();
    CHECK(converter_runtime_clear_faults(&runtime, &raw));
    CHECK(runtime.controller.faults == CONVERTER_FAULT_NONE);
    CHECK(runtime.controller.state == CONVERTER_STATE_IDLE);
    CHECK(runtime.measurement.sample_valid);
}

static void test_invalid_runtime_config_disarms(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_set_reference(&runtime, 1.0f, 0u);
    converter_runtime_arm(&runtime, 0u);
    converter_runtime_step(&runtime, &raw, 1u, 0.001f);
    CHECK(runtime.controller.duty_command != 0.0f);
    runtime.runtime_config.filter_alpha = 0.0f;
    converter_runtime_step(&runtime, &raw, 2u, 0.001f);
    CHECK(!runtime.controller.armed);
    CHECK(runtime.controller.duty_command == 0.0f);
    CHECK(runtime.controller.state == CONVERTER_STATE_IDLE);
}

static void test_null_and_config_guards(void) {
    converter_runtime_t runtime;
    converter_raw_sample_t raw = safe_raw();
    converter_runtime_config_t config = converter_runtime_default_config();
    converter_runtime_init(NULL, NULL, NULL);
    converter_runtime_set_reference(NULL, 1.0f, 0u);
    converter_runtime_arm(NULL, 0u);
    converter_runtime_disarm(NULL);
    converter_runtime_emergency_stop(NULL);
    converter_runtime_step(NULL, &raw, 0u, 0.001f);
    CHECK(!converter_runtime_clear_faults(NULL, &raw));
    CHECK(!converter_runtime_acknowledge_estop(NULL));
    CHECK(!converter_runtime_config_is_valid(NULL));
    CHECK(converter_runtime_config_is_valid(&config));
    config.command_timeout_ms = 0u;
    CHECK(!converter_runtime_config_is_valid(&config));
    config = converter_runtime_default_config();
    config.bus_gain_v_per_count = 0.0f;
    CHECK(!converter_runtime_config_is_valid(&config));
    converter_runtime_init(&runtime, NULL, NULL);
    converter_runtime_set_reference(&runtime, (float)NAN, 0u);
    CHECK(!runtime.command_received);
    CHECK(!converter_runtime_clear_faults(&runtime, NULL));
}

int main(void) {
    test_default_calibration();
    test_filter_smooths_a_sub_trip_step();
    test_unfiltered_overcurrent_still_trips();
    test_unfiltered_overvoltage_still_trips();
    test_command_timeout_latches_when_armed();
    test_idle_timeout_does_not_latch();
    test_new_command_refreshes_the_deadline();
    test_timeout_survives_millisecond_wrap();
    test_invalid_adc_latches_fault();
    test_full_scale_adc_is_valid();
    test_hardware_fault_is_distinct();
    test_emergency_stop_requires_local_acknowledgement();
    test_clear_fault_after_safe_sample();
    test_invalid_runtime_config_disarms();
    test_null_and_config_guards();
    if (g_test_failures != 0) {
        fprintf(stderr, "%d runtime checks failed\n", g_test_failures);
        return 1;
    }
    puts("all runtime tests passed");
    return 0;
}
