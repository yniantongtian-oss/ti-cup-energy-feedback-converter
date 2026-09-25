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
    float kp;                       /* Id/Iq PI kp, V/A (demo) */
    float ki;                       /* Id/Iq PI ki, V/(A*s) */
    float current_limit_a;          /* |Id| limit */
    float current_trip_a;
    float bus_voltage_min_v;
    float bus_voltage_max_v;
    float temperature_trip_c;
    float duty_limit;
    float voltage_command_limit;    /* |ud|,|uq|,|u_alpha| clamp */
    float current_slew_a_per_s;
    float integrator_limit;
    float mod_bus_min_v;
    bool feedforward_enable;        /* duty = (u_alpha + U1) / Vbus */
    bool park_enable;               /* default ON: Park + Iq=0, still PI, no PR */
} converter_config_t;

typedef struct {
    float input_current_a;          /* i_alpha (measured) */
    float current_beta_a;           /* i_beta from current SOGI */
    float bus_voltage_v;
    float plant_voltage_v;          /* U1 */
    float temperature_c;
    float theta_rad;                /* from SOGI-PLL */
    bool theta_valid;
    bool sample_valid;
} converter_measurement_t;

typedef struct {
    converter_state_t state;
    uint32_t faults;
    bool armed;
    float requested_current_a;      /* Id reference */
    float ramped_current_a;
    float integrator_d;
    float integrator_q;
    float id_meas;
    float iq_meas;
    float ud;
    float uq;
    float voltage_command;          /* u_alpha after inv Park, volts */
    float duty_command;
    bool feedforward_active;
    bool park_active;
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
