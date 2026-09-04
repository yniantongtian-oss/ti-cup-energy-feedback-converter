#ifndef CONVERTER_H
#define CONVERTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONVERTER_STATE_IDLE = 0,
    CONVERTER_STATE_RUN,
    CONVERTER_STATE_FAULT
} converter_state_t;

typedef enum {
    CONVERTER_FAULT_NONE = 0,
    CONVERTER_FAULT_INVALID_SAMPLE = 1u << 0,
    CONVERTER_FAULT_OVERCURRENT = 1u << 1,
    CONVERTER_FAULT_OVERVOLTAGE = 1u << 2,
    CONVERTER_FAULT_UNDERVOLTAGE = 1u << 3,
    CONVERTER_FAULT_OVERTEMPERATURE = 1u << 4,
    CONVERTER_FAULT_BAD_TIMESTEP = 1u << 5
} converter_fault_t;

typedef struct {
    float kp;                       /* V/A, demo value, not a 20 kHz tune */
    float ki;                       /* V/(A*s) */
    float current_limit_a;
    float current_trip_a;
    float bus_voltage_min_v;
    float bus_voltage_max_v;
    float temperature_trip_c;
    float duty_limit;
    float voltage_command_limit;    /* |u_i| clamp, volts */
    float current_slew_a_per_s;
    float integrator_limit;         /* volts */
    float mod_bus_min_v;            /* refuse to divide if Vbus below this */
    bool feedforward_enable;        /* default ON: duty = (u_i + U1) / Vbus */
} converter_config_t;

typedef struct {
    float input_current_a;
    float bus_voltage_v;
    float plant_voltage_v;          /* U1, plant-side voltage */
    float temperature_c;
    bool sample_valid;
} converter_measurement_t;

typedef struct {
    converter_state_t state;
    uint32_t faults;
    bool armed;
    float requested_current_a;
    float ramped_current_a;
    float integrator;
    float voltage_command;          /* current-loop output u_i, volts */
    float duty_command;             /* (u_i + U1) / Vbus, signed */
    bool feedforward_active;
} converter_t;

converter_config_t converter_default_config(void);
void converter_init(converter_t *converter);
bool converter_config_is_valid(const converter_config_t *config);
void converter_arm(converter_t *converter);
void converter_disarm(converter_t *converter);
void converter_set_current_reference(converter_t *converter, float current_a);
void converter_step(converter_t *converter,
                    const converter_config_t *config,
                    const converter_measurement_t *measurement,
                    float dt_s);
bool converter_clear_faults(converter_t *converter,
                            const converter_config_t *config,
                            const converter_measurement_t *measurement);
const char *converter_state_name(converter_state_t state);

#ifdef __cplusplus
}
#endif

#endif
