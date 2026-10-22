"""Averaged first-order plant used by the host current-loop twin.

This is not a switching model and it does not contain measured hardware
parameters. di/dt = (gain * duty - current) / tau.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class FirstOrderPlant:
    gain: float = 2.0
    time_constant_s: float = 0.02
    current_a: float = 0.0

    def step(self, duty: float, dt_s: float) -> float:
        if self.time_constant_s <= 0.0 or dt_s <= 0.0:
            raise ValueError("plant time constant and dt must be positive")
        self.current_a += ((self.gain * duty) - self.current_a) * dt_s / self.time_constant_s
        return self.current_a
