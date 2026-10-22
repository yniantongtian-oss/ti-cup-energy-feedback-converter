"""Pole-cancellation starting point for the averaged current loop.

The plant is I(s)/D(s) = K / (tau s + 1). Cancelling the plant pole and
placing the crossover at omega_c gives kp = omega_c * tau / K and
ki = omega_c / K. The crossover is kept well below the plant pole and the
sample-rate Nyquist frequency so the forward-Euler twin stays stable.
"""

from __future__ import annotations

import math


def suggest_current_gains(
    plant_gain: float,
    plant_tau_s: float,
    dt_s: float,
    crossover_hz: float | None = None,
) -> dict[str, float]:
    if (
        not math.isfinite(plant_gain)
        or plant_gain <= 0.0
        or not math.isfinite(plant_tau_s)
        or plant_tau_s <= 0.0
        or not math.isfinite(dt_s)
        or dt_s <= 0.0
    ):
        raise ValueError("plant gain, time constant, and dt must be positive and finite")

    pole_hz = 1.0 / (math.tau * plant_tau_s)
    nyquist_hz = 0.5 / dt_s
    if crossover_hz is None:
        chosen = min(pole_hz / 5.0, nyquist_hz / 50.0)
        chosen = max(chosen, 0.5)
    else:
        if not math.isfinite(crossover_hz) or crossover_hz <= 0.0 or crossover_hz >= nyquist_hz / 2.0:
            raise ValueError("crossover_hz must be positive and below one quarter of the sample rate")
        chosen = crossover_hz

    omega = math.tau * chosen
    return {
        "kp": omega * plant_tau_s / plant_gain,
        "ki": omega / plant_gain,
        "kd": 0.0,
        "crossover_hz": chosen,
        "plant_pole_hz": pole_hz,
    }
