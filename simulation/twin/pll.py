"""Single-phase SOGI-PLL matching firmware/src/pll.c."""

from __future__ import annotations

import math
from dataclasses import dataclass


def _clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


@dataclass(frozen=True)
class PllConfig:
    nominal_hz: float = 50.0
    kp: float = 40.0
    ki: float = 800.0
    sogi_k: float = math.sqrt(2.0)
    frequency_limit_hz: float = 12.0
    lock_vq_ratio: float = 0.2
    lock_samples: int = 200

    def valid(self) -> bool:
        return (
            math.isfinite(self.nominal_hz)
            and 0.0 < self.nominal_hz < 10000.0
            and math.isfinite(self.kp)
            and self.kp >= 0.0
            and math.isfinite(self.ki)
            and self.ki >= 0.0
            and math.isfinite(self.sogi_k)
            and self.sogi_k > 0.0
            and math.isfinite(self.frequency_limit_hz)
            and self.frequency_limit_hz > 0.0
            and math.isfinite(self.lock_vq_ratio)
            and 0.0 < self.lock_vq_ratio <= 1.0
            and self.lock_samples > 0
        )


@dataclass
class PllState:
    alpha: float = 0.0
    beta: float = 0.0
    theta_rad: float = 0.0
    frequency_hz: float = 50.0
    integrator: float = 0.0
    vd: float = 0.0
    vq: float = 0.0
    aligned_samples: int = 0
    locked: bool = False


def reset(state: PllState, config: PllConfig) -> None:
    state.alpha = 0.0
    state.beta = 0.0
    state.theta_rad = 0.0
    state.frequency_hz = config.nominal_hz if math.isfinite(config.nominal_hz) else 0.0
    state.integrator = 0.0
    state.vd = 0.0
    state.vq = 0.0
    state.aligned_samples = 0
    state.locked = False


def step(state: PllState, config: PllConfig, voltage: float, dt_s: float) -> bool:
    if (
        not config.valid()
        or not math.isfinite(voltage)
        or not math.isfinite(dt_s)
        or dt_s <= 0.0
    ):
        return False

    omega = math.tau * state.frequency_hz
    error = voltage - state.alpha
    alpha_next = state.alpha + dt_s * omega * (config.sogi_k * error - state.beta)
    beta_next = state.beta + dt_s * omega * alpha_next
    if not math.isfinite(alpha_next) or not math.isfinite(beta_next):
        reset(state, config)
        return False
    state.alpha = alpha_next
    state.beta = beta_next

    cosine = math.cos(state.theta_rad)
    sine = math.sin(state.theta_rad)
    state.vd = state.alpha * cosine + state.beta * sine
    state.vq = -state.alpha * sine + state.beta * cosine

    limit = math.tau * config.frequency_limit_hz
    state.integrator = _clamp(state.integrator + config.ki * state.vq * dt_s, -limit, limit)
    correction = config.kp * state.vq + state.integrator
    frequency_hz = _clamp(
        config.nominal_hz + correction / math.tau,
        config.nominal_hz - config.frequency_limit_hz,
        config.nominal_hz + config.frequency_limit_hz,
    )
    state.frequency_hz = frequency_hz
    state.theta_rad = math.fmod(state.theta_rad + math.tau * frequency_hz * dt_s, math.tau)
    if state.theta_rad < 0.0:
        state.theta_rad += math.tau

    amplitude = math.hypot(state.vd, state.vq)
    aligned = amplitude > 0.5 and abs(state.vq) <= config.lock_vq_ratio * amplitude
    if aligned:
        state.aligned_samples += 1
    else:
        state.aligned_samples = 0
    state.locked = state.aligned_samples >= config.lock_samples
    return True


@dataclass(frozen=True)
class PllResult:
    time_s: tuple[float, ...]
    frequency_hz: tuple[float, ...]
    vq: tuple[float, ...]
    locked: bool
    mean_frequency_hz: float
    final_vq: float


def run_pll(
    *,
    frequency_hz: float = 50.0,
    duration_s: float = 0.4,
    dt_s: float = 1.0e-4,
    config: PllConfig | None = None,
    amplitude: float = 1.0,
) -> PllResult:
    from simulation.twin.loop import sample_times

    if not math.isfinite(frequency_hz) or frequency_hz <= 0.0:
        raise ValueError("frequency_hz must be a positive finite number")
    if not math.isfinite(amplitude) or amplitude <= 0.0:
        raise ValueError("amplitude must be a positive finite number")
    pll_config = config if config is not None else PllConfig()
    if not pll_config.valid():
        raise ValueError("PLL configuration is invalid")

    state = PllState(frequency_hz=pll_config.nominal_hz)
    times: list[float] = []
    frequencies: list[float] = []
    quadrature: list[float] = []
    for time_s in sample_times(duration_s, dt_s):
        voltage = amplitude * math.cos(math.tau * frequency_hz * time_s)
        if not step(state, pll_config, voltage, dt_s):
            raise ValueError("PLL step rejected a finite sample")
        times.append(time_s)
        frequencies.append(state.frequency_hz)
        quadrature.append(state.vq)
    tail = frequencies[max(0, len(frequencies) - max(1, len(frequencies) // 5)) :]
    return PllResult(
        time_s=tuple(times),
        frequency_hz=tuple(frequencies),
        vq=tuple(quadrature),
        locked=state.locked,
        mean_frequency_hz=sum(tail) / len(tail),
        final_vq=quadrature[-1],
    )
