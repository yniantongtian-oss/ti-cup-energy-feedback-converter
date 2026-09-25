#include "converter_crc.h"

uint16_t crc16_modbus(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    size_t i;
    uint8_t j;
    if (data == NULL || len == 0u) return crc;
    for (i = 0u; i < len; ++i) {
        crc ^= (uint16_t)data[i];
        for (j = 0u; j < 8u; ++j) {
            if ((crc & 0x0001u) != 0u) {
                crc = (crc >> 1u) ^ 0xA001u;
            } else {
                crc >>= 1u;
            }
        }
    }
    return crc;
}

uint32_t crc32_iso(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFuL;
    size_t i;
    uint8_t j;
    if (data == NULL || len == 0u) return crc ^ 0xFFFFFFFFuL;
    for (i = 0u; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (j = 0u; j < 8u; ++j) {
            if ((crc & 0x00000001uL) != 0uL) {
                crc = (crc >> 1u) ^ 0xEDB88320uL;
            } else {
                crc >>= 1u;
            }
        }
    }
    return crc ^ 0xFFFFFFFFuL;
}

