#include "converter.h"

#include "test_harness.h"

#include <math.h>

static converter_measurement_t safe_measurement(void) {
    converter_measurement_t measurement;
    measurement.input_current_a = 0.0f;
    measurement.bus_voltage_v = 12.0f;
    measurement.temperature_c = 25.0f;
    measurement.sample_valid = true;
    return measurement;
}

static void test_safe_startup(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(converter.state == CONVERTER_STATE_IDLE);
    CHECK(converter.duty_command == 0.0f);
    CHECK(!converter.armed);
}

static void test_arm_and_slew_limit(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_set_current_reference(&converter, 100.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.01f);
    CHECK(converter.state == CONVERTER_STATE_RUN);
    CHECK_NEAR(converter.ramped_current_a, 0.05f, 1e-6f);
    CHECK(fabsf(converter.duty_command) <= config.duty_limit);
    /* error = 0.05, duty = kp*error + ki*error*dt */
    CHECK_NEAR(converter.duty_command, 0.008f, 1e-6f);
    CHECK_NEAR(converter.integrator, 0.004f, 1e-6f);
}

static void test_negative_slew_and_reference_clamp(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_set_current_reference(&converter, -100.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.01f);
    CHECK_NEAR(converter.ramped_current_a, -0.05f, 1e-6f);
    config.current_slew_a_per_s = 1000.0f;
    converter_step(&converter, &config, &measurement, 0.01f);
    CHECK_NEAR(converter.ramped_current_a, -config.current_limit_a, 1e-5f);
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
    CHECK(converter.state == CONVERTER_STATE_FAULT);
    CHECK((converter.faults & CONVERTER_FAULT_OVERCURRENT) != 0u);
    CHECK(converter.duty_command == 0.0f);
    CHECK(converter.ramped_current_a == 0.0f);
    CHECK(converter.integrator == 0.0f);
    CHECK(!converter.armed);
    measurement = safe_measurement();
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(converter.state == CONVERTER_STATE_FAULT);
    CHECK(!converter.armed);
}

static void test_fault_clear_requires_safe_disarmed_state(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.temperature_c = config.temperature_trip_c + 1.0f;
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(!converter_clear_faults(&converter, &config, &measurement));
    measurement = safe_measurement();
    CHECK(converter_clear_faults(&converter, &config, &measurement));
    CHECK(converter.state == CONVERTER_STATE_IDLE);
    CHECK(converter.faults == CONVERTER_FAULT_NONE);
    CHECK(converter.duty_command == 0.0f);
}

static void test_clear_rejected_while_armed_and_running(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    CHECK(!converter_clear_faults(&converter, &config, &measurement));
    CHECK(converter.armed);
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(converter.state == CONVERTER_STATE_RUN);
    CHECK(!converter_clear_faults(&converter, &config, &measurement));
    CHECK(converter.state == CONVERTER_STATE_RUN);
    CHECK(converter.armed);
}

