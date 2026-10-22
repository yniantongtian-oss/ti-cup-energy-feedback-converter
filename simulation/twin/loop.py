"""Closed-loop averaged twin of the portable current controller."""

from __future__ import annotations

import math
from dataclasses import dataclass, replace

from simulation.twin.pid import PIDController, PIDGains
from simulation.twin.plant import FirstOrderPlant
from simulation.twin.protection import (
    FAULT_NONE,
    Measurement,
    ProtectionLimits,
    evaluate,
)

MAX_SAMPLES = 1_000_000


def sample_times(duration_s: float, dt_s: float) -> tuple[float, ...]:
    """Sample instants from 0 through the last step that does not pass duration."""

    if not math.isfinite(duration_s) or not math.isfinite(dt_s):
        raise ValueError("duration and dt must be finite")
    if duration_s <= 0.0 or dt_s <= 0.0:
        raise ValueError("duration and dt must be positive")
    if dt_s > duration_s:
        raise ValueError("dt must be <= duration")

    times: list[float] = []
    index = 0
    while True:
        time_s = index * dt_s
        if time_s > duration_s + 1e-9:
            break
        times.append(time_s)
        index += 1
        if index > MAX_SAMPLES:
            raise ValueError(f"simulation exceeds {MAX_SAMPLES} samples; increase dt or shorten duration")
    if not times:
        raise ValueError("duration and dt produced no samples")
    return tuple(times)


@dataclass(frozen=True)
class LoopConfig:
    kp: float = 0.08
    ki: float = 8.0
    kd: float = 0.0
    duty_limit: float = 0.85
    integrator_limit: float = 0.75
    derivative_filter_hz: float = 0.0
    current_limit_a: float = 2.0
    current_trip_a: float = 2.5
    bus_voltage_min_v: float = 8.0
    bus_voltage_max_v: float = 30.0
    temperature_trip_c: float = 70.0
    slew_a_per_s: float = 5.0
    plant_gain: float = 2.0
    plant_tau_s: float = 0.02
    bus_voltage_v: float = 12.0
    temperature_c: float = 25.0
    reference_a: float = 1.0
    duration_s: float = 0.5
    dt_s: float = 0.001

    def validated(self) -> LoopConfig:
        gains = PIDGains(
            kp=self.kp,
            ki=self.ki,
            kd=self.kd,
            output_limit=self.duty_limit,
            integrator_limit=self.integrator_limit,
            derivative_filter_hz=self.derivative_filter_hz,
        )
        if not gains.valid():
            raise ValueError("PID gains are invalid")
        numeric = (
            self.current_limit_a,
            self.current_trip_a,
            self.bus_voltage_min_v,
            self.bus_voltage_max_v,
            self.temperature_trip_c,
            self.slew_a_per_s,
            self.plant_gain,
            self.plant_tau_s,
            self.bus_voltage_v,
            self.temperature_c,
            self.reference_a,
        )
        if not all(math.isfinite(value) for value in numeric):
            raise ValueError("loop configuration contains a non-finite value")
        if self.current_limit_a <= 0.0 or self.current_trip_a <= self.current_limit_a:
            raise ValueError("current trip must be greater than the current limit")
        if self.bus_voltage_min_v < 0.0 or self.bus_voltage_max_v <= self.bus_voltage_min_v:
            raise ValueError("bus voltage window is invalid")
        if self.temperature_trip_c <= 0.0 or self.slew_a_per_s <= 0.0:
            raise ValueError("temperature trip and slew rate must be positive")
        if self.plant_gain == 0.0 or self.plant_tau_s <= 0.0:
            raise ValueError("plant gain and time constant must be usable")
        sample_times(self.duration_s, self.dt_s)
        return self


@dataclass(frozen=True)
class LoopSample:
    time_s: float
    reference_a: float
    current_a: float
    duty: float
    faults: int


@dataclass(frozen=True)
class LoopMetrics:
    overshoot_pct: float | None
    settling_time_s: float | None
    steady_state_error_a: float | None
    peak_abs_duty: float
    final_current_a: float
    final_faults: int
    stable: bool


@dataclass(frozen=True)
class LoopResult:
    samples: tuple[LoopSample, ...]
    metrics: LoopMetrics
    target_a: float


