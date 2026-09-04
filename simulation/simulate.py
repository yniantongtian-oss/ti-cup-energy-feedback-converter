#!/usr/bin/env python3
"""Averaged current-loop sim: U1 feedforward (A) + control-rate compare (B).

Educational host model only. Not a switching model.

Duty (A):
    duty = (u_i + U1_meas) / Vbus   if feedforward enabled (default)
Plant:
    L * di/dt = duty * Vbus - U1_true - R * i

Step B evidence: plant integrates fine; controller (and U1 measurement used for
feedforward) updates only every control_dt. Stale U1 at 1 ms fails 100 Hz
disturbance; ~50 us tracks.
"""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path


@dataclass
class PIController:
    kp: float = 2.0
    ki: float = 200.0
    voltage_limit: float = 20.0
    integrator_limit: float = 15.0
    integrator: float = 0.0

    def step(self, reference: float, measured: float, dt: float) -> float:
        error = reference - measured
        candidate = max(
            -self.integrator_limit,
            min(self.integrator_limit, self.integrator + self.ki * error * dt),
        )
        raw = self.kp * error + candidate
        output = max(-self.voltage_limit, min(self.voltage_limit, raw))
        if output == raw or (output > 0 and error < 0) or (output < 0 and error > 0):
            self.integrator = candidate
        return output


def u1_at(time_s: float, amplitude: float, freq_hz: float) -> float:
    return amplitude * math.sin(2.0 * math.pi * freq_hz * time_s)


def run(
    duration: float,
    plant_dt: float,
    control_dt: float,
    output: Path,
    feedforward: bool,
    u1_amp: float,
    u1_freq: float,
    vbus: float,
    current_ref: float,
) -> dict:
    if duration <= 0 or plant_dt <= 0 or control_dt <= 0 or plant_dt > duration:
        raise ValueError("duration/plant_dt/control_dt must be positive; plant_dt <= duration")
    if control_dt + 1e-15 < plant_dt:
        raise ValueError("control_dt must be >= plant_dt")
    if vbus <= 1.0:
        raise ValueError("vbus must be > 1 V for modulation")

    controller = PIController()
    current = 0.0
    ramped_reference = 0.0
    slew_rate = 20.0
    inductance_h = 0.002
    resistance_ohm = 0.5
    duty_limit = 0.85
    duty = 0.0
    u_i = 0.0
    u1_meas = 0.0
    next_control_t = 0.0

    output.parent.mkdir(parents=True, exist_ok=True)
    error_sq = 0.0
    n_err = 0
    settle_s = 0.15
    control_updates = 0

    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "time_s",
                "reference_a",
                "current_a",
                "u_i",
                "u1_true",
                "u1_meas",
                "duty",
                "feedforward",
                "control_dt",
            ]
        )
        steps = math.ceil(duration / plant_dt)
        for index in range(steps + 1):
            time_s = index * plant_dt
            u1_true = u1_at(time_s, u1_amp, u1_freq)

            if time_s + 1e-15 >= next_control_t:
                max_delta = slew_rate * control_dt
                delta = max(-max_delta, min(max_delta, current_ref - ramped_reference))
                ramped_reference += delta
                u1_meas = u1_true  # sample at control instant only
                u_i = controller.step(ramped_reference, current, control_dt)
                fed = u1_meas if feedforward else 0.0
                duty = (u_i + fed) / vbus
                duty = max(-duty_limit, min(duty_limit, duty))
                control_updates += 1
                next_control_t += control_dt

            di = (duty * vbus - u1_true - resistance_ohm * current) * plant_dt / inductance_h
            current += di
            writer.writerow(
                [
                    f"{time_s:.6f}",
                    f"{ramped_reference:.6f}",
                    f"{current:.6f}",
                    f"{u_i:.6f}",
                    f"{u1_true:.6f}",
                    f"{u1_meas:.6f}",
                    f"{duty:.6f}",
                    "1" if feedforward else "0",
                    f"{control_dt:.6f}",
                ]
            )
            if time_s >= settle_s:
                err = current - current_ref
                error_sq += err * err
                n_err += 1

    rms = math.sqrt(error_sq / n_err) if n_err else float("nan")
    print(
        f"wrote {steps + 1} samples to {output}  ctrl_dt={control_dt:g}s  "
        f"updates={control_updates}  Irms_err={rms:.4f} A  final={current:.3f} A"
    )
    return {
        "rms_error_a": rms,
        "final_current_a": current,
        "samples": steps + 1,
        "control_updates": control_updates,
        "control_dt": control_dt,
    }