static void test_invalid_sample_fault(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.sample_valid = false;
    measurement.input_current_a = 100.0f;
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK((converter.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    CHECK((converter.faults & CONVERTER_FAULT_OVERCURRENT) == 0u);
    CHECK(converter.duty_command == 0.0f);
}

static void test_threshold_is_inclusive_and_multiple_faults_latch(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.input_current_a = config.current_trip_a;
    measurement.bus_voltage_v = config.bus_voltage_max_v;
    measurement.temperature_c = config.temperature_trip_c;
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK((converter.faults & CONVERTER_FAULT_OVERCURRENT) != 0u);
    CHECK((converter.faults & CONVERTER_FAULT_OVERVOLTAGE) != 0u);
    CHECK((converter.faults & CONVERTER_FAULT_OVERTEMPERATURE) != 0u);
    CHECK(converter.duty_command == 0.0f);
}

static void test_undervoltage_and_bad_timestep(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.bus_voltage_v = config.bus_voltage_min_v;
    converter_step(&converter, &config, &measurement, 0.0f);
    CHECK((converter.faults & CONVERTER_FAULT_UNDERVOLTAGE) != 0u);
    CHECK((converter.faults & CONVERTER_FAULT_BAD_TIMESTEP) != 0u);
    CHECK(converter.duty_command == 0.0f);
}

static void test_nonfinite_timestep_and_measurement(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, (float)NAN);
    CHECK((converter.faults & CONVERTER_FAULT_BAD_TIMESTEP) != 0u);
    converter_init(&converter);
    converter_arm(&converter);
    measurement.input_current_a = (float)NAN;
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK((converter.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    CHECK((converter.faults & CONVERTER_FAULT_OVERCURRENT) == 0u);
    converter_init(&converter);
    converter_arm(&converter);
    converter_step(&converter, &config, NULL, 0.001f);
    CHECK((converter.faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
}

static void test_invalid_config_drops_the_output(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(converter.duty_command != 0.0f);
    config.kp = -1.0f;
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(converter.duty_command == 0.0f);
    CHECK(converter.ramped_current_a == 0.0f);
    CHECK(!converter.armed);
    CHECK(converter.state == CONVERTER_STATE_IDLE);
    converter_step(&converter, NULL, &measurement, 0.001f);
    CHECK(converter.duty_command == 0.0f);
}

static void test_disarm_and_rejected_references(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_set_current_reference(&converter, (float)NAN);
    CHECK(converter.requested_current_a == 1.0f);
    converter_set_current_reference(&converter, (float)INFINITY);
    CHECK(converter.requested_current_a == 1.0f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    converter_disarm(&converter);
    CHECK(converter.state == CONVERTER_STATE_IDLE);
    CHECK(converter.duty_command == 0.0f);
    CHECK(converter.ramped_current_a == 0.0f);
    CHECK(converter.integrator == 0.0f);
    CHECK(converter.requested_current_a == 1.0f);
}

static void test_arm_blocked_by_fault_and_null_guards(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    measurement.bus_voltage_v = config.bus_voltage_min_v - 1.0f;
    converter_step(&converter, &config, &measurement, 0.001f);
    converter_arm(&converter);
    CHECK(!converter.armed);
    converter_init(NULL);
    converter_arm(NULL);
    converter_disarm(NULL);
    converter_set_current_reference(NULL, 1.0f);
    converter_step(NULL, &config, &measurement, 0.001f);
    CHECK(!converter_clear_faults(NULL, &config, &measurement));
    CHECK(!converter_config_is_valid(NULL));
    CHECK(converter_state_name(CONVERTER_STATE_IDLE)[0] == 'I');
    CHECK(converter_state_name(CONVERTER_STATE_RUN)[0] == 'R');
    CHECK(converter_state_name(CONVERTER_STATE_FAULT)[0] == 'F');
    CHECK(converter_state_name((converter_state_t)99)[0] == 'U');
}

static void test_rearm_after_clear_tracks_again(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = safe_measurement();
    converter_init(&converter);
    converter_arm(&converter);
    measurement.temperature_c = 90.0f;
    converter_step(&converter, &config, &measurement, 0.001f);
    measurement = safe_measurement();
    CHECK(converter_clear_faults(&converter, &config, &measurement));
    converter_set_current_reference(&converter, 0.5f);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 0.001f);
    CHECK(converter.state == CONVERTER_STATE_RUN);
    CHECK(converter.duty_command > 0.0f);
}

static void test_default_config_accepts_derivative_fields(void) {
    converter_config_t config = converter_default_config();
    CHECK(config.kd == 0.0f);
    CHECK(config.derivative_filter_hz == 0.0f);
    CHECK(converter_config_is_valid(&config));
    config.kd = -0.1f;
    CHECK(!converter_config_is_valid(&config));
    config = converter_default_config();
    config.current_trip_a = config.current_limit_a;
    CHECK(!converter_config_is_valid(&config));
    config = converter_default_config();
    config.duty_limit = 1.0f;
    CHECK(converter_config_is_valid(&config));
    config.duty_limit = 0.0f;
    CHECK(!converter_config_is_valid(&config));
}

int main(void) {
    test_safe_startup();
    test_arm_and_slew_limit();
    test_negative_slew_and_reference_clamp();
    test_fault_latches_and_forces_zero();
    test_fault_clear_requires_safe_disarmed_state();
    test_clear_rejected_while_armed_and_running();
    test_invalid_sample_fault();
    test_threshold_is_inclusive_and_multiple_faults_latch();
    test_undervoltage_and_bad_timestep();
    test_nonfinite_timestep_and_measurement();
    test_invalid_config_drops_the_output();
    test_disarm_and_rejected_references();
    test_arm_blocked_by_fault_and_null_guards();
    test_rearm_after_clear_tracks_again();
    test_default_config_accepts_derivative_fields();
    if (g_test_failures != 0) {
        fprintf(stderr, "%d converter checks failed\n", g_test_failures);
        return 1;
    }
    puts("all converter tests passed");
    return 0;
}
