#include "app_modbus.h"
#include "app_converter.h"
#include "converter_crc.h"

#include "main.h"  /* HAL_UART_Transmit, GPIO macros */

#include <string.h>

extern UART_HandleTypeDef huart3;

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

static uint8_t  g_slave_addr;
static uint8_t  g_rx_buf[APP_MODBUS_BUF_SIZE];
static uint8_t  g_rx_len;
static uint32_t g_last_rx_tick;   /* ms timestamp of last received byte */
static bool     g_frame_ready;

/* ------------------------------------------------------------------ */
/* RS-485 direction control                                            */
/* ------------------------------------------------------------------ */

#if defined(RS485_DE_GPIO_Port) && defined(RS485_DE_Pin)
#define RS485_TX_ENABLE()  HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET)
#define RS485_TX_DISABLE() HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET)
#else
#define RS485_TX_ENABLE()  do {} while (0)
#define RS485_TX_DISABLE() do {} while (0)
#endif

/* ------------------------------------------------------------------ */
/* Response builder helpers                                            */
/* ------------------------------------------------------------------ */

static void send_response(uint8_t *buf, uint8_t len) {
    uint16_t crc;
    if (len + 2u > APP_MODBUS_BUF_SIZE) return;
    crc = crc16_modbus(buf, (size_t)len);
    buf[len]     = (uint8_t)(crc & 0xFFu);
    buf[len + 1u] = (uint8_t)(crc >> 8u);
    RS485_TX_ENABLE();
    (void)HAL_UART_Transmit(&huart3, buf, (uint16_t)(len + 2u), 10u);
    RS485_TX_DISABLE();
}

static void send_exception(uint8_t fc, uint8_t exception_code) {
    uint8_t buf[5];
    buf[0] = g_slave_addr;
    buf[1] = fc | 0x80u;
    buf[2] = exception_code;
    send_response(buf, 3u);
}

/* ------------------------------------------------------------------ */
/* Register read helpers                                               */
/* ------------------------------------------------------------------ */

static uint16_t read_holding_register(uint16_t addr, uint32_t now_ms) {
    const converter_runtime_t *rt = AppConverter_GetRuntime();
    (void)now_ms;
    switch (addr) {
        case MODBUS_HR_CONTROL:
            return 0u;
        case MODBUS_HR_CURRENT_REF_MA:
            return (uint16_t)(int16_t)(rt->controller.requested_current_a * 1000.0f);
        case MODBUS_HR_CMD_TIMEOUT_MS:
            return (uint16_t)rt->runtime_config.command_timeout_ms;
        case MODBUS_HR_CURRENT_LIM_MA:
            return (uint16_t)(rt->control_config.current_limit_a * 1000.0f);
        case MODBUS_HR_CURRENT_TRIP_MA:
            return (uint16_t)(rt->control_config.current_trip_a * 1000.0f);
        case MODBUS_HR_BUS_MIN_CENTIV:
            return (uint16_t)(rt->control_config.bus_voltage_min_v * 100.0f);
        case MODBUS_HR_BUS_MAX_CENTIV:
            return (uint16_t)(rt->control_config.bus_voltage_max_v * 100.0f);
        case MODBUS_HR_TEMP_TRIP_DECI:
            return (uint16_t)(int16_t)(rt->control_config.temperature_trip_c * 10.0f);
        case MODBUS_HR_KP_1E4:
            return (uint16_t)(rt->control_config.kp * 10000.0f);
        case MODBUS_HR_KI_1E3:
            return (uint16_t)(rt->control_config.ki * 1000.0f);
        default:
            return 0xFFFFu;
    }
}

static uint16_t read_input_register(uint16_t addr, uint32_t now_ms) {
    const converter_runtime_t *rt = AppConverter_GetRuntime();
    switch (addr) {
        case MODBUS_IR_STATE:
            return (uint16_t)rt->controller.state;
        case MODBUS_IR_FAULT_LO:
            return (uint16_t)(rt->controller.faults & 0xFFFFu);
        case MODBUS_IR_FAULT_HI:
            return (uint16_t)(rt->controller.faults >> 16u);
        case MODBUS_IR_CURRENT_MA:
            return (uint16_t)(int16_t)(rt->measurement.input_current_a * 1000.0f);
        case MODBUS_IR_BUS_CENTIV:
            return (uint16_t)(rt->measurement.bus_voltage_v * 100.0f);
        case MODBUS_IR_TEMP_DECI:
            return (uint16_t)(int16_t)(rt->measurement.temperature_c * 10.0f);
        case MODBUS_IR_DUTY_1E4:
            return (uint16_t)(int16_t)(rt->controller.duty_command * 10000.0f);
        case MODBUS_IR_REQUESTED_MA:
            return (uint16_t)(int16_t)(rt->controller.requested_current_a * 1000.0f);
        case MODBUS_IR_RAMPED_MA:
            return (uint16_t)(int16_t)(rt->controller.ramped_current_a * 1000.0f);
        case MODBUS_IR_UPTIME_LO:
            return (uint16_t)(now_ms & 0xFFFFu);
        case MODBUS_IR_UPTIME_HI:
            return (uint16_t)(now_ms >> 16u);
        case MODBUS_IR_FW_VERSION:
            return APP_MODBUS_FW_VERSION;
        default:
            return 0xFFFFu;
    }
}

