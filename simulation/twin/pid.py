"""Discrete PID that follows firmware/src/pid.c.

The derivative term is taken on the measurement. The integrator is frozen
when the unsaturated output is beyond the stop and the error would push
further into that stop.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


def _clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


@dataclass
class PIDGains:
    kp: float = 0.08
    ki: float = 8.0
    kd: float = 0.0
    output_limit: float = 0.85
    integrator_limit: float = 0.75
    derivative_filter_hz: float = 0.0

    def valid(self) -> bool:
        values = (
            self.kp,
            self.ki,
            self.kd,
            self.output_limit,
            self.integrator_limit,
            self.derivative_filter_hz,
        )
        return (
            all(math.isfinite(value) for value in values)
            and self.kp >= 0.0
            and self.ki >= 0.0
            and self.kd >= 0.0
            and self.output_limit > 0.0
            and self.integrator_limit >= 0.0
            and self.derivative_filter_hz >= 0.0
        )


class PIDController:
    def __init__(self, gains: PIDGains | None = None) -> None:
        self.gains = gains if gains is not None else PIDGains()
        self.integrator = 0.0
        self.derivative = 0.0
        self.previous_measurement = 0.0
        self.initialized = False

    def reset(self) -> None:
        self.integrator = 0.0
        self.derivative = 0.0
        self.previous_measurement = 0.0
        self.initialized = False

    def step(self, reference: float, measurement: float, dt: float) -> float:
        gains = self.gains
        if (
            not gains.valid()
            or not math.isfinite(reference)
            or not math.isfinite(measurement)
            or not math.isfinite(dt)
            or dt <= 0.0
        ):
            return 0.0

        error = reference - measurement
        candidate = _clamp(
            self.integrator + gains.ki * error * dt,
            -gains.integrator_limit,
            gains.integrator_limit,
        )
        derivative = 0.0
        if self.initialized:
            raw_derivative = (measurement - self.previous_measurement) / dt
            if gains.derivative_filter_hz > 0.0:
                tau = 1.0 / (math.tau * gains.derivative_filter_hz)
                alpha = dt / (tau + dt)
                self.derivative += alpha * (raw_derivative - self.derivative)
                derivative = self.derivative
            else:
                self.derivative = raw_derivative
                derivative = raw_derivative
        else:
            self.derivative = 0.0
        self.previous_measurement = measurement
        self.initialized = True

        unsaturated = gains.kp * error + candidate - gains.kd * derivative
        output = _clamp(unsaturated, -gains.output_limit, gains.output_limit)
        saturated_high = unsaturated > gains.output_limit
        saturated_low = unsaturated < -gains.output_limit
        winding_up = (saturated_high and error > 0.0) or (saturated_low and error < 0.0)
        if not winding_up:
            self.integrator = candidate
        return output
