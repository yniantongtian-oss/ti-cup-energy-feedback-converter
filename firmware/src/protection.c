#include "protection.h"

#include <math.h>
#include <stddef.h>

static bool finite_measurement(const converter_measurement_t *measurement) {
    return measurement != NULL && measurement->sample_valid &&
           isfinite(measurement->input_current_a) &&
           isfinite(measurement->bus_voltage_v) &&
           isfinite(measurement->temperature_c);
}

uint32_t protection_evaluate(const converter_config_t *config,
                             const converter_measurement_t *measurement,
                             float dt_s) {
    uint32_t faults = CONVERTER_FAULT_NONE;

    if (config == NULL) {
        return CONVERTER_FAULT_INVALID_SAMPLE | CONVERTER_FAULT_BAD_TIMESTEP;
    }
    if (!isfinite(dt_s) || dt_s <= 0.0f || dt_s > 1.0f) {
        faults |= CONVERTER_FAULT_BAD_TIMESTEP;
    }
    if (!finite_measurement(measurement)) {
        faults |= CONVERTER_FAULT_INVALID_SAMPLE;
        return faults;
    }
    if (fabsf(measurement->input_current_a) >= config->current_trip_a) {
        faults |= CONVERTER_FAULT_OVERCURRENT;
    }
    if (measurement->bus_voltage_v >= config->bus_voltage_max_v) {
        faults |= CONVERTER_FAULT_OVERVOLTAGE;
    }
    if (measurement->bus_voltage_v <= config->bus_voltage_min_v) {
        faults |= CONVERTER_FAULT_UNDERVOLTAGE;
    }
    if (measurement->temperature_c >= config->temperature_trip_c) {
        faults |= CONVERTER_FAULT_OVERTEMPERATURE;
    }
    return faults;
}
