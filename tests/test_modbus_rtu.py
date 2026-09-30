import unittest

from tools.modbus_rtu import (
    append_crc,
    crc16,
    parse_register_response,
    read_holding_registers,
    validate_crc,
    write_multiple_registers,
    write_single_register,
)


class ModbusRtuTests(unittest.TestCase):
    def test_known_crc_vector(self):
        payload = bytes.fromhex("01 03 00 00 00 0A")
        self.assertEqual(crc16(payload), 0xCDC5)
        self.assertEqual(append_crc(payload), bytes.fromhex("01 03 00 00 00 0A C5 CD"))

    def test_read_holding_request(self):
        frame = read_holding_registers(1, 0, 10)
        self.assertEqual(frame, bytes.fromhex("01 03 00 00 00 0A C5 CD"))
        self.assertTrue(validate_crc(frame))

    def test_write_single_register(self):
        frame = write_single_register(1, 1, 1000)
        self.assertTrue(validate_crc(frame))
        self.assertEqual(frame[:6], bytes.fromhex("01 06 00 01 03 E8"))

    def test_write_multiple_registers(self):
        frame = write_multiple_registers(2, 4, [1, 2, 0xFFFF])
        self.assertTrue(validate_crc(frame))
        self.assertEqual(frame[0:7], bytes.fromhex("02 10 00 04 00 03 06"))
        self.assertEqual(frame[7:-2], bytes.fromhex("00 01 00 02 FF FF"))

    def test_parse_register_response(self):
        payload = bytes.fromhex("01 04 06 00 2A FF 9C 04 D2")
        response = parse_register_response(append_crc(payload), expected_slave=1)
        self.assertEqual(response.slave, 1)
        self.assertEqual(response.function, 4)
        self.assertEqual(response.registers, (42, 0xFF9C, 1234))

    def test_reject_bad_crc(self):
        with self.assertRaises(ValueError):
            parse_register_response(bytes.fromhex("01 03 02 00 2A 00 00"))

    def test_reject_invalid_limits(self):
        with self.assertRaises(ValueError):
            read_holding_registers(0, 0, 1)
        with self.assertRaises(ValueError):
            read_holding_registers(1, 0, 126)
        with self.assertRaises(ValueError):
            write_multiple_registers(1, 0, [])


if __name__ == "__main__":
    unittest.main()
