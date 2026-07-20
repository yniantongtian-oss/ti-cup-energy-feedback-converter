#include "app_flash_params.h"

#include "converter_crc.h"

#include "main.h"  /* HAL_FLASH_Unlock / HAL_FLASH_Lock / HAL_FLASHEx_Erase */

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal layout                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    converter_flash_params_t params;
    uint32_t crc32;
} flash_page_layout_t;

/* Total bytes covered by the CRC (everything before the crc32 field). */
#define LAYOUT_CRC_LEN (offsetof(flash_page_layout_t, crc32))

/* ------------------------------------------------------------------ */
/* Validation                                                          */
/* ------------------------------------------------------------------ */

bool AppFlashParams_IsValid(const converter_flash_params_t *params) {
    if (params == NULL) return false;
    /* Control config checks (mirrors converter_config_is_valid). */
    if (!isfinite(params->kp) || params->kp < 0.0f) return false;
    if (!isfinite(params->ki) || params->ki < 0.0f) return false;
    if (!isfinite(params->current_limit_a) || params->current_limit_a <= 0.0f) return false;
    if (!isfinite(params->current_trip_a) || params->current_trip_a <= params->current_limit_a) return false;
    if (!isfinite(params->bus_voltage_min_v) || params->bus_voltage_min_v < 0.0f) return false;
    if (!isfinite(params->bus_voltage_max_v) || params->bus_voltage_max_v <= params->bus_voltage_min_v) return false;
    if (!isfinite(params->temperature_trip_c) || params->temperature_trip_c <= 0.0f) return false;
    if (!isfinite(params->duty_limit) || params->duty_limit <= 0.0f || params->duty_limit > 1.0f) return false;
    if (!isfinite(params->current_slew_a_per_s) || params->current_slew_a_per_s <= 0.0f) return false;
    if (!isfinite(params->integrator_limit) || params->integrator_limit < 0.0f) return false;
    /* Runtime calibration checks (mirrors converter_runtime_config_is_valid). */
    if (!isfinite(params->current_gain_a_per_count)) return false;
    if (!isfinite(params->current_offset_a)) return false;
    if (!isfinite(params->bus_gain_v_per_count) || params->bus_gain_v_per_count <= 0.0f) return false;
    if (!isfinite(params->bus_offset_v)) return false;
    if (!isfinite(params->temperature_gain_c_per_count)) return false;
    if (!isfinite(params->temperature_offset_c)) return false;
    if (!isfinite(params->filter_alpha) ||
        params->filter_alpha <= 0.0f || params->filter_alpha > 1.0f) return false;
    if (params->command_timeout_ms == 0u) return false;
    if (params->modbus_address == 0u || params->modbus_address > 247u) return false;
    return true;
}

/* ------------------------------------------------------------------ */
/* Load                                                                */
/* ------------------------------------------------------------------ */

bool AppFlashParams_Load(converter_flash_params_t *params) {
    const flash_page_layout_t *layout;
    uint32_t expected_crc;

    if (params == NULL) return false;

    layout = (const flash_page_layout_t *)(uintptr_t)APP_FLASH_PARAMS_PAGE_ADDR;

    if (layout->magic != APP_FLASH_PARAMS_MAGIC) return false;
    if (layout->version != APP_FLASH_PARAMS_VERSION) return false;

    expected_crc = crc32_iso((const uint8_t *)layout, LAYOUT_CRC_LEN);
    if (layout->crc32 != expected_crc) return false;

    if (!AppFlashParams_IsValid(&layout->params)) return false;

    *params = layout->params;
    return true;
}

/* ------------------------------------------------------------------ */
/* Save                                                                */
/* ------------------------------------------------------------------ */

