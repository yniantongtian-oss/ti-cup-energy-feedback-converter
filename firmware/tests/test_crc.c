#include "converter_crc.h"

#include <assert.h>
#include <stdio.h>

/* Known-good CRC-16/IBM (Modbus) vectors from the Modbus specification. */
static void test_crc16_known_vectors(void) {
    /* "01 04 00 00 00 01" -> CRC = 0xCA31 */
    const uint8_t frame1[] = {0x01u, 0x04u, 0x00u, 0x00u, 0x00u, 0x01u};
    assert(crc16_modbus(frame1, sizeof(frame1)) == 0xCA31u);

    /* NULL / zero-length input gives the initial value 0xFFFF. */
    assert(crc16_modbus(NULL, 0u) == 0xFFFFu);
    assert(crc16_modbus(frame1, 0u) == 0xFFFFu);

    /* Single zero byte. */
    const uint8_t zero[] = {0x00u};
    assert(crc16_modbus(zero, 1u) == 0x40BFu);

    /* "01 03 00 00 00 06" -> CRC = 0xC8C5 */
    const uint8_t frame2[] = {0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x06u};
    assert(crc16_modbus(frame2, sizeof(frame2)) == 0xC8C5u);

    /* "01 06 00 01 00 0A" -> CRC = 0x0D58 */
    const uint8_t frame3[] = {0x01u, 0x06u, 0x00u, 0x01u, 0x00u, 0x0Au};
    assert(crc16_modbus(frame3, sizeof(frame3)) == 0x0D58u);
}

/* Known-good CRC-32/ISO-HDLC vectors (zlib compatible). */
static void test_crc32_known_vectors(void) {
    /* CRC32("123456789") == 0xCBF43926 */
    const uint8_t digits[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    assert(crc32_iso(digits, sizeof(digits)) == 0xCBF43926uL);

    /* Empty buffer gives the finalised value 0x00000000. */
    assert(crc32_iso(NULL, 0u) == 0x00000000uL);
}

static void test_crc16_incremental(void) {
    /* Verify the known value of "01 06 00 01 00 0A" is stable across calls. */
    const uint8_t data[] = {0x01u, 0x06u, 0x00u, 0x01u, 0x00u, 0x0Au};
    uint16_t first = crc16_modbus(data, sizeof(data));
    uint16_t second = crc16_modbus(data, sizeof(data));
    assert(first == 0x0D58u);
    assert(first == second);
}

static void test_crc32_incremental(void) {
    const uint8_t data[] = {'a', 'b', 'c', 'd', 'e', 'f'};
    uint32_t full = crc32_iso(data, sizeof(data));
    uint32_t again = crc32_iso(data, sizeof(data));
    assert(full == again);
    (void)full;
}

int main(void) {
    test_crc16_known_vectors();
    test_crc32_known_vectors();
    test_crc16_incremental();
    test_crc32_incremental();
    puts("all CRC tests passed");
    return 0;
}
