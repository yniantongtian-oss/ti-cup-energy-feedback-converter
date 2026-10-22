"""Trip logic aligned with firmware/src/protection.c and the Modbus fault map."""

from __future__ import annotations

import math
from dataclasses import dataclass

FAULT_NONE = 0
FAULT_OVERCURRENT = 1 << 0
FAULT_OVERVOLTAGE = 1 << 1
FAULT_UNDERVOLTAGE = 1 << 2
FAULT_OVERTEMPERATURE = 1 << 3
FAULT_INVALID_SAMPLE = 1 << 4
FAULT_BAD_TIMESTEP = 1 << 5
FAULT_HARDWARE = 1 << 6
FAULT_ESTOP = 1 << 7
FAULT_COMM_TIMEOUT = 1 << 8

FAULT_BITS = {
    "OVERCURRENT": FAULT_OVERCURRENT,
    "OVERVOLTAGE": FAULT_OVERVOLTAGE,
    "UNDERVOLTAGE": FAULT_UNDERVOLTAGE,
    "OVERTEMPERATURE": FAULT_OVERTEMPERATURE,
    "INVALID_SAMPLE": FAULT_INVALID_SAMPLE,
    "BAD_TIMESTEP": FAULT_BAD_TIMESTEP,
    "HARDWARE": FAULT_HARDWARE,
    "ESTOP": FAULT_ESTOP,
    "COMM_TIMEOUT": FAULT_COMM_TIMEOUT,
}

FAULT_NAMES = {value: name.lower() for name, value in FAULT_BITS.items()}


def fault_names(mask: int) -> list[str]:
    return [name for bit, name in sorted(FAULT_NAMES.items()) if mask & bit]


@dataclass(frozen=True)
class ProtectionLimits:
    current_trip_a: float = 2.5
    bus_voltage_min_v: float = 8.0
    bus_voltage_max_v: float = 30.0
    temperature_trip_c: float = 70.0


@dataclass(frozen=True)
class Measurement:
    current_a: float
    bus_voltage_v: float
    temperature_c: float
    sample_valid: bool = True


def _finite_measurement(measurement: Measurement | None) -> bool:
    if measurement is None or not measurement.sample_valid:
        return False
    return all(
        math.isfinite(value)
        for value in (
            measurement.current_a,
            measurement.bus_voltage_v,
            measurement.temperature_c,
        )
    )


def evaluate(
    limits: ProtectionLimits | None,
    measurement: Measurement | None,
    dt_s: float,
) -> int:
    """Return the latched-style fault mask for one sample.

    Electrical trips use the threshold itself (`>=` / `<=`). An invalid sample
    suppresses electrical bits because those readings are not meaningful.
    """

    faults = FAULT_NONE
    if limits is None:
        return FAULT_INVALID_SAMPLE | FAULT_BAD_TIMESTEP
    if not math.isfinite(dt_s) or dt_s <= 0.0 or dt_s > 1.0:
        faults |= FAULT_BAD_TIMESTEP
    if not isinstance(measurement, Measurement) or not _finite_measurement(measurement):
        return faults | FAULT_INVALID_SAMPLE
    if abs(measurement.current_a) >= limits.current_trip_a:
        faults |= FAULT_OVERCURRENT
    if measurement.bus_voltage_v >= limits.bus_voltage_max_v:
        faults |= FAULT_OVERVOLTAGE
    if measurement.bus_voltage_v <= limits.bus_voltage_min_v:
        faults |= FAULT_UNDERVOLTAGE
    if measurement.temperature_c >= limits.temperature_trip_c:
        faults |= FAULT_OVERTEMPERATURE
    return faults