bool AppFlashParams_Save(const converter_flash_params_t *params) {
    flash_page_layout_t layout;
    const uint16_t *src;
    uint32_t addr;
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error;
    size_t words;
    size_t i;

    if (params == NULL || !AppFlashParams_IsValid(params)) return false;

    memset(&layout, 0xFF, sizeof(layout));
    layout.magic   = APP_FLASH_PARAMS_MAGIC;
    layout.version = APP_FLASH_PARAMS_VERSION;
    layout.reserved = 0xFFFFu;
    layout.params  = *params;
    layout.crc32   = crc32_iso((const uint8_t *)&layout, LAYOUT_CRC_LEN);

    if (HAL_FLASH_Unlock() != HAL_OK) return false;

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = APP_FLASH_PARAMS_PAGE_ADDR;
    erase.NbPages     = 1u;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK || page_error != 0xFFFFFFFFuL) {
        (void)HAL_FLASH_Lock();
        return false;
    }

    /* STM32F103 Flash is programmed in 16-bit half-words. */
    src   = (const uint16_t *)&layout;
    addr  = APP_FLASH_PARAMS_PAGE_ADDR;
    words = (sizeof(layout) + 1u) / 2u;
    for (i = 0u; i < words; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, src[i]) != HAL_OK) {
            (void)HAL_FLASH_Lock();
            return false;
        }
        addr += 2u;
    }

    (void)HAL_FLASH_Lock();

    /* Readback verification. */
    {
        const flash_page_layout_t *readback =
            (const flash_page_layout_t *)(uintptr_t)APP_FLASH_PARAMS_PAGE_ADDR;
        return (readback->magic == APP_FLASH_PARAMS_MAGIC) &&
               (readback->crc32 == layout.crc32);
    }
}

/* ------------------------------------------------------------------ */
/* Config <-> Flash conversion helpers                                 */
/* ------------------------------------------------------------------ */

void AppFlashParams_FromRuntime(converter_flash_params_t *params,
                                const converter_config_t *ctrl,
                                const converter_runtime_config_t *rt,
                                uint8_t modbus_address) {
    if (params == NULL) return;
    if (ctrl != NULL) {
        params->kp                  = ctrl->kp;
        params->ki                  = ctrl->ki;
        params->current_limit_a     = ctrl->current_limit_a;
        params->current_trip_a      = ctrl->current_trip_a;
        params->bus_voltage_min_v   = ctrl->bus_voltage_min_v;
        params->bus_voltage_max_v   = ctrl->bus_voltage_max_v;
        params->temperature_trip_c  = ctrl->temperature_trip_c;
        params->duty_limit          = ctrl->duty_limit;
        params->current_slew_a_per_s = ctrl->current_slew_a_per_s;
        params->integrator_limit    = ctrl->integrator_limit;
    }
    if (rt != NULL) {
        params->current_gain_a_per_count     = rt->current_gain_a_per_count;
        params->current_offset_a             = rt->current_offset_a;
        params->bus_gain_v_per_count         = rt->bus_gain_v_per_count;
        params->bus_offset_v                 = rt->bus_offset_v;
        params->temperature_gain_c_per_count = rt->temperature_gain_c_per_count;
        params->temperature_offset_c         = rt->temperature_offset_c;
        params->filter_alpha                 = rt->filter_alpha;
        params->command_timeout_ms           = rt->command_timeout_ms;
    }
    params->modbus_address = modbus_address;
    params->reserved[0] = 0u;
    params->reserved[1] = 0u;
    params->reserved[2] = 0u;
}

void AppFlashParams_ToRuntime(const converter_flash_params_t *params,
                              converter_config_t *ctrl,
                              converter_runtime_config_t *rt,
                              uint8_t *modbus_address) {
    if (params == NULL) return;
    if (ctrl != NULL) {
        ctrl->kp                  = params->kp;
        ctrl->ki                  = params->ki;
        ctrl->current_limit_a     = params->current_limit_a;
        ctrl->current_trip_a      = params->current_trip_a;
        ctrl->bus_voltage_min_v   = params->bus_voltage_min_v;
        ctrl->bus_voltage_max_v   = params->bus_voltage_max_v;
        ctrl->temperature_trip_c  = params->temperature_trip_c;
        ctrl->duty_limit          = params->duty_limit;
        ctrl->current_slew_a_per_s = params->current_slew_a_per_s;
        ctrl->integrator_limit    = params->integrator_limit;
    }
    if (rt != NULL) {
        rt->current_gain_a_per_count     = params->current_gain_a_per_count;
        rt->current_offset_a             = params->current_offset_a;
        rt->bus_gain_v_per_count         = params->bus_gain_v_per_count;
        rt->bus_offset_v                 = params->bus_offset_v;
        rt->temperature_gain_c_per_count = params->temperature_gain_c_per_count;
        rt->temperature_offset_c         = params->temperature_offset_c;
        rt->filter_alpha                 = params->filter_alpha;
        rt->command_timeout_ms           = params->command_timeout_ms;
    }
    if (modbus_address != NULL) *modbus_address = params->modbus_address;
}
