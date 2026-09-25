#include "bringup.h"

#include <math.h>
#include <stddef.h>

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bringup_config_t bringup_default_config(void) {
    bringup_config_t c;
    c.open_loop_duty = 0.05f;
    c.id_dc_a = 0.5f;
    c.bus_start_v = 12.0f;
    c.bus_target_v = 24.0f;
    c.bus_ramp_v_per_s = 4.0f; /* 3 s from 12→24 */
    c.v_out_ref = 8.0f;
    c.outer_kp = 0.05f;
    c.outer_ki = 0.5f;
    c.outer_integ_limit = 2.0f;
    c.id_limit_a = 2.0f;
    c.open_loop_dwell_ms = 200u;
    c.current_dc_dwell_ms = 300u;
    return c;
}

bool bringup_config_is_valid(const bringup_config_t *config) {
    if (config == NULL) return false;
    return isfinite(config->open_loop_duty) && fabsf(config->open_loop_duty) <= 1.0f &&
           isfinite(config->id_dc_a) && config->id_dc_a >= 0.0f &&
           isfinite(config->bus_start_v) && config->bus_start_v > 0.0f &&
           isfinite(config->bus_target_v) && config->bus_target_v >= config->bus_start_v &&
           isfinite(config->bus_ramp_v_per_s) && config->bus_ramp_v_per_s > 0.0f &&
           isfinite(config->v_out_ref) && config->v_out_ref >= 0.0f &&
           isfinite(config->outer_kp) && config->outer_kp >= 0.0f &&
           isfinite(config->outer_ki) && config->outer_ki >= 0.0f &&
           isfinite(config->outer_integ_limit) && config->outer_integ_limit >= 0.0f &&
           isfinite(config->id_limit_a) && config->id_limit_a > 0.0f &&
           config->open_loop_dwell_ms > 0u && config->current_dc_dwell_ms > 0u;
}

void bringup_init(bringup_t *b, const bringup_config_t *config) {
    if (b == NULL) return;
    b->config = config != NULL ? *config : bringup_default_config();
    b->mode = BRINGUP_IDLE;
    b->bus_cmd_v = b->config.bus_start_v;
    b->outer_integ = 0.0f;
    b->id_ref_a = 0.0f;
    b->open_loop_duty_cmd = 0.0f;
    b->force_open_loop = false;
    b->current_enable = false;
    b->outer_enable = false;
    b->mode_elapsed_ms = 0u;
    b->start_requested = false;
}

void bringup_request_start(bringup_t *b) {
    if (b == NULL) return;
    b->start_requested = true;
}

void bringup_request_stop(bringup_t *b) {
    if (b == NULL) return;
    b->start_requested = false;
    b->mode = BRINGUP_IDLE;
    b->force_open_loop = false;
    b->current_enable = false;
    b->outer_enable = false;
    b->id_ref_a = 0.0f;
    b->open_loop_duty_cmd = 0.0f;
    b->outer_integ = 0.0f;
    b->bus_cmd_v = b->config.bus_start_v;
    b->mode_elapsed_ms = 0u;
}

void bringup_fault(bringup_t *b) {
    if (b == NULL) return;
    b->mode = BRINGUP_FAULT;
    b->force_open_loop = false;
    b->current_enable = false;
    b->outer_enable = false;
    b->id_ref_a = 0.0f;
    b->open_loop_duty_cmd = 0.0f;
    b->start_requested = false;
}

static void enter(bringup_t *b, bringup_mode_t mode) {
    b->mode = mode;
    b->mode_elapsed_ms = 0u;
}

static void ramp_bus(bringup_t *b, float dt_s) {
    float max_d = b->config.bus_ramp_v_per_s * dt_s;
    float err = b->config.bus_target_v - b->bus_cmd_v;
    if (err > max_d) b->bus_cmd_v += max_d;
    else if (err < -max_d) b->bus_cmd_v -= max_d;
    else b->bus_cmd_v = b->config.bus_target_v;
}

void bringup_step_1khz(bringup_t *b,
                       float bus_meas_v,
                       float v_out_meas_v,
                       bool pll_locked,
                       bool hardware_ok,
                       uint32_t dt_ms) {
    float dt_s;
    if (b == NULL || !bringup_config_is_valid(&b->config)) return;
    if (dt_ms == 0u || dt_ms > 100u) dt_ms = 1u;
    dt_s = (float)dt_ms * 0.001f;

    if (!hardware_ok) {
        bringup_fault(b);
        return;
    }
    if (!b->start_requested) {
        if (b->mode != BRINGUP_IDLE && b->mode != BRINGUP_FAULT) bringup_request_stop(b);
        return;
    }
    if (b->mode == BRINGUP_FAULT) return;

    b->mode_elapsed_ms += dt_ms;
    ramp_bus(b, dt_s);

    switch (b->mode) {
        case BRINGUP_IDLE:
            b->bus_cmd_v = b->config.bus_start_v;
            enter(b, BRINGUP_OPEN_LOOP);
            /* fall through */
        case BRINGUP_OPEN_LOOP:
            b->force_open_loop = true;
            b->current_enable = false;
            b->outer_enable = false;
            b->open_loop_duty_cmd = b->config.open_loop_duty;
            b->id_ref_a = 0.0f;
            /* Require PLL lock before leaving open-loop (modulation/sample check under sync). */
            if (pll_locked && b->mode_elapsed_ms >= b->config.open_loop_dwell_ms) {
                enter(b, BRINGUP_CURRENT_DC);
            }
            break;

        case BRINGUP_CURRENT_DC:
            b->force_open_loop = false;
            b->current_enable = true;
            b->outer_enable = false;
            b->open_loop_duty_cmd = 0.0f;
            b->id_ref_a = b->config.id_dc_a;
            if (!pll_locked) {
                enter(b, BRINGUP_OPEN_LOOP);
                break;
            }
            if (b->mode_elapsed_ms >= b->config.current_dc_dwell_ms) {
                enter(b, BRINGUP_OUTER_V);
            }
            break;

        case BRINGUP_OUTER_V:
            b->force_open_loop = false;
            b->current_enable = true;
            b->outer_enable = true;
            b->open_loop_duty_cmd = 0.0f;
            if (!pll_locked) {
                enter(b, BRINGUP_OPEN_LOOP);
                break;
            }
            {
                float err = b->config.v_out_ref - v_out_meas_v;
                b->outer_integ = clampf(b->outer_integ + b->config.outer_ki * err * dt_s,
                                        -b->config.outer_integ_limit,
                                        b->config.outer_integ_limit);
                b->id_ref_a = clampf(b->config.outer_kp * err + b->outer_integ,
                                     -b->config.id_limit_a,
                                     b->config.id_limit_a);
            }
            break;

        case BRINGUP_FAULT:
        default:
            break;
    }

    (void)bus_meas_v;
}

const char *bringup_mode_name(bringup_mode_t mode) {
    switch (mode) {
        case BRINGUP_IDLE: return "IDLE";
        case BRINGUP_OPEN_LOOP: return "OPEN_LOOP";
        case BRINGUP_CURRENT_DC: return "CURRENT_DC";
        case BRINGUP_OUTER_V: return "OUTER_V";
        case BRINGUP_FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}
