import re
import unittest
from pathlib import Path

from simulation.twin.protection import (
    FAULT_BAD_TIMESTEP,
    FAULT_BITS,
    FAULT_ESTOP,
    FAULT_INVALID_SAMPLE,
    FAULT_OVERCURRENT,
    FAULT_OVERTEMPERATURE,
    FAULT_OVERVOLTAGE,
    FAULT_UNDERVOLTAGE,
    Measurement,
    ProtectionLimits,
    evaluate,
    fault_names,
)

ROOT = Path(__file__).resolve().parents[1]
HEADER_BITS = re.compile(r"CONVERTER_FAULT_([A-Z0-9_]+)\s*=\s*(?:0|1u\s*<<\s*(\d+))")
MAP_ROWS = (
    (0, "过流"),
    (1, "母线过压"),
    (2, "母线欠压"),
    (3, "过温"),
    (4, "无效采样/ADC 越界"),
    (5, "控制周期异常"),
    (6, "硬件故障输入"),
    (7, "急停"),
    (8, "通信超时"),
)


def header_fault_bits() -> dict[str, int]:
    text = (ROOT / "firmware/include/converter.h").read_text(encoding="utf-8")
    body = text.split("typedef enum {", 2)[2].split("} converter_fault_t;", 1)[0]
    found = {}
    for name, shift in HEADER_BITS.findall(body):
        found[name] = 0 if not shift else 1 << int(shift)
    return found


class ProtectionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.limits = ProtectionLimits()
        self.safe = Measurement(0.0, 12.0, 25.0)

    def test_bits_match_the_header_and_register_map(self) -> None:
        header = header_fault_bits()
        self.assertEqual(header["NONE"], 0)
        for name, value in FAULT_BITS.items():
            self.assertEqual(header[name], value, name)
        section = (ROOT / "docs/MODBUS_REGISTER_MAP.md").read_text(encoding="utf-8")
        section = section.split("## 故障位建议", 1)[1].split("## ", 1)[0]
        for bit, label in MAP_ROWS:
            self.assertIn(f"| {bit} | {label} |", section)

    def test_thresholds_are_inclusive(self) -> None:
        self.assertEqual(evaluate(self.limits, self.safe, 0.001), 0)
        self.assertEqual(evaluate(self.limits, self.safe, 1.0), 0)
        tripped = Measurement(self.limits.current_trip_a, self.limits.bus_voltage_max_v, self.limits.temperature_trip_c)
        faults = evaluate(self.limits, tripped, 0.001)
        self.assertTrue(faults & FAULT_OVERCURRENT)
        self.assertTrue(faults & FAULT_OVERVOLTAGE)
        self.assertTrue(faults & FAULT_OVERTEMPERATURE)
        self.assertFalse(
            evaluate(
                self.limits,
                Measurement(-self.limits.current_trip_a + 0.01, self.limits.bus_voltage_min_v + 0.01, 25.0),
                0.001,
            )
        )
        self.assertTrue(
            evaluate(self.limits, Measurement(0.0, self.limits.bus_voltage_min_v, 25.0), 0.001)
            & FAULT_UNDERVOLTAGE
        )

    def test_invalid_sample_hides_electrical_bits(self) -> None:
        measurement = Measurement(100.0, 100.0, 200.0, sample_valid=False)
        self.assertEqual(
            evaluate(self.limits, measurement, -1.0),
            FAULT_INVALID_SAMPLE | FAULT_BAD_TIMESTEP,
        )
        self.assertEqual(evaluate(self.limits, None, 0.001), FAULT_INVALID_SAMPLE)
        self.assertEqual(evaluate(None, self.safe, 0.001), FAULT_INVALID_SAMPLE | FAULT_BAD_TIMESTEP)
        self.assertEqual(
            fault_names(FAULT_OVERCURRENT | FAULT_ESTOP),
            ["overcurrent", "estop"],
        )


if __name__ == "__main__":
    unittest.main()
