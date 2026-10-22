#include "protection.h"

#include "test_harness.h"

#include <math.h>

static converter_measurement_t measurement_of(float current_a, float bus_v, float temperature_c) {
    converter_measurement_t measurement;
    measurement.input_current_a = current_a;
    measurement.bus_voltage_v = bus_v;
    measurement.temperature_c = temperature_c;
    measurement.sample_valid = true;
    return measurement;
}

static void test_documented_fault_bits(void) {
    CHECK(CONVERTER_FAULT_NONE == 0u);
    CHECK(CONVERTER_FAULT_OVERCURRENT == (1u << 0));
    CHECK(CONVERTER_FAULT_OVERVOLTAGE == (1u << 1));
    CHECK(CONVERTER_FAULT_UNDERVOLTAGE == (1u << 2));
    CHECK(CONVERTER_FAULT_OVERTEMPERATURE == (1u << 3));
    CHECK(CONVERTER_FAULT_INVALID_SAMPLE == (1u << 4));
    CHECK(CONVERTER_FAULT_BAD_TIMESTEP == (1u << 5));
    CHECK(CONVERTER_FAULT_HARDWARE == (1u << 6));
    CHECK(CONVERTER_FAULT_ESTOP == (1u << 7));
    CHECK(CONVERTER_FAULT_COMM_TIMEOUT == (1u << 8));
}

static void test_each_electrical_threshold(void) {
    converter_config_t config = converter_default_config();
    converter_measurement_t safe = measurement_of(0.0f, 12.0f, 25.0f);
    converter_measurement_t measurement;
    CHECK(protection_evaluate(&config, &safe, 0.001f) == CONVERTER_FAULT_NONE);
    CHECK(protection_evaluate(&config, &safe, 1.0f) == CONVERTER_FAULT_NONE);

    measurement = safe;
    measurement.input_current_a = config.current_trip_a - 0.01f;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_NONE);
    measurement.input_current_a = -config.current_trip_a;
    CHECK((protection_evaluate(&config, &measurement, 0.001f) & CONVERTER_FAULT_OVERCURRENT) != 0u);

    measurement = safe;
    measurement.bus_voltage_v = config.bus_voltage_max_v - 0.01f;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_NONE);
    measurement.bus_voltage_v = config.bus_voltage_max_v;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_OVERVOLTAGE);

    measurement = safe;
    measurement.bus_voltage_v = config.bus_voltage_min_v + 0.01f;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_NONE);
    measurement.bus_voltage_v = config.bus_voltage_min_v;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_UNDERVOLTAGE);

    measurement = safe;
    measurement.temperature_c = config.temperature_trip_c - 0.01f;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_NONE);
    measurement.temperature_c = config.temperature_trip_c;
    CHECK(protection_evaluate(&config, &measurement, 0.001f) == CONVERTER_FAULT_OVERTEMPERATURE);
}

static void test_invalid_samples_hide_electrical_bits(void) {
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = measurement_of(100.0f, 100.0f, 200.0f);
    uint32_t faults;
    measurement.sample_valid = false;
    faults = protection_evaluate(&config, &measurement, -1.0f);
    CHECK(faults == (CONVERTER_FAULT_INVALID_SAMPLE | CONVERTER_FAULT_BAD_TIMESTEP));
    measurement.sample_valid = true;
    measurement.temperature_c = (float)NAN;
    faults = protection_evaluate(&config, &measurement, 0.001f);
    CHECK(faults == CONVERTER_FAULT_INVALID_SAMPLE);
    faults = protection_evaluate(&config, NULL, 2.0f);
    CHECK(faults == (CONVERTER_FAULT_INVALID_SAMPLE | CONVERTER_FAULT_BAD_TIMESTEP));
    faults = protection_evaluate(NULL, &measurement, 0.001f);
    CHECK((faults & CONVERTER_FAULT_INVALID_SAMPLE) != 0u);
    CHECK((faults & CONVERTER_FAULT_BAD_TIMESTEP) != 0u);
}

static void test_converter_step_uses_the_same_thresholds(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = measurement_of(0.0f, config.bus_voltage_min_v + 0.1f, 25.0f);
    converter_init(&converter);
    converter_arm(&converter);
    converter_step(&converter, &config, &measurement, 1.0f);
    CHECK(converter.state == CONVERTER_STATE_RUN);
    measurement.bus_voltage_v = config.bus_voltage_min_v;
    converter_step(&converter, &config, &measurement, 1.01f);
    CHECK((converter.faults & CONVERTER_FAULT_UNDERVOLTAGE) != 0u);
    CHECK((converter.faults & CONVERTER_FAULT_BAD_TIMESTEP) != 0u);
}

int main(void) {
    test_documented_fault_bits();
    test_each_electrical_threshold();
    test_invalid_samples_hide_electrical_bits();
    test_converter_step_uses_the_same_thresholds();
    if (g_test_failures != 0) {
        fprintf(stderr, "%d protection checks failed\n", g_test_failures);
        return 1;
    }
    puts("all protection tests passed");
    return 0;
}
