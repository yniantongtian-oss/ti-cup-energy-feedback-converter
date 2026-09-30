#!/usr/bin/env python3
"""Small dependency-free Modbus RTU frame helper.

This module does not open a serial port. It focuses on deterministic frame
construction, CRC validation and response decoding so it can be reused by
PySerial, WPF, Qt or embedded test harnesses.
"""

import argparse
import json
from dataclasses import dataclass
from typing import Iterable, Optional, Tuple


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF


def append_crc(payload: bytes) -> bytes:
    value = crc16(payload)
    return payload + bytes((value & 0xFF, value >> 8))


def validate_crc(frame: bytes) -> bool:
    return len(frame) >= 4 and crc16(frame[:-2]) == int.from_bytes(frame[-2:], "little")


def _u16(value: int, name: str) -> int:
    if not 0 <= value <= 0xFFFF:
        raise ValueError(f"{name} must fit in uint16")
    return value


def _slave(slave: int) -> int:
    if not 1 <= slave <= 247:
        raise ValueError("slave must be in 1..247")
    return slave


def read_holding_registers(slave: int, address: int, count: int) -> bytes:
    _slave(slave)
    _u16(address, "address")
    if not 1 <= count <= 125:
        raise ValueError("count must be in 1..125")
    return append_crc(bytes((slave, 0x03)) + address.to_bytes(2, "big") + count.to_bytes(2, "big"))


def read_input_registers(slave: int, address: int, count: int) -> bytes:
    _slave(slave)
    _u16(address, "address")
    if not 1 <= count <= 125:
        raise ValueError("count must be in 1..125")
    return append_crc(bytes((slave, 0x04)) + address.to_bytes(2, "big") + count.to_bytes(2, "big"))


def write_single_register(slave: int, address: int, value: int) -> bytes:
    _slave(slave)
    _u16(address, "address")
    _u16(value, "value")
    return append_crc(bytes((slave, 0x06)) + address.to_bytes(2, "big") + value.to_bytes(2, "big"))


def write_multiple_registers(slave: int, address: int, values: Iterable[int]) -> bytes:
    _slave(slave)
    _u16(address, "address")
    registers = [_u16(value, "register") for value in values]
    if not 1 <= len(registers) <= 123:
        raise ValueError("register count must be in 1..123")
    body = b"".join(value.to_bytes(2, "big") for value in registers)
    payload = (
        bytes((slave, 0x10))
        + address.to_bytes(2, "big")
        + len(registers).to_bytes(2, "big")
        + bytes((len(body),))
        + body
    )
    return append_crc(payload)


@dataclass(frozen=True)
class RegisterResponse:
    slave: int
    function: int
    registers: Tuple[int, ...]


def parse_register_response(frame: bytes, expected_slave: Optional[int] = None) -> RegisterResponse:
    if not validate_crc(frame):
        raise ValueError("invalid Modbus CRC")
    slave, function = frame[0], frame[1]
    if expected_slave is not None and slave != expected_slave:
        raise ValueError(f"unexpected slave {slave}")
    if function & 0x80:
        if len(frame) != 5:
            raise ValueError("invalid exception response length")
        raise RuntimeError(f"Modbus exception code {frame[2]}")
    if function not in (0x03, 0x04):
        raise ValueError(f"unsupported response function 0x{function:02X}")
    byte_count = frame[2]
    if byte_count % 2 or len(frame) != byte_count + 5:
        raise ValueError("invalid register byte count")
    registers = tuple(
        int.from_bytes(frame[index : index + 2], "big")
        for index in range(3, 3 + byte_count, 2)
    )
    return RegisterResponse(slave=slave, function=function, registers=registers)


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{byte:02X}" for byte in data)


def _parse_int(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build and inspect Modbus RTU frames")
    sub = parser.add_subparsers(dest="command", required=True)

    read = sub.add_parser("read", help="build function 03/04 request")
    read.add_argument("slave", type=_parse_int)
    read.add_argument("address", type=_parse_int)
    read.add_argument("count", type=_parse_int)
    read.add_argument("--input", action="store_true", help="use input registers (0x04)")

    write = sub.add_parser("write", help="build function 06 request")
    write.add_argument("slave", type=_parse_int)
    write.add_argument("address", type=_parse_int)
    write.add_argument("value", type=_parse_int)

    parse = sub.add_parser("parse", help="parse function 03/04 response")
    parse.add_argument("hex_frame", help='for example: "01 03 02 00 2A 39 9B"')

    args = parser.parse_args()
    if args.command == "read":
        frame = (
            read_input_registers(args.slave, args.address, args.count)
            if args.input
            else read_holding_registers(args.slave, args.address, args.count)
        )
        print(hex_bytes(frame))
    elif args.command == "write":
        print(hex_bytes(write_single_register(args.slave, args.address, args.value)))
    else:
        frame = bytes.fromhex(args.hex_frame)
        response = parse_register_response(frame)
        print(
            json.dumps(
                {
                    "slave": response.slave,
                    "function": response.function,
                    "registers": response.registers,
                }
            )
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
