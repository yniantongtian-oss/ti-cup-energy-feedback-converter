from __future__ import annotations

from contextlib import redirect_stdout
from dataclasses import asdict
import io
import json
import unittest

from examples.host.converter_telemetry import build_telemetry_request, decode_telemetry, main
from tools.modbus_rtu import append_crc, validate_crc


def response(registers: list[int], function: int = 4, slave: int = 1) -> bytes:
    data = b"".join((r & 0xFFFF).to_bytes(2, "big") for r in registers)
    return append_crc(bytes([slave, function, len(data)]) + data)


class TelemetryTests(unittest.TestCase):
    def setUp(self) -> None:
        # Documented fields: negative current/temperature/duty, high fault/uptime bits.
        self.registers = [2, 0x0101, 0x0002, -1250, 4850, -55, -2500, -2000, -1500, 0xFFFF, 2, 0x0102]

    def test_request_reads_input_registers_zero_through_eleven(self) -> None:
        frame = build_telemetry_request(7)
        self.assertEqual(frame[:6], bytes.fromhex("07 04 00 00 00 0C"))
        self.assertTrue(validate_crc(frame))

    def test_complete_response_decodes_signed_values_scaling_and_word_order(self) -> None:
        telemetry = decode_telemetry(response(self.registers))
        self.assertEqual(asdict(telemetry), {
            "state": "FAULT", "fault_flags": 0x20101,
            "measured_current_a": -1.25, "bus_voltage_v": 48.5,
            "temperature_c": -5.5, "duty_command": -0.25,
            "requested_current_a": -2.0, "ramped_current_a": -1.5,
            "uptime_ms": 196607, "firmware_version": "1.2",
        })

    def test_reject_holding_registers_even_with_correct_crc_and_count(self) -> None:
        with self.assertRaises(ValueError):
            decode_telemetry(response(self.registers, function=3))

    def test_reject_truncated_or_extra_register_blocks(self) -> None:
        for registers in [self.registers[:-1], self.registers + [0]]:
            with self.subTest(registers=len(registers)), self.assertRaises(ValueError):
                decode_telemetry(response(registers))

    def test_reject_wrong_slave_bad_crc_and_exception_frames(self) -> None:
        with self.assertRaises(ValueError):
            decode_telemetry(response(self.registers, slave=2))
        bad_crc = bytearray(response(self.registers))
        bad_crc[-1] ^= 1
        with self.assertRaises(ValueError):
            decode_telemetry(bytes(bad_crc))
        with self.assertRaises(RuntimeError):
            decode_telemetry(append_crc(bytes([1, 0x84, 2])))

    def test_reject_unknown_state_and_invalid_duty(self) -> None:
        for index, value in [(0, 3), (6, 10001), (6, -10001)]:
            data = self.registers.copy()
            data[index] = value
            with self.subTest(index=index, value=value), self.assertRaises(ValueError):
                decode_telemetry(response(data))

    def test_cli_json_has_telemetry_units(self) -> None:
        output = io.StringIO()
        with redirect_stdout(output):
            result = main(["decode", response(self.registers).hex()])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(output.getvalue())["measured_current_a"], -1.25)


if __name__ == "__main__":
    unittest.main()
