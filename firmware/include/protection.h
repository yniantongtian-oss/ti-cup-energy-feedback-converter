#ifndef PROTECTION_H
#define PROTECTION_H

#include "converter.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trips at the configured threshold itself. A non-finite or invalid sample
 * reports INVALID_SAMPLE and does not also report electrical trips.
 */
uint32_t protection_evaluate(const converter_config_t *config,
                             const converter_measurement_t *measurement,
                             float dt_s);

#ifdef __cplusplus
}
#endif

#endif
