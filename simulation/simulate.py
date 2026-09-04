#!/usr/bin/env python3
"""Averaged current-loop sim with plant-side U1 feedforward.

Educational host model only. Not a switching model and not tuned hardware
parameters.

Duty formula (step A):
    duty = (u_i + U1) / Vbus   if feedforward enabled (default)
    duty = u_i / Vbus          if feedforward disabled
Plant:
    L * di/dt = duty * Vbus - U1 - R * i
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
    dt: float,
    output: Path,
    feedforward: bool,
    u1_amp: float,
    u1_freq: float,
    vbus: float,
    current_ref: float,
) -> dict:
    if duration <= 0 or dt <= 0 or dt > duration:
        raise ValueError("duration and dt must be positive, and dt <= duration")
    if vbus <= 1.0:
        raise ValueError("vbus must be > 1 V for modulation")

    controller = PIController()
    current = 0.0
    ramped_reference = 0.0
    slew_rate = 20.0
    inductance_h = 0.002
    resistance_ohm = 0.5
    duty_limit = 0.85

    output.parent.mkdir(parents=True, exist_ok=True)
    error_sq = 0.0
    n_err = 0
    settle_s = 0.15

    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            ["time_s", "reference_a", "current_a", "u_i", "u1", "duty", "feedforward"]
        )
        steps = math.ceil(duration / dt)
        for index in range(steps + 1):
            time_s = index * dt
            max_delta = slew_rate * dt
            delta = max(-max_delta, min(max_delta, current_ref - ramped_reference))
            ramped_reference += delta
            u1 = u1_at(time_s, u1_amp, u1_freq)
            u_i = controller.step(ramped_reference, current, dt)
            fed = u1 if feedforward else 0.0
            duty = (u_i + fed) / vbus
            duty = max(-duty_limit, min(duty_limit, duty))
            di = (duty * vbus - u1 - resistance_ohm * current) * dt / inductance_h
            current += di
            writer.writerow(
                [
                    f"{time_s:.6f}",
                    f"{ramped_reference:.6f}",
                    f"{current:.6f}",
                    f"{u_i:.6f}",
                    f"{u1:.6f}",
                    f"{duty:.6f}",
                    "1" if feedforward else "0",
                ]
            )
            if time_s >= settle_s:
                err = current - current_ref
                error_sq += err * err
                n_err += 1

    rms = math.sqrt(error_sq / n_err) if n_err else float("nan")
    print(
        f"wrote {steps + 1} samples to {output}  ff={'ON' if feedforward else 'OFF'}  "
        f"Irms_err(after {settle_s}s)={rms:.4f} A  final={current:.3f} A"
    )
    return {"rms_error_a": rms, "final_current_a": current, "samples": steps + 1}


def write_svg(on_csv: Path, off_csv: Path, svg_path: Path) -> None:
    def load(path: Path):
        rows = []
        with path.open(encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            for row in reader:
                rows.append(row)
        return rows

    on_rows = load(on_csv)
    off_rows = load(off_csv)
    width, height = 900, 420
    left, right, top, bottom = 60, 20, 30, 40
    plot_w = width - left - right
    plot_h = height - top - bottom
    t_max = float(on_rows[-1]["time_s"]) or 1.0
    y_min, y_max = -1.5, 2.5

    def xy(t, y):
        x = left + (float(t) / t_max) * plot_w
        yv = top + (1.0 - (float(y) - y_min) / (y_max - y_min)) * plot_h
        return x, yv

    def polyline(rows, key, color):
        pts = " ".join(f"{xy(r['time_s'], r[key])[0]:.1f},{xy(r['time_s'], r[key])[1]:.1f}" for r in rows[::2])
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.5" points="{pts}" />'

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{left}" y="20" font-size="14" font-family="sans-serif">'
        "Step A: current tracking with 100 Hz U1 disturbance</text>",
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{height-bottom}" stroke="#333"/>',
        f'<line x1="{left}" y1="{height-bottom}" x2="{width-right}" y2="{height-bottom}" stroke="#333"/>',
        polyline(on_rows, "reference_a", "#888"),
        polyline(on_rows, "current_a", "#1b6"),
        polyline(off_rows, "current_a", "#c33"),
        f'<text x="{width-280}" y="20" font-size="12" fill="#888">ref</text>',
        f'<text x="{width-230}" y="20" font-size="12" fill="#1b6">I FF ON</text>',
        f'<text x="{width-150}" y="20" font-size="12" fill="#c33">I FF OFF</text>',
        f'<text x="{width/2}" y="{height-10}" font-size="12" font-family="sans-serif">time (s)</text>',
        f'<text x="12" y="{height/2}" font-size="12" font-family="sans-serif" transform="rotate(-90 12 {height/2})">A</text>',
        "</svg>",
    ]
    svg_path.write_text("\n".join(parts), encoding="utf-8")
    print(f"wrote {svg_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=0.4)
    parser.add_argument("--dt", type=float, default=0.0002)
    parser.add_argument("--vbus", type=float, default=24.0)
    parser.add_argument("--u1-amp", type=float, default=8.0)
    parser.add_argument("--u1-freq", type=float, default=100.0)
    parser.add_argument("--current-ref", type=float, default=1.0)
    parser.add_argument("--compare", action="store_true", help="run FF ON vs OFF and write SVG")
    parser.add_argument("--feedforward", type=int, choices=(0, 1), default=1)
    parser.add_argument("--output", type=Path, default=Path("simulation/output.csv"))
    args = parser.parse_args()

    if args.compare:
        on_path = Path("simulation/output_ff_on.csv")
        off_path = Path("simulation/output_ff_off.csv")
        on_stats = run(
            args.duration, args.dt, on_path, True, args.u1_amp, args.u1_freq, args.vbus, args.current_ref
        )
        off_stats = run(
            args.duration, args.dt, off_path, False, args.u1_amp, args.u1_freq, args.vbus, args.current_ref
        )
        write_svg(on_path, off_path, Path("simulation/ff_compare.svg"))
        log_path = Path("simulation/ff_compare.log")
        ratio = (
            off_stats["rms_error_a"] / on_stats["rms_error_a"]
            if on_stats["rms_error_a"] > 1e-9
            else float("inf")
        )
        log_path.write_text(
            "\n".join(
                [
                    "step A feedforward comparison",
                    f"Vbus={args.vbus} V  U1={args.u1_amp} Vpk @ {args.u1_freq} Hz  Iref={args.current_ref} A",
                    f"FF ON  Irms_err={on_stats['rms_error_a']:.6f} A  final={on_stats['final_current_a']:.4f} A",
                    f"FF OFF Irms_err={off_stats['rms_error_a']:.6f} A  final={off_stats['final_current_a']:.4f} A",
                    f"OFF/ON rms ratio={ratio:.2f} ( >1 means ON tracks better )",
                    "duty = (u_i + U1)/Vbus when ON; duty = u_i/Vbus when OFF",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        print(log_path.read_text(encoding="utf-8"))
        if not (off_stats["rms_error_a"] > on_stats["rms_error_a"] * 1.5):
            raise SystemExit("expected FF ON to reduce RMS current error vs OFF")
        return

    run(
        args.duration,
        args.dt,
        args.output,
        bool(args.feedforward),
        args.u1_amp,
        args.u1_freq,
        args.vbus,
        args.current_ref,
    )


if __name__ == "__main__":
    main()