/* ------------------------------------------------------------------ */
/* Write dispatch                                                      */
/* ------------------------------------------------------------------ */

/** Returns true if the write was accepted, false for illegal value. */
static bool write_holding_register(uint16_t addr, uint16_t value, uint32_t now_ms) {
    const converter_runtime_t *rt = AppConverter_GetRuntime();

    /* Disallow parameter writes while converter is running. */
    if (rt->controller.state == CONVERTER_STATE_RUN) {
        switch (addr) {
            case MODBUS_HR_CURRENT_LIM_MA:
            case MODBUS_HR_CURRENT_TRIP_MA:
            case MODBUS_HR_BUS_MIN_CENTIV:
            case MODBUS_HR_BUS_MAX_CENTIV:
            case MODBUS_HR_TEMP_TRIP_DECI:
            case MODBUS_HR_KP_1E4:
            case MODBUS_HR_KI_1E3:
                return false;
            default:
                break;
        }
    }

    switch (addr) {
        case MODBUS_HR_CONTROL:
            if ((value & MODBUS_CTRL_ESTOP) != 0u) {
                AppConverter_EmergencyStop();
            } else if ((value & MODBUS_CTRL_DISARM) != 0u) {
                AppConverter_Disarm();
            } else if ((value & MODBUS_CTRL_CLEAR_FAULT) != 0u) {
                (void)AppConverter_ClearFaults();
            } else if ((value & MODBUS_CTRL_ARM) != 0u) {
                (void)AppConverter_Arm(now_ms);
            }
            break;
        case MODBUS_HR_CURRENT_REF_MA:
            AppConverter_SetCurrentMilliamp((int16_t)value, now_ms);
            break;
        /* NOTE: The remaining parameter registers (timeout, limits, PI gains)
         * would require re-initialising the runtime with new config.  This is
         * intentionally left as a placeholder — implementations must call
         * AppConverter_Disarm() first, update AppConverter parameters via the
         * AppFlashParams helpers, and then call AppConverter_Init() again.
         * This prevents in-flight parameter changes from destabilising the
         * control loop. */
        case MODBUS_HR_CMD_TIMEOUT_MS:
        case MODBUS_HR_CURRENT_LIM_MA:
        case MODBUS_HR_CURRENT_TRIP_MA:
        case MODBUS_HR_BUS_MIN_CENTIV:
        case MODBUS_HR_BUS_MAX_CENTIV:
        case MODBUS_HR_TEMP_TRIP_DECI:
        case MODBUS_HR_KP_1E4:
        case MODBUS_HR_KI_1E3:
            /* Accepted but deferred; caller must persist and reinitialise. */
            break;
        default:
            return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Frame processing                                                    */
/* ------------------------------------------------------------------ */

static void process_frame(uint32_t now_ms) {
    uint8_t  *frame = g_rx_buf;
    uint8_t   len   = g_rx_len;
    uint16_t  rx_crc;
    uint16_t  calc_crc;
    uint8_t   fc;
    uint16_t  start_addr;
    uint16_t  qty;
    uint8_t   tx_buf[APP_MODBUS_BUF_SIZE];
    uint8_t   tx_len;
    uint16_t  i;

    /* Minimum frame: addr(1) + fc(1) + data(min 2) + crc(2) = 6 bytes. */
    if (len < 6u) return;

    /* Address filter. */
    if (frame[0] != g_slave_addr) return;

    /* CRC check. */
    rx_crc   = (uint16_t)frame[len - 2u] | ((uint16_t)frame[len - 1u] << 8u);
    calc_crc = crc16_modbus(frame, (size_t)(len - 2u));
    if (rx_crc != calc_crc) return; /* silently discard corrupted frames */

    fc = frame[1];

    switch (fc) {
        /* ---- FC 03: Read Holding Registers ---- */
        case 0x03u:
            if (len != 8u) { send_exception(fc, 0x03u); return; }
            start_addr = (uint16_t)((frame[2] << 8u) | frame[3]);
            qty        = (uint16_t)((frame[4] << 8u) | frame[5]);
            if (qty == 0u || qty > 125u ||
                (start_addr + qty) > MODBUS_HR_COUNT) {
                send_exception(fc, 0x02u); return;
            }
            tx_buf[0] = g_slave_addr;
            tx_buf[1] = fc;
            tx_buf[2] = (uint8_t)(qty * 2u);
            tx_len    = 3u;
            for (i = 0u; i < qty; ++i) {
                uint16_t val = read_holding_register(start_addr + i, now_ms);
                tx_buf[tx_len++] = (uint8_t)(val >> 8u);
                tx_buf[tx_len++] = (uint8_t)(val & 0xFFu);
            }
            send_response(tx_buf, tx_len);
            break;

        /* ---- FC 04: Read Input Registers ---- */
        case 0x04u:
            if (len != 8u) { send_exception(fc, 0x03u); return; }
            start_addr = (uint16_t)((frame[2] << 8u) | frame[3]);
            qty        = (uint16_t)((frame[4] << 8u) | frame[5]);
            if (qty == 0u || qty > 125u ||
                (start_addr + qty) > MODBUS_IR_COUNT) {
                send_exception(fc, 0x02u); return;
            }
            tx_buf[0] = g_slave_addr;
            tx_buf[1] = fc;
            tx_buf[2] = (uint8_t)(qty * 2u);
            tx_len    = 3u;
            for (i = 0u; i < qty; ++i) {
                uint16_t val = read_input_register(start_addr + i, now_ms);
                tx_buf[tx_len++] = (uint8_t)(val >> 8u);
                tx_buf[tx_len++] = (uint8_t)(val & 0xFFu);
            }
            send_response(tx_buf, tx_len);
            break;

        /* ---- FC 06: Write Single Holding Register ---- */
        case 0x06u:
            if (len != 8u) { send_exception(fc, 0x03u); return; }
            start_addr = (uint16_t)((frame[2] << 8u) | frame[3]);
            {
                uint16_t val = (uint16_t)((frame[4] << 8u) | frame[5]);
                if (!write_holding_register(start_addr, val, now_ms)) {
                    send_exception(fc, 0x02u); return;
                }
            }
            /* Echo request as response. */
            send_response(frame, (uint8_t)(len - 2u));
            break;

        /* ---- FC 10: Write Multiple Holding Registers ---- */
        case 0x10u:
            if (len < 9u) { send_exception(fc, 0x03u); return; }
            start_addr = (uint16_t)((frame[2] << 8u) | frame[3]);
            qty        = (uint16_t)((frame[4] << 8u) | frame[5]);
            {
                uint8_t byte_count = frame[6];
                if (qty == 0u || qty > 123u ||
                    byte_count != (uint8_t)(qty * 2u) ||
                    (start_addr + qty) > MODBUS_HR_COUNT ||
                    len != (uint8_t)(9u + byte_count)) {
                    send_exception(fc, 0x03u); return;
                }
                for (i = 0u; i < qty; ++i) {
                    uint16_t val = (uint16_t)((frame[7u + i * 2u] << 8u) |
                                               frame[8u + i * 2u]);
                    if (!write_holding_register(start_addr + i, val, now_ms)) {
                        send_exception(fc, 0x02u); return;
                    }
                }
            }
            tx_buf[0] = g_slave_addr;
            tx_buf[1] = fc;
            tx_buf[2] = frame[2];
            tx_buf[3] = frame[3];
            tx_buf[4] = frame[4];
            tx_buf[5] = frame[5];
            send_response(tx_buf, 6u);
            break;

        default:
            send_exception(fc, 0x01u); /* illegal function */
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void AppModbus_Init(uint8_t slave_address) {
    g_slave_addr  = (slave_address >= 1u && slave_address <= 247u) ? slave_address : 1u;
    g_rx_len      = 0u;
    g_last_rx_tick = 0u;
    g_frame_ready  = false;
    RS485_TX_DISABLE();
}

void AppModbus_ByteReceived(uint8_t byte) {
    /* Reset the silence timer on every new byte. */
    g_frame_ready = false;
    if (g_rx_len < APP_MODBUS_BUF_SIZE) {
        g_rx_buf[g_rx_len++] = byte;
    } else {
        /* Overflow: discard entire buffer and restart. */
        g_rx_len = 0u;
        g_rx_buf[g_rx_len++] = byte;
    }
    /* The tick counter update must happen in AppModbus_1msTick context;
     * here we just signal "bytes are arriving". */
}

void AppModbus_1msTick(uint32_t now_ms) {
    /* Update last-byte timestamp whenever new bytes arrived since last tick.
     * g_last_rx_tick is maintained here to stay in the scheduler context. */
    static uint8_t prev_rx_len = 0u;

    if (g_rx_len != prev_rx_len) {
        g_last_rx_tick = now_ms;
        prev_rx_len    = g_rx_len;
    }

    /* Detect inter-frame gap. */
    if (g_rx_len > 0u && !g_frame_ready) {
        if ((uint32_t)(now_ms - g_last_rx_tick) >= APP_MODBUS_SILENCE_TICKS) {
            g_frame_ready = true;
        }
    }

    if (g_frame_ready && g_rx_len > 0u) {
        process_frame(now_ms);
        g_rx_len      = 0u;
        g_frame_ready = false;
        prev_rx_len   = 0u;
    }
}
