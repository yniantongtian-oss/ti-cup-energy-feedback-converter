#!/usr/bin/env python3
"""Build read-only requests and decode the documented converter telemetry block.

This is a frame/units adapter. It does not connect to hardware, send frames,
implement the STM32 Modbus slave, or arm the power stage.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.modbus_rtu import hex_bytes, parse_register_response, read_input_registers  # noqa: E402

INPUT_REGISTER_COUNT = 12
STATE_NAMES = {0: "IDLE", 1: "RUN", 2: "FAULT"}


def signed_u16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


@dataclass(frozen=True)
class ConverterTelemetry:
    state: str
    fault_flags: int
    measured_current_a: float
    bus_voltage_v: float
    temperature_c: float
    duty_command: float
    requested_current_a: float
    ramped_current_a: float
    uptime_ms: int
    firmware_version: str


def build_telemetry_request(slave: int = 1) -> bytes:
    """Read the complete input-register block at addresses 0..11."""
    return read_input_registers(slave, address=0, count=INPUT_REGISTER_COUNT)


def decode_telemetry(frame: bytes, expected_slave: int = 1) -> ConverterTelemetry:
    response = parse_register_response(frame, expected_slave=expected_slave)
    if response.function != 0x04:
        raise ValueError("Converter telemetry must use input registers (function 0x04)")
    registers = response.registers
    if len(registers) != INPUT_REGISTER_COUNT:
        raise ValueError("Converter telemetry requires all 12 documented input registers")
    if registers[0] not in STATE_NAMES:
        raise ValueError(f"Unknown converter state: {registers[0]}")
    duty = signed_u16(registers[6]) / 10000.0
    if not -1.0 <= duty <= 1.0:
        raise ValueError("Duty command is outside the documented -1..1 range")
    version = registers[11]
    return ConverterTelemetry(
        state=STATE_NAMES[registers[0]],
        fault_flags=registers[1] | registers[2] << 16,
        measured_current_a=signed_u16(registers[3]) / 1000.0,
        bus_voltage_v=registers[4] / 100.0,
        temperature_c=signed_u16(registers[5]) / 10.0,
        duty_command=duty,
        requested_current_a=signed_u16(registers[7]) / 1000.0,
        ramped_current_a=signed_u16(registers[8]) / 1000.0,
        uptime_ms=registers[9] | registers[10] << 16,
        firmware_version=f"{version >> 8}.{version & 0xFF}",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    request = sub.add_parser("request", help="print a function-04 read request; sends nothing")
    request.add_argument("--slave", type=int, default=1)
    decode = sub.add_parser("decode", help="decode a complete 12-register response")
    decode.add_argument("hex_frame")
    decode.add_argument("--slave", type=int, default=1)
    args = parser.parse_args(argv)
    try:
        if args.command == "request":
            print(hex_bytes(build_telemetry_request(args.slave)))
        else:
            print(json.dumps(asdict(decode_telemetry(bytes.fromhex(args.hex_frame), args.slave)), indent=2))
    except (ValueError, RuntimeError) as exc:
        parser.exit(2, f"error: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
