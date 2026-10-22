#!/usr/bin/env python3
"""Agent-friendly tuner for the averaged converter twin.

Stdout is one JSON object unless --human is set. The tool does not flash
firmware, open a serial port, or command a power stage.
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from simulation.twin.loop import LoopConfig, LoopResult, run_current_loop  # noqa: E402
from simulation.twin.plotting import write_series_svg  # noqa: E402
from simulation.twin.pll import run_pll  # noqa: E402
from simulation.twin.protection import FAULT_BITS, fault_names  # noqa: E402
from simulation.twin.tuning import suggest_current_gains  # noqa: E402

SCHEMA_VERSION = 1
MAX_GRID = 8
MAX_COMBINATIONS = 32


def _grid(text: str) -> list[float]:
    values = [float(part) for part in text.split(",") if part.strip()]
    if not values:
        raise argparse.ArgumentTypeError("grid must contain at least one number")
    if len(values) > MAX_GRID:
        raise argparse.ArgumentTypeError(f"grid is limited to {MAX_GRID} values")
    return values


def _emit(payload: dict, human: bool) -> None:
    if human:
        print(_human(payload))
        return
    print(json.dumps(payload, indent=2))


def _human(payload: dict) -> str:
    lines = [f"{payload.get('command', 'result')}: ok={payload.get('ok')}"]
    metrics = payload.get("metrics") or payload.get("evaluation", {}).get("metrics")
    if isinstance(metrics, dict):
        lines.append(
            "stable={stable} settling_s={settling_time_s} error_a={steady_state_error_a} "
            "overshoot_pct={overshoot_pct} faults={final_faults}".format(**metrics)
        )
    if "best" in payload:
        best = payload["best"]
        lines.append(
            f"best kp={best.get('kp')} ki={best.get('ki')} stable={best.get('stable')} "
            f"error_a={best.get('steady_state_error_a')}"
        )
    if "mean_frequency_hz" in payload:
        lines.append(
            f"locked={payload['locked']} mean_hz={payload['mean_frequency_hz']:.4f} "
            f"final_vq={payload['final_vq']:.4f}"
        )
    artifacts = payload.get("artifacts") or {}
    for name, location in artifacts.items():
        if location:
            lines.append(f"{name}: {location}")
    return "\n".join(lines)


def _metrics_dict(result: LoopResult) -> dict:
    metrics = result.metrics
    return {
        "overshoot_pct": metrics.overshoot_pct,
        "settling_time_s": metrics.settling_time_s,
        "steady_state_error_a": metrics.steady_state_error_a,
        "peak_abs_duty": metrics.peak_abs_duty,
        "final_current_a": metrics.final_current_a,
        "final_faults": metrics.final_faults,
        "fault_names": fault_names(metrics.final_faults),
        "stable": metrics.stable,
        "target_a": result.target_a,
    }


def _write_loop_artifacts(
    result: LoopResult,
    output_dir: Path,
    stem: str,
    *,
    write_plot: bool = True,
) -> dict[str, str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / f"{stem}.csv"
    plot_path = output_dir / f"{stem}.svg"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["time_s", "reference_a", "current_a", "duty", "faults"])
        for sample in result.samples:
            writer.writerow([
                f"{sample.time_s:.6f}",
                f"{sample.reference_a:.6f}",
                f"{sample.current_a:.6f}",
                f"{sample.duty:.6f}",
                sample.faults,
            ])
    if write_plot:
        write_series_svg(
            plot_path,
            [sample.time_s for sample in result.samples],
            {
                "reference_a": [sample.reference_a for sample in result.samples],
                "current_a": [sample.current_a for sample in result.samples],
                "duty": [sample.duty for sample in result.samples],
            },
            title="Current-loop digital twin",
            y_label="amperes and duty",
        )
    return {
        "csv": str(csv_path.resolve()),
        "plot": str(plot_path.resolve()) if write_plot else "",
    }


def _config_from_args(args: argparse.Namespace, kp: float | None = None, ki: float | None = None) -> LoopConfig:
    return LoopConfig(
        kp=args.kp if kp is None else kp,
        ki=args.ki if ki is None else ki,
        kd=args.kd,
        duty_limit=args.duty_limit,
        integrator_limit=args.integrator_limit,
        current_limit_a=args.current_limit,
        current_trip_a=args.current_trip,
        slew_a_per_s=args.slew,
        plant_gain=args.plant_gain,
        plant_tau_s=args.plant_tau,
        bus_voltage_v=args.bus_voltage,
        temperature_c=args.temperature,
        reference_a=args.reference,
        duration_s=args.duration,
        dt_s=args.dt,
    )


def _add_plant_args(parser: argparse.ArgumentParser, *, duration: float, include_pi: bool = True) -> None:
    if include_pi:
        parser.add_argument("--kp", type=float, default=0.08)
        parser.add_argument("--ki", type=float, default=8.0)
    parser.add_argument("--kd", type=float, default=0.0)
    parser.add_argument("--duty-limit", type=float, default=0.85)
    parser.add_argument("--integrator-limit", type=float, default=0.75)
    parser.add_argument("--current-limit", type=float, default=2.0)
    parser.add_argument("--current-trip", type=float, default=2.5)
    parser.add_argument("--slew", type=float, default=5.0)
    parser.add_argument("--plant-gain", type=float, default=2.0)
    parser.add_argument("--plant-tau", type=float, default=0.02)
    parser.add_argument("--bus-voltage", type=float, default=12.0)
    parser.add_argument("--temperature", type=float, default=25.0)
    parser.add_argument("--reference", type=float, default=1.0)
    parser.add_argument("--duration", type=float, default=duration)
    parser.add_argument("--dt", type=float, default=0.001)


def _parameters(config: LoopConfig) -> dict[str, float]:
    return {
        "kp": config.kp,
        "ki": config.ki,
        "kd": config.kd,
        "duty_limit": config.duty_limit,
        "integrator_limit": config.integrator_limit,
        "current_limit_a": config.current_limit_a,
        "current_trip_a": config.current_trip_a,
        "slew_a_per_s": config.slew_a_per_s,
        "plant_gain": config.plant_gain,
        "plant_tau_s": config.plant_tau_s,
        "reference_a": config.reference_a,
        "duration_s": config.duration_s,
        "dt_s": config.dt_s,
    }


def command_run(args: argparse.Namespace) -> int:
    config = _config_from_args(args)
    result = run_current_loop(config)
    artifacts = _write_loop_artifacts(
        result,
        args.output_dir,
        "current_loop",
        write_plot=not args.no_plot,
    )
    stable = result.metrics.stable
    payload = {
        "schema_version": SCHEMA_VERSION,
        "ok": stable if args.require_stable else True,
        "command": "run",
        "parameters": _parameters(config),
        "metrics": _metrics_dict(result),
        "artifacts": artifacts,
    }
    _emit(payload, args.human)
    if args.require_stable and not stable:
        return 1
    return 0


def _row_key(row: dict) -> tuple:
    error = row["steady_state_error_a"]
    overshoot = row["overshoot_pct"]
    return (
        0 if row["stable"] else 1,
        abs(error) if error is not None else 1e9,
        overshoot if overshoot is not None else 1e9,
    )


def command_sweep(args: argparse.Namespace) -> int:
    if len(args.kp) * len(args.ki) > MAX_COMBINATIONS:
        raise ValueError(f"sweep is limited to {MAX_COMBINATIONS} combinations")
    rows = []
    for kp in args.kp:
        for ki in args.ki:
            config = _config_from_args(args, kp=kp, ki=ki)
            result = run_current_loop(config)
            metrics = _metrics_dict(result)
            rows.append({"kp": kp, "ki": ki, **metrics})
    best = min(rows, key=_row_key)
    payload = {
        "schema_version": SCHEMA_VERSION,
        "ok": True,
        "command": "sweep",
        "count": len(rows),
        "best": best,
        "rows": rows,
    }
    _emit(payload, args.human)
    return 0


def command_suggest(args: argparse.Namespace) -> int:
    suggestion = suggest_current_gains(args.plant_gain, args.plant_tau, args.dt, args.crossover_hz)
    config = _config_from_args(args, kp=suggestion["kp"], ki=suggestion["ki"])
    result = run_current_loop(config)
    artifacts = _write_loop_artifacts(result, args.output_dir, "suggested_loop")
    stable = result.metrics.stable
    payload = {
        "schema_version": SCHEMA_VERSION,
        "ok": stable if args.require_stable else True,
        "command": "suggest",
        "kp": suggestion["kp"],
        "ki": suggestion["ki"],
        "kd": 0.0,
        "crossover_hz": suggestion["crossover_hz"],
        "plant_pole_hz": suggestion["plant_pole_hz"],
        "rationale": (
            "Cancel the averaged plant pole and place the crossover below "
            "both that pole and the simulation Nyquist frequency."
        ),
        "parameters": _parameters(config),
        "metrics": _metrics_dict(result),
        "artifacts": artifacts,
    }
    _emit(payload, args.human)
    if args.require_stable and not stable:
        return 1
    return 0


def command_pll(args: argparse.Namespace) -> int:
    result = run_pll(frequency_hz=args.frequency, duration_s=args.duration, dt_s=args.dt)
    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "pll.csv"
    plot_path = output_dir / "pll.svg"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["time_s", "frequency_hz", "vq"])
        for time_s, frequency, vq in zip(result.time_s, result.frequency_hz, result.vq):
            writer.writerow([f"{time_s:.6f}", f"{frequency:.6f}", f"{vq:.6f}"])
    write_series_svg(
        plot_path,
        list(result.time_s),
        {"frequency_hz": list(result.frequency_hz), "vq": list(result.vq)},
        title="SOGI-PLL digital twin",
        y_label="hertz and quadrature",
    )
    payload = {
        "schema_version": SCHEMA_VERSION,
        "ok": result.locked,
        "command": "pll",
        "input_frequency_hz": args.frequency,
        "locked": result.locked,
        "mean_frequency_hz": result.mean_frequency_hz,
        "final_vq": result.final_vq,
        "artifacts": {"csv": str(csv_path.resolve()), "plot": str(plot_path.resolve())},
    }
    _emit(payload, args.human)
    return 0 if result.locked else 1


def command_schema(_args: argparse.Namespace) -> int:
    payload = {
        "schema_version": SCHEMA_VERSION,
        "ok": True,
        "command": "schema",
        "commands": ["run", "sweep", "suggest", "pll", "schema"],
        "stdout": "One JSON object. Pass --human for a short text summary.",
        "safety": "Host twin only. It does not arm hardware or write controller flash.",
        "fault_bits": FAULT_BITS,
        "parameters": [
            {"name": "kp", "unit": "duty per ampere", "minimum": 0, "default": 0.08},
            {"name": "ki", "unit": "duty per ampere-second", "minimum": 0, "default": 8.0},
            {"name": "kd", "unit": "duty per ampere per second", "minimum": 0, "default": 0.0},
            {"name": "duty_limit", "unit": "1", "minimum_exclusive": 0, "maximum": 1, "default": 0.85},
            {"name": "current_limit_a", "unit": "A", "default": 2.0},
            {"name": "current_trip_a", "unit": "A", "default": 2.5},
            {"name": "slew_a_per_s", "unit": "A/s", "default": 5.0},
            {"name": "plant_gain", "unit": "A per duty", "default": 2.0},
            {"name": "plant_tau_s", "unit": "s", "default": 0.02},
            {"name": "reference_a", "unit": "A", "default": 1.0},
            {"name": "duration_s", "unit": "s", "default": 0.5},
            {"name": "dt_s", "unit": "s", "default": 0.001},
        ],
    }
    _emit(payload, False)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    shared = argparse.ArgumentParser(add_help=False)
    shared.add_argument("--json", action="store_true", help="print one JSON object (this is the default)")
    shared.add_argument("--human", action="store_true", help="print a short text summary instead of JSON")
    sub = parser.add_subparsers(dest="command", required=True)

    run = sub.add_parser("run", parents=[shared], help="simulate one current-loop parameter set")
    _add_plant_args(run, duration=0.5)
    run.add_argument("--output-dir", type=Path, default=Path("simulation/runs/latest"))
    run.add_argument("--require-stable", action="store_true")
    run.add_argument("--no-plot", action="store_true", help="still write the CSV, omit the SVG")
    run.set_defaults(func=command_run)

    sweep = sub.add_parser("sweep", parents=[shared], help="rank a small kp/ki grid")
    _add_plant_args(sweep, duration=0.4, include_pi=False)
    sweep.add_argument("--kp", type=_grid, default=[0.04, 0.08, 0.16])
    sweep.add_argument("--ki", type=_grid, default=[4.0, 8.0, 16.0])
    sweep.set_defaults(func=command_sweep)

    suggest = sub.add_parser("suggest", parents=[shared], help="propose PI gains and evaluate them")
    _add_plant_args(suggest, duration=0.6, include_pi=False)
    suggest.add_argument("--crossover-hz", type=float, default=None)
    suggest.add_argument("--output-dir", type=Path, default=Path("simulation/runs/latest"))
    suggest.add_argument("--require-stable", action="store_true")
    suggest.set_defaults(func=command_suggest)

    pll = sub.add_parser("pll", parents=[shared], help="simulate the single-phase SOGI-PLL")
    pll.add_argument("--frequency", type=float, default=50.0)
    pll.add_argument("--duration", type=float, default=0.4)
    pll.add_argument("--dt", type=float, default=1.0e-4)
    pll.add_argument("--output-dir", type=Path, default=Path("simulation/runs/latest"))
    pll.set_defaults(func=command_pll)

    schema = sub.add_parser("schema", parents=[shared], help="print the machine-readable parameter contract")
    schema.set_defaults(func=command_schema)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except ValueError as exc:
        payload = {
            "schema_version": SCHEMA_VERSION,
            "ok": False,
            "command": args.command,
            "error": str(exc),
        }
        _emit(payload, getattr(args, "human", False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
