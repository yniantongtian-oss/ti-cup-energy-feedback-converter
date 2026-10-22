#ifndef PID_H
#define PID_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_limit;
    float integrator_limit;
    float derivative_filter_hz;
} pid_gains_t;

typedef struct {
    float integrator;
    float derivative;
    float previous_measurement;
    bool initialized;
} pid_state_t;

bool pid_gains_valid(const pid_gains_t *gains);
void pid_reset(pid_state_t *state);

/* Derivative acts on the measurement so a reference step does not kick the output.
 * The integrator is frozen while the output is saturated and the error would
 * drive it further into the stop. Invalid arguments leave the state unchanged
 * and return 0.
 */
float pid_step(pid_state_t *state,
               const pid_gains_t *gains,
               float reference,
               float measurement,
               float dt_s);

#ifdef __cplusplus
}
#endif

#endif
