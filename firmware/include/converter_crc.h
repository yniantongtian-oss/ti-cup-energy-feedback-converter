#ifndef CONVERTER_CRC_H
#define CONVERTER_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CRC-16/IBM (Modbus) — polynomial 0xA001 (reflected 0x8005).
 * @param data  Pointer to the byte buffer.
 * @param len   Number of bytes.
 * @return      16-bit Modbus CRC.
 */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

/**
 * CRC-32/ISO-HDLC — polynomial 0xEDB88320 (reflected 0x04C11DB7).
 * Compatible with zlib crc32() and STM32 CRC peripheral (reversed bit order).
 * @param data  Pointer to the byte buffer.
 * @param len   Number of bytes.
 * @return      32-bit CRC.
 */
uint32_t crc32_iso(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CONVERTER_CRC_H */
