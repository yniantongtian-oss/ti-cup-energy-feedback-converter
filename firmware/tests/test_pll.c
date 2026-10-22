#include "pll.h"

#include "test_harness.h"

#include <math.h>

static const float TWO_PI = 6.28318530717958647692f;

static void step_tone(pll_state_t *state,
                      const pll_config_t *config,
                      float frequency_hz,
                      int index,
                      float dt_s) {
    const float time_s = (float)index * dt_s;
    const float voltage = cosf(TWO_PI * frequency_hz * time_s);
    CHECK(pll_step(state, config, voltage, dt_s));
    CHECK(state->theta_rad >= 0.0f);
    CHECK(state->theta_rad < TWO_PI + 1e-4f);
}

static float mean_frequency(const pll_config_t *config, float frequency_hz, float dt_s) {
    pll_state_t state;
    int index;
    const int steps = (int)(1.0f / dt_s);
    const int tail = (int)(0.2f / dt_s);
    double sum = 0.0;
    pll_reset(&state, config);
    for (index = 0; index < steps; ++index) {
        const float time_s = (float)index * dt_s;
        const float voltage = cosf(TWO_PI * frequency_hz * time_s);
        CHECK(pll_step(&state, config, voltage, dt_s));
        if (index >= steps - tail) {
            sum += (double)state.frequency_hz;
        }
    }
    return (float)(sum / (double)tail);
}

static void test_locks_to_nominal_and_nearby_grid(void) {
    pll_config_t config = pll_default_config();
    pll_state_t state;
    const float dt_s = 1.0e-4f;
    int index;
    pll_reset(&state, &config);
    CHECK(!state.locked);
    CHECK_NEAR(state.frequency_hz, 50.0f, 1e-6f);
    for (index = 0; index < 5200; ++index) {
        step_tone(&state, &config, 50.0f, index, dt_s);
        if (index == 199) {
            CHECK(!state.locked);
        }
    }
    CHECK(state.locked);
    CHECK(fabsf(state.vq) < 0.2f);
    CHECK(state.vd > 0.5f);
    CHECK_NEAR(mean_frequency(&config, 50.0f, dt_s), 50.0f, 0.05f);
    CHECK_NEAR(mean_frequency(&config, 49.0f, dt_s), 49.0f, 0.05f);
    CHECK_NEAR(mean_frequency(&config, 51.0f, dt_s), 51.0f, 0.05f);
}

static void test_dc_input_does_not_lock(void) {
    pll_config_t config = pll_default_config();
    pll_state_t state;
    int index;
    pll_reset(&state, &config);
    for (index = 0; index < 5000; ++index) {
        CHECK(pll_step(&state, &config, 1.0f, 1.0e-4f));
    }
    CHECK(!state.locked);
    CHECK(state.frequency_hz <= config.nominal_hz + config.frequency_limit_hz);
    CHECK(state.frequency_hz >= config.nominal_hz - config.frequency_limit_hz);
}

static void test_frequency_stays_inside_the_configured_window(void) {
    pll_config_t config = pll_default_config();
    pll_state_t state;
    int index;
    pll_reset(&state, &config);
    for (index = 0; index < 4000; ++index) {
        const float voltage = cosf(TWO_PI * 20.0f * (float)index * 1.0e-4f);
        CHECK(pll_step(&state, &config, voltage, 1.0e-4f));
        CHECK(state.frequency_hz <= config.nominal_hz + config.frequency_limit_hz + 1e-3f);
        CHECK(state.frequency_hz >= config.nominal_hz - config.frequency_limit_hz - 1e-3f);
    }
}

static void test_rejected_inputs_do_not_move_the_state(void) {
    pll_config_t config = pll_default_config();
    pll_state_t state;
    float held_frequency;
    pll_reset(&state, &config);
    CHECK(pll_step(&state, &config, 0.0f, 1.0e-4f));
    held_frequency = state.frequency_hz;
    CHECK(!pll_step(&state, &config, (float)NAN, 1.0e-4f));
    CHECK(!pll_step(&state, &config, 0.0f, 0.0f));
    CHECK(!pll_step(NULL, &config, 0.0f, 1.0e-4f));
    CHECK(!pll_step(&state, NULL, 0.0f, 1.0e-4f));
    CHECK(state.frequency_hz == held_frequency);
    config.sogi_k = 0.0f;
    CHECK(!pll_config_valid(&config));
    config = pll_default_config();
    config.lock_vq_ratio = 0.0f;
    CHECK(!pll_config_valid(&config));
    CHECK(!pll_config_valid(NULL));
    pll_reset(NULL, &config);
}

int main(void) {
    test_locks_to_nominal_and_nearby_grid();
    test_dc_input_does_not_lock();
    test_frequency_stays_inside_the_configured_window();
    test_rejected_inputs_do_not_move_the_state();
    if (g_test_failures != 0) {
        fprintf(stderr, "%d pll checks failed\n", g_test_failures);
        return 1;
    }
    puts("all pll tests passed");
    return 0;
}