def _clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def _metrics(samples: tuple[LoopSample, ...], target_a: float, duty_limit: float) -> LoopMetrics:
    currents = [sample.current_a for sample in samples]
    times = [sample.time_s for sample in samples]
    duties = [sample.duty for sample in samples]
    final_faults = samples[-1].faults
    finite = all(math.isfinite(value) for value in currents + duties + [target_a])
    peak_abs_duty = max(abs(duty) for duty in duties) if duties else 0.0
    final_current = currents[-1]
    if not finite:
        return LoopMetrics(None, None, None, peak_abs_duty, final_current, final_faults, False)

    if target_a == 0.0:
        overshoot = max(0.0, max(abs(current) for current in currents) * 100.0)
    elif target_a > 0.0:
        overshoot = max(0.0, (max(currents) - target_a) / abs(target_a) * 100.0)
    else:
        overshoot = max(0.0, (target_a - min(currents)) / abs(target_a) * 100.0)

    band = max(0.02, 0.02 * abs(target_a))
    last_outside = None
    for time_s, current in zip(times, currents):
        if abs(current - target_a) > band:
            last_outside = time_s
    settling = None
    if last_outside is None:
        settling = 0.0
    else:
        for time_s in times:
            if time_s > last_outside:
                settling = time_s
                break
    tail_count = max(1, len(currents) // 10)
    steady_error = sum(currents[-tail_count:]) / tail_count - target_a
    stable = (
        final_faults == FAULT_NONE
        and settling is not None
        and abs(steady_error) <= band
        and peak_abs_duty <= duty_limit + 1e-6
    )
    return LoopMetrics(
        overshoot_pct=overshoot,
        settling_time_s=settling,
        steady_state_error_a=steady_error,
        peak_abs_duty=peak_abs_duty,
        final_current_a=final_current,
        final_faults=final_faults,
        stable=stable,
    )


def run_current_loop(config: LoopConfig | None = None) -> LoopResult:
    """Run one armed current-loop trial from a healthy sample.

    The controller starts armed, matching converter_demo. Protection sees the
    plant current before the new duty is applied.
    """

    selected = (config if config is not None else LoopConfig()).validated()
    gains = PIDGains(
        kp=selected.kp,
        ki=selected.ki,
        kd=selected.kd,
        output_limit=selected.duty_limit,
        integrator_limit=selected.integrator_limit,
        derivative_filter_hz=selected.derivative_filter_hz,
    )
    controller = PIDController(gains)
    plant = FirstOrderPlant(gain=selected.plant_gain, time_constant_s=selected.plant_tau_s)
    limits = ProtectionLimits(
        current_trip_a=selected.current_trip_a,
        bus_voltage_min_v=selected.bus_voltage_min_v,
        bus_voltage_max_v=selected.bus_voltage_max_v,
        temperature_trip_c=selected.temperature_trip_c,
    )
    target = _clamp(selected.reference_a, -selected.current_limit_a, selected.current_limit_a)
    ramped = 0.0
    faults = FAULT_NONE
    samples: list[LoopSample] = []

    for time_s in sample_times(selected.duration_s, selected.dt_s):
        observed = Measurement(
            current_a=plant.current_a,
            bus_voltage_v=selected.bus_voltage_v,
            temperature_c=selected.temperature_c,
        )
        faults |= evaluate(limits, observed, selected.dt_s)
        if faults != FAULT_NONE:
            duty = 0.0
            ramped = 0.0
            controller.reset()
        else:
            max_delta = selected.slew_a_per_s * selected.dt_s
            ramped += _clamp(target - ramped, -max_delta, max_delta)
            duty = controller.step(ramped, plant.current_a, selected.dt_s)
        current = plant.step(duty, selected.dt_s)
        samples.append(
            LoopSample(
                time_s=time_s,
                reference_a=ramped,
                current_a=current,
                duty=duty,
                faults=faults,
            )
        )
    result_samples = tuple(samples)
    return LoopResult(
        samples=result_samples,
        metrics=_metrics(result_samples, target, selected.duty_limit),
        target_a=target,
    )


def with_gains(config: LoopConfig, kp: float, ki: float, kd: float | None = None) -> LoopConfig:
    return replace(config, kp=kp, ki=ki, kd=config.kd if kd is None else kd)
