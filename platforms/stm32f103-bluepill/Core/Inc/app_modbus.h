#ifndef APP_MODBUS_H
#define APP_MODBUS_H

/**
 * @file app_modbus.h
 * @brief Modbus RTU slave driver for STM32F103C8T6 + MAX485.
 *
 * ## Supported function codes
 *   FC 03 – Read Holding Registers
 *   FC 04 – Read Input Registers
 *   FC 06 – Write Single Holding Register
 *   FC 10 – Write Multiple Holding Registers
 *
 * ## Integration
 * 1. Call AppModbus_Init() after USART and GPIO are initialised.
 * 2. In the USART3 IRQ handler (or in HAL_UART_RxCpltCallback for single-byte
 *    DMA), call AppModbus_ByteReceived() for every received byte.
 * 3. Call AppModbus_1msTick() from the 1 ms scheduler.  It detects the
 *    inter-frame gap (3.5 character times) and processes complete frames.
 *
 * ## Register map
 * See docs/MODBUS_REGISTER_MAP.md for the full table.
 */

#include "converter_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Compile-time constants                                              */
/* ------------------------------------------------------------------ */

/** Maximum ADU size: 1 address + 1 FC + 252 data + 2 CRC = 256 bytes. */
#define APP_MODBUS_BUF_SIZE (256u)

/**
 * Inter-frame silence = 3.5 character times.
 * At 9600 bps, 8N1: 1 char = ~1.042 ms → 3.5 chars ≈ 3.65 ms.
 * We use 4 ms ticks to be safe.
 */
#define APP_MODBUS_SILENCE_TICKS (4u)

/* Holding register addresses (FC 03/06/10). */
#define MODBUS_HR_CONTROL         (0u)  /**< R/W: control bits (arm/disarm/clear/estop) */
#define MODBUS_HR_CURRENT_REF_MA  (1u)  /**< R/W: current reference in mA (int16) */
/* HR 2–9: read via FC 03; writes return exception 0x04.  Use AppFlashParams
 * to persist parameters and reload them on the next power-on. */
#define MODBUS_HR_CMD_TIMEOUT_MS  (2u)  /**< R (read-only over Modbus): command timeout */
#define MODBUS_HR_CURRENT_LIM_MA  (3u)  /**< R: software current limit in mA */
#define MODBUS_HR_CURRENT_TRIP_MA (4u)  /**< R: hardware trip threshold in mA (> limit) */
#define MODBUS_HR_BUS_MIN_CENTIV  (5u)  /**< R: undervoltage threshold in 0.01 V units */
#define MODBUS_HR_BUS_MAX_CENTIV  (6u)  /**< R: overvoltage threshold in 0.01 V units */
#define MODBUS_HR_TEMP_TRIP_DECI  (7u)  /**< R: overtemperature threshold in 0.1 degC */
#define MODBUS_HR_KP_1E4          (8u)  /**< R: proportional gain * 10000 */
#define MODBUS_HR_KI_1E3          (9u)  /**< R: integral gain * 1000 */
#define MODBUS_HR_COUNT           (10u)

/* Control register bit definitions (HR 0). */
#define MODBUS_CTRL_ARM           (1u << 0)
#define MODBUS_CTRL_DISARM        (1u << 1)
#define MODBUS_CTRL_CLEAR_FAULT   (1u << 2)
#define MODBUS_CTRL_ESTOP         (1u << 3)

/* Input register addresses (FC 04). */
#define MODBUS_IR_STATE           (0u)
#define MODBUS_IR_FAULT_LO        (1u)
#define MODBUS_IR_FAULT_HI        (2u)
#define MODBUS_IR_CURRENT_MA      (3u)
#define MODBUS_IR_BUS_CENTIV      (4u)
#define MODBUS_IR_TEMP_DECI       (5u)
#define MODBUS_IR_DUTY_1E4        (6u)
#define MODBUS_IR_REQUESTED_MA    (7u)
#define MODBUS_IR_RAMPED_MA       (8u)
#define MODBUS_IR_UPTIME_LO       (9u)
#define MODBUS_IR_UPTIME_HI       (10u)
#define MODBUS_IR_FW_VERSION      (11u)
#define MODBUS_IR_COUNT           (12u)

/** Firmware version reported in IR 11 (BCD: major*256 + minor). */
#ifndef APP_MODBUS_FW_VERSION
#define APP_MODBUS_FW_VERSION (0x0100u)
#endif

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * Initialise the Modbus slave.
 * @param slave_address  Modbus slave address (1–247).
 */
void AppModbus_Init(uint8_t slave_address);

/**
 * Feed one received byte into the frame assembler.
 * Call from USART3 RX interrupt (or equivalent).
 * @param byte  Received byte.
 */
void AppModbus_ByteReceived(uint8_t byte);

/**
 * 1 ms tick — drives the inter-frame timer and dispatches complete frames.
 * Must be called from the same task context as AppConverter_1msTask()
 * (or with appropriate locking if used from a different context).
 * @param now_ms  Current millisecond counter (same epoch as AppConverter).
 */
void AppModbus_1msTick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_H */