def write_svg(fast_csv: Path, slow_csv: Path, svg_path: Path) -> None:
    def load(path: Path):
        with path.open(encoding="utf-8") as handle:
            return list(csv.DictReader(handle))

    fast_rows = load(fast_csv)
    slow_rows = load(slow_csv)
    width, height = 900, 420
    left, right, top, bottom = 60, 20, 30, 40
    plot_w = width - left - right
    plot_h = height - top - bottom
    t_max = float(fast_rows[-1]["time_s"]) or 1.0
    y_min, y_max = -1.5, 2.5

    def xy(t, y):
        x = left + (float(t) / t_max) * plot_w
        yv = top + (1.0 - (float(y) - y_min) / (y_max - y_min)) * plot_h
        return x, yv

    def polyline(rows, key, color, stride=2):
        pts = " ".join(
            f"{xy(r['time_s'], r[key])[0]:.1f},{xy(r['time_s'], r[key])[1]:.1f}"
            for r in rows[::stride]
        )
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.5" points="{pts}" />'

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{left}" y="20" font-size="14" font-family="sans-serif">'
        "Step B: control rate 20 kHz vs 1 kHz (FF ON, U1 sampled at control rate)</text>",
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{height-bottom}" stroke="#333"/>',
        f'<line x1="{left}" y1="{height-bottom}" x2="{width-right}" y2="{height-bottom}" stroke="#333"/>',
        polyline(fast_rows, "reference_a", "#888", 10),
        polyline(fast_rows, "current_a", "#1b6", 10),
        polyline(slow_rows, "current_a", "#c33", 10),
        f'<text x="{width-320}" y="20" font-size="12" fill="#888">ref</text>',
        f'<text x="{width-270}" y="20" font-size="12" fill="#1b6">I @20kHz</text>',
        f'<text x="{width-170}" y="20" font-size="12" fill="#c33">I @1kHz</text>',
        "</svg>",
    ]
    svg_path.write_text("\n".join(parts), encoding="utf-8")
    print(f"wrote {svg_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=0.4)
    parser.add_argument("--plant-dt", type=float, default=0.00005)
    parser.add_argument("--dt", type=float, default=None, help="alias control dt for single run")
    parser.add_argument("--vbus", type=float, default=24.0)
    parser.add_argument("--u1-amp", type=float, default=8.0)
    parser.add_argument("--u1-freq", type=float, default=100.0)
    parser.add_argument("--current-ref", type=float, default=1.0)
    parser.add_argument("--compare", action="store_true", help="A: FF ON vs OFF @ 20 kHz")
    parser.add_argument("--rate-compare", action="store_true", help="B: 20 kHz vs 1 kHz control")
    parser.add_argument("--feedforward", type=int, choices=(0, 1), default=1)
    parser.add_argument("--output", type=Path, default=Path("simulation/output.csv"))
    args = parser.parse_args()

    if args.rate_compare:
        plant_dt = args.plant_dt
        fast = run(
            args.duration, plant_dt, 1.0 / 20000.0,
            Path("simulation/output_rate_20khz.csv"),
            True, args.u1_amp, args.u1_freq, args.vbus, args.current_ref,
        )
        slow = run(
            args.duration, plant_dt, 0.001,
            Path("simulation/output_rate_1khz.csv"),
            True, args.u1_amp, args.u1_freq, args.vbus, args.current_ref,
        )
        write_svg(
            Path("simulation/output_rate_20khz.csv"),
            Path("simulation/output_rate_1khz.csv"),
            Path("simulation/rate_compare.svg"),
        )
        ratio = (
            slow["rms_error_a"] / fast["rms_error_a"]
            if fast["rms_error_a"] > 1e-9
            else float("inf")
        )
        log = Path("simulation/rate_compare_log.txt")
        text = "\n".join(
            [
                "step B control-rate comparison",
                f"Vbus={args.vbus} V  U1={args.u1_amp} Vpk @ {args.u1_freq} Hz  Iref={args.current_ref} A",
                f"plant_dt={plant_dt} s  FF=ON  U1_meas held between control updates",
                f"20 kHz Irms_err={fast['rms_error_a']:.6f} A  updates={fast['control_updates']}",
                f"1 kHz  Irms_err={slow['rms_error_a']:.6f} A  updates={slow['control_updates']}",
                f"1k/20k rms ratio={ratio:.2f} ( >1 means 20 kHz tracks better )",
                "1 kHz must not be used as current-loop sample rate",
                "",
            ]
        )
        log.write_text(text, encoding="utf-8")
        print(text)
        if not (slow["rms_error_a"] > fast["rms_error_a"] * 1.5):
            raise SystemExit("expected 20 kHz control to beat 1 kHz under U1 disturbance")
        return

    if args.compare:
        plant_dt = args.plant_dt
        ctrl = 1.0 / 20000.0
        on_stats = run(
            args.duration, plant_dt, ctrl, Path("simulation/output_ff_on.csv"),
            True, args.u1_amp, args.u1_freq, args.vbus, args.current_ref,
        )
        off_stats = run(
            args.duration, plant_dt, ctrl, Path("simulation/output_ff_off.csv"),
            False, args.u1_amp, args.u1_freq, args.vbus, args.current_ref,
        )
        # keep A SVG helper lightweight: reuse rate writer with ON as fast coloring
        write_svg(
            Path("simulation/output_ff_on.csv"),
            Path("simulation/output_ff_off.csv"),
            Path("simulation/ff_compare.svg"),
        )
        ratio = (
            off_stats["rms_error_a"] / on_stats["rms_error_a"]
            if on_stats["rms_error_a"] > 1e-9
            else float("inf")
        )
        Path("simulation/ff_compare_log.txt").write_text(
            "\n".join(
                [
                    "step A feedforward comparison (@20 kHz control)",
                    f"Vbus={args.vbus} V  U1={args.u1_amp} Vpk @ {args.u1_freq} Hz  Iref={args.current_ref} A",
                    f"FF ON  Irms_err={on_stats['rms_error_a']:.6f} A  final={on_stats['final_current_a']:.4f} A",
                    f"FF OFF Irms_err={off_stats['rms_error_a']:.6f} A  final={off_stats['final_current_a']:.4f} A",
                    f"OFF/ON rms ratio={ratio:.2f}",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        print(Path("simulation/ff_compare_log.txt").read_text(encoding="utf-8"))
        if not (off_stats["rms_error_a"] > on_stats["rms_error_a"] * 1.5):
            raise SystemExit("expected FF ON to reduce RMS current error vs OFF")
        return

    control_dt = args.dt if args.dt is not None else args.plant_dt
    run(
        args.duration,
        args.plant_dt,
        control_dt,
        args.output,
        bool(args.feedforward),
        args.u1_amp,
        args.u1_freq,
        args.vbus,
        args.current_ref,
    )


if __name__ == "__main__":
    main()
