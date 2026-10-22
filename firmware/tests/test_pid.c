#include "pid.h"

#include "test_harness.h"

#include <math.h>

static pid_gains_t base_gains(void) {
    pid_gains_t gains;
    gains.kp = 0.08f;
    gains.ki = 8.0f;
    gains.kd = 0.0f;
    gains.output_limit = 0.85f;
    gains.integrator_limit = 0.75f;
    gains.derivative_filter_hz = 0.0f;
    return gains;
}

static void test_pi_matches_the_current_loop_formula(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    float duty;
    pid_reset(&state);
    duty = pid_step(&state, &gains, 0.005f, 0.0f, 0.001f);
    CHECK_NEAR(duty, 0.00044f, 1e-6f);
    CHECK_NEAR(state.integrator, 0.00004f, 1e-7f);
    duty = pid_step(&state, &gains, 0.005f, 0.0f, 0.001f);
    CHECK_NEAR(duty, 0.00048f, 1e-6f);
    CHECK_NEAR(state.integrator, 0.00008f, 1e-7f);
}

static void test_integrator_freezes_into_saturation_and_unwinds(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    float duty;
    pid_reset(&state);
    gains.kp = 0.0f;
    gains.ki = 5.0f;
    gains.output_limit = 0.1f;
    gains.integrator_limit = 1.0f;
    state.integrator = 0.2f;
    duty = pid_step(&state, &gains, 0.05f, 0.0f, 0.1f);
    CHECK_NEAR(duty, 0.1f, 1e-6f);
    CHECK_NEAR(state.integrator, 0.2f, 1e-6f);
    duty = pid_step(&state, &gains, 0.0f, 0.05f, 0.1f);
    CHECK_NEAR(duty, 0.1f, 1e-6f);
    CHECK_NEAR(state.integrator, 0.175f, 1e-5f);
}

static void test_negative_saturation_freezes(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    float duty;
    pid_reset(&state);
    gains.kp = 2.0f;
    gains.ki = 50.0f;
    gains.output_limit = 0.2f;
    duty = pid_step(&state, &gains, 0.0f, 1.0f, 0.01f);
    CHECK_NEAR(duty, -0.2f, 1e-6f);
    CHECK_NEAR(state.integrator, 0.0f, 1e-6f);
    duty = pid_step(&state, &gains, 0.0f, 1.0f, 0.01f);
    CHECK_NEAR(state.integrator, 0.0f, 1e-6f);
    CHECK_NEAR(duty, -0.2f, 1e-6f);
}

static void test_integrator_limit_applies_inside_the_duty_range(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    float duty;
    pid_reset(&state);
    gains.kp = 0.0f;
    gains.ki = 10.0f;
    gains.output_limit = 1.0f;
    gains.integrator_limit = 0.05f;
    duty = pid_step(&state, &gains, 1.0f, 0.0f, 0.01f);
    CHECK_NEAR(duty, 0.05f, 1e-6f);
    CHECK_NEAR(state.integrator, 0.05f, 1e-6f);
}

static void test_derivative_on_measurement_opposes_the_jump(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    float first;
    float second;
    pid_reset(&state);
    gains.kp = 0.0f;
    gains.ki = 0.0f;
    gains.kd = 0.01f;
    gains.output_limit = 1.0f;
    first = pid_step(&state, &gains, 0.0f, 0.0f, 0.1f);
    CHECK_NEAR(first, 0.0f, 1e-6f);
    second = pid_step(&state, &gains, 0.0f, 1.0f, 0.1f);
    CHECK_NEAR(second, -0.1f, 1e-5f);
}

static void test_derivative_filter_is_slower_than_the_raw_difference(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    float duty;
    pid_reset(&state);
    gains.kp = 0.0f;
    gains.ki = 0.0f;
    gains.kd = 0.01f;
    gains.output_limit = 1.0f;
    gains.derivative_filter_hz = 10.0f;
    (void)pid_step(&state, &gains, 0.0f, 0.0f, 0.1f);
    duty = pid_step(&state, &gains, 0.0f, 1.0f, 0.1f);
    CHECK(duty < 0.0f);
    CHECK(duty > -0.1f);
}

static void test_reset_and_rejected_steps_hold_the_state(void) {
    pid_state_t state;
    pid_gains_t gains = base_gains();
    pid_reset(&state);
    (void)pid_step(&state, &gains, 1.0f, 0.0f, 0.01f);
    CHECK(state.integrator > 0.0f);
    CHECK(pid_step(&state, &gains, (float)NAN, 0.0f, 0.01f) == 0.0f);
    CHECK(state.integrator > 0.0f);
    CHECK(pid_step(&state, NULL, 1.0f, 0.0f, 0.01f) == 0.0f);
    CHECK(pid_step(NULL, &gains, 1.0f, 0.0f, 0.01f) == 0.0f);
    CHECK(pid_step(&state, &gains, 1.0f, 0.0f, 0.0f) == 0.0f);
    pid_reset(&state);
    CHECK(state.integrator == 0.0f);
    CHECK(!state.initialized);
    pid_reset(NULL);
    CHECK(!pid_gains_valid(NULL));
    gains.output_limit = 0.0f;
    CHECK(!pid_gains_valid(&gains));
}

int main(void) {
    test_pi_matches_the_current_loop_formula();
    test_integrator_freezes_into_saturation_and_unwinds();
    test_negative_saturation_freezes();
    test_integrator_limit_applies_inside_the_duty_range();
    test_derivative_on_measurement_opposes_the_jump();
    test_derivative_filter_is_slower_than_the_raw_difference();
    test_reset_and_rejected_steps_hold_the_state();
    if (g_test_failures != 0) {
        fprintf(stderr, "%d pid checks failed\n", g_test_failures);
        return 1;
    }
    puts("all pid tests passed");
    return 0;
}
