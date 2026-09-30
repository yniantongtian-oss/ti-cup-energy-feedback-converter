#ifndef BRINGUP_H
#define BRINGUP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Step E power-on sequence (TIDUAY6E-style), topology unchanged:
 *   OPEN_LOOP  — check modulation + sampling (fixed duty)
 *   CURRENT_DC — current loop only, DC Id reference
 *   OUTER_V    — enable outer voltage; Id_ref from outer PI
 * Bus command ramps slowly from bus_start_v toward bus_target_v.
 * Does not configure TIM1 BKIN (step F).
 */

typedef enum {
    BRINGUP_IDLE = 0,
    BRINGUP_OPEN_LOOP,
    BRINGUP_CURRENT_DC,
    BRINGUP_OUTER_V,
    BRINGUP_FAULT
} bringup_mode_t;

typedef struct {
    float open_loop_duty;           /* signed, small */
    float id_dc_a;                  /* DC current ref in CURRENT_DC */
    float bus_start_v;
    float bus_target_v;
    float bus_ramp_v_per_s;
    float v_out_ref;                /* outer voltage amplitude ref */
    float outer_kp;
    float outer_ki;
    float outer_integ_limit;
    float id_limit_a;
    uint32_t open_loop_dwell_ms;
    uint32_t current_dc_dwell_ms;
} bringup_config_t;

typedef struct {
    bringup_mode_t mode;
    bringup_config_t config;
    float bus_cmd_v;                /* slow-ramped bus command (software) */
    float outer_integ;
    float id_ref_a;                 /* output to current loop */
    float open_loop_duty_cmd;       /* used only in OPEN_LOOP */
    bool force_open_loop;           /* true → bypass current PI duty */
    bool current_enable;            /* allow current PI */
    bool outer_enable;
    uint32_t mode_elapsed_ms;
    bool start_requested;
} bringup_t;

bringup_config_t bringup_default_config(void);
bool bringup_config_is_valid(const bringup_config_t *config);
void bringup_init(bringup_t *b, const bringup_config_t *config);
void bringup_request_start(bringup_t *b);
void bringup_request_stop(bringup_t *b);
void bringup_fault(bringup_t *b);

/*
 * Call at ~1 kHz. Inputs: measured bus, outer voltage amplitude (e.g. PLL amp),
 * pll_locked, hardware_ok.
 */
void bringup_step_1khz(bringup_t *b,
                       float bus_meas_v,
                       float v_out_meas_v,
                       bool pll_locked,
                       bool hardware_ok,
                       uint32_t dt_ms);

const char *bringup_mode_name(bringup_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
