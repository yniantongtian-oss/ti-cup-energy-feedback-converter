#include "converter.h"

#include <stdio.h>

int main(void) {
    converter_t converter;
    converter_config_t config = converter_default_config();
    converter_measurement_t measurement = {
        .input_current_a = 0.0f,
        .bus_voltage_v = 12.0f,
        .temperature_c = 25.0f,
        .sample_valid = true,
    };

    converter_init(&converter);
    converter_set_current_reference(&converter, 1.0f);
    converter_arm(&converter);

    puts("time_s,state,current_a,duty");
    for (int step = 0; step < 200; ++step) {
        const float dt = 0.001f;
        converter_step(&converter, &config, &measurement, dt);
        measurement.input_current_a +=
            (converter.duty_command * 2.0f - measurement.input_current_a) * 0.05f;
        if (step % 10 == 0) {
            printf("%.3f,%s,%.4f,%.4f\n",
                   step * dt,
                   converter_state_name(converter.state),
                   measurement.input_current_a,
                   converter.duty_command);
        }
    }
    return converter.state == CONVERTER_STATE_RUN ? 0 : 1;
}
