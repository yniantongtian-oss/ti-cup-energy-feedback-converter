#ifndef APP_FLASH_PARAMS_H
#define APP_FLASH_PARAMS_H

/**
 * @file app_flash_params.h
 * @brief Flash-backed parameter persistence for STM32F103C8T6.
 *
 * Parameters are stored in the last 1 KB page of Flash
 * (configurable via APP_FLASH_PARAMS_PAGE_ADDR).  The layout is:
 *
 *   [0x00]  magic     uint32  APP_FLASH_PARAMS_MAGIC
 *   [0x04]  version   uint16  APP_FLASH_PARAMS_VERSION
 *   [0x06]  reserved  uint16  0xFFFF
 *   [0x08]  params    converter_flash_params_t
 *   [last]  crc32     uint32  CRC-32/ISO-HDLC over bytes [0x00 .. last-4]
 *
 * If the page is blank, corrupt, or the version does not match,
 * AppFlashParams_Load() returns false and the caller must use defaults.
 *
 * All write operations erase the page first; Flash wear is therefore
 * bounded by the number of Save() calls, not read operations.
 */

#include "converter.h"
#include "converter_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Compile-time constants — adjust for your Flash map.                */
/* ------------------------------------------------------------------ */

/** Last 1 KB page on a 64 KB STM32F103C8T6 device. */
#ifndef APP_FLASH_PARAMS_PAGE_ADDR
#define APP_FLASH_PARAMS_PAGE_ADDR (0x0800FC00uL)
#endif

#define APP_FLASH_PARAMS_PAGE_SIZE (1024u)
#define APP_FLASH_PARAMS_MAGIC     (0x45464350uL) /* "PCFE" */
#define APP_FLASH_PARAMS_VERSION   (0x0001u)

/* ------------------------------------------------------------------ */
/* Stored parameter structure                                          */
/* ------------------------------------------------------------------ */

/**
 * All converter parameters that are persisted to Flash.
 * Calibration fields from converter_runtime_config_t are stored without
 * command_timeout_ms and adc_full_scale (those are compile-time constants).
 */
typedef struct {
    /* Control loop — mirrors converter_config_t.
     * Constraints: kp >= 0, ki >= 0, current_limit_a > 0,
     *   current_trip_a > current_limit_a (hard requirement for fault detection),
     *   bus_voltage_max_v > bus_voltage_min_v >= 0,
     *   0 < duty_limit <= 1, current_slew_a_per_s > 0,
     *   integrator_limit >= 0, temperature_trip_c > 0. */
    float kp;
    float ki;
    float current_limit_a;
    float current_trip_a;        /* Must be strictly greater than current_limit_a */
    float bus_voltage_min_v;
    float bus_voltage_max_v;
    float temperature_trip_c;
    float duty_limit;
    float current_slew_a_per_s;
    float integrator_limit;

    /* Calibration */
    float current_gain_a_per_count;
    float current_offset_a;
    float bus_gain_v_per_count;
    float bus_offset_v;
    float temperature_gain_c_per_count;
    float temperature_offset_c;
    float filter_alpha;

    /* Communication */
    uint32_t command_timeout_ms;
    uint8_t  modbus_address;
    uint8_t  reserved[3];
} converter_flash_params_t;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * Validate that @p params values are within safe operating ranges.
 * Returns true only when every field passes the same checks as
 * converter_config_is_valid() and converter_runtime_config_is_valid().
 */
bool AppFlashParams_IsValid(const converter_flash_params_t *params);

/**
 * Load parameters from Flash.
 * @param[out] params  Destination structure (written only on success).
 * @return             true on success (magic, version and CRC all match).
 */
bool AppFlashParams_Load(converter_flash_params_t *params);

/**
 * Save parameters to Flash (erase + program).
 * Must NOT be called while the converter is running (RUN state).
 * @param[in] params  Parameters to persist.
 * @return            true if Flash write succeeded and readback matches.
 */
bool AppFlashParams_Save(const converter_flash_params_t *params);

/**
 * Populate @p params from the current runtime configuration.
 * Useful before calling AppFlashParams_Save().
 */
void AppFlashParams_FromRuntime(converter_flash_params_t *params,
                                const converter_config_t *ctrl,
                                const converter_runtime_config_t *rt,
                                uint8_t modbus_address);

/**
 * Apply loaded parameters back into the split config structures.
 */
void AppFlashParams_ToRuntime(const converter_flash_params_t *params,
                              converter_config_t *ctrl,
                              converter_runtime_config_t *rt,
                              uint8_t *modbus_address);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASH_PARAMS_H */
