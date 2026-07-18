#!/usr/bin/env python3
"""Dependency-free averaged current-loop demonstration.

Educational host simulation only; it is not a switching model and does not
contain validated hardware parameters.
"""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path


@dataclass
class PIController:
    kp: float = 0.08
    ki: float = 8.0
    duty_limit: float = 0.85
    integrator_limit: float = 0.75
    integrator: float = 0.0

    def step(self, reference: float, measured: float, dt: float) -> float:
        error = reference - measured
        candidate = max(
            -self.integrator_limit,
            min(self.integrator_limit, self.integrator + self.ki * error * dt),
        )
        raw = self.kp * error + candidate
        output = max(-self.duty_limit, min(self.duty_limit, raw))
        if output == raw or (output > 0 and error < 0) or (output < 0 and error > 0):
            self.integrator = candidate
        return output


def run(duration: float, dt: float, output: Path) -> None:
    if duration <= 0 or dt <= 0 or dt > duration:
        raise ValueError("duration and dt must be positive, and dt <= duration")

    controller = PIController()
    current = 0.0
    ramped_reference = 0.0
    requested_reference = 1.0
    slew_rate = 5.0
    plant_gain = 2.0
    plant_time_constant = 0.02

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["time_s", "reference_a", "current_a", "duty"])
        steps = math.ceil(duration / dt)
        for index in range(steps + 1):
            time_s = index * dt
            max_delta = slew_rate * dt
            delta = max(-max_delta, min(max_delta, requested_reference - ramped_reference))
            ramped_reference += delta
            duty = controller.step(ramped_reference, current, dt)
            current += ((plant_gain * duty) - current) * dt / plant_time_constant
            writer.writerow([
                f"{time_s:.6f}", f"{ramped_reference:.6f}",
                f"{current:.6f}", f"{duty:.6f}"
            ])

    print(f"wrote {steps + 1} samples to {output}")
    print(f"final current: {current:.3f} A")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=0.5)
    parser.add_argument("--dt", type=float, default=0.001)
    parser.add_argument("--output", type=Path, default=Path("simulation/output.csv"))
    args = parser.parse_args()
    run(args.duration, args.dt, args.output)


if __name__ == "__main__":
    main()
